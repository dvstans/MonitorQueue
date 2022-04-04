#include <stdexcept>
#include <algorithm>
#include "Queue.hpp"

using namespace std;

//================================= PUBLIC METHODS ============================

Queue::Queue( uint8_t a_priorities, size_t a_capacity, size_t a_poll_interval, size_t a_fail_timeout ) :
    m_capacity(a_capacity),
    m_count_queued(0),
    m_count_failed(0),
    m_rng(chrono::steady_clock::now().time_since_epoch().count()),
    m_poll_interval( a_poll_interval ),
    m_fail_timeout( a_fail_timeout ),
    m_max_retries( 10 ), // TODO make config
    m_boost_timeout( 1000 ), // TODO make config
    m_monitor_thread( &Queue::monitorThread, this ),
    m_delay_thread( &Queue::delayThread, this ),
    m_run( true ),
    m_err_cb( 0 )
{
    m_queue_list.resize( a_priorities );
}

Queue::~Queue() {
    m_run = false;
    m_mon_cv.notify_one();
    m_delay_cv.notify_one();
    m_monitor_thread.join();
    m_delay_thread.join();

    // TODO - Need to free memory
}

void
Queue::push( const std::string & a_id, const std::string & a_data, uint8_t a_priority ) {
    // Verify priority
    if ( a_priority >= m_queue_list.size() ) {
        throw runtime_error( "Invalid queue priority" );
    }

    lock_guard<mutex> lock(m_mutex);

    // Check for duplicate messages
    if ( m_msg_map.find( a_id ) != m_msg_map.end() ) {
        throw runtime_error( "Duplicate message ID" );
    }

    // Make sure capacity isn't exceeded
    if ( m_msg_map.size() == m_capacity ) {
        throw length_error( "Queue capacity exceeded" );
    }

    MsgEntry_t * msg = getMsgEntry( a_id, a_data, a_priority );

    m_msg_map[a_id] = msg;
    m_queue_list[a_priority].push_front( msg );

    m_count_queued++;

    m_cv.notify_one();
}

const Queue::Msg_t &
Queue::pop() {
    unique_lock<mutex> lock(m_mutex);

    return popImpl( lock );
}

void
Queue::ack( const std::string & a_id, uint64_t a_token, bool a_requeue, size_t a_requeue_delay ) {
    unique_lock<mutex> lock(m_mutex);

    ackImpl( a_id, a_token, a_requeue, a_requeue_delay );
}


const Queue::Msg_t &
Queue::popAck( const std::string & a_id, uint64_t a_token, bool a_requeue, size_t a_requeue_delay ) {
    unique_lock<mutex> lock(m_mutex);

    ackImpl( a_id, a_token, a_requeue, a_requeue_delay );

    return popImpl( lock );
}

size_t
Queue::count() {
    //lock_guard<mutex> lock(m_mutex);
    // TODO need locks?
    return m_msg_map.size();
}

size_t
Queue::freeCount() {
    // TODO need to use atomic int?
    return m_capacity - m_msg_map.size();
}

size_t
Queue::workingCount() {
    // TODO need to use atomic int?
    return m_msg_map.size() - m_count_failed;
}

size_t
Queue::failedCount() {
    // TODO need to use atomic int?
    return m_count_failed;
}

Queue::MsgIdList_t
Queue::getFailed() {
    MsgIdList_t failed;
    failed.reserve( m_count_failed );

    lock_guard<mutex> lock(m_mutex);

    for ( msg_map_t::iterator m = m_msg_map.begin(); m != m_msg_map.end(); m++ ) {
        if ( m->second->state == MSG_FAILED ) {
            failed.push_back( m->first );
        }
    }

    return failed;
}

Queue::MsgIdList_t
Queue::eraseFailed( const MsgIdList_t & a_msg_ids ) {
    MsgIdList_t failed;
    failed.reserve( m_count_failed );

    lock_guard<mutex> lock(m_mutex);
    msg_map_t::iterator m;

    for ( MsgIdList_t::const_iterator i = a_msg_ids.begin(); i != a_msg_ids.end(); i++ ) {
        m = m_msg_map.find( *i );
        if ( m != m_msg_map.end() ) {
            if ( m->second->state == MSG_FAILED ) {
                failed.push_back( m->first );
                m_msg_pool.push_back( m->second );
                m_msg_map.erase( m );
            }
        }
    }

    return failed;
}

void
Queue::setErrorCallback( ErrorCB_t * a_callback ) {
    m_err_cb = a_callback;
}


//================================= PRIVATE METHODS ===========================

Queue::MsgEntry_t *
Queue::getMsgEntry( const string & a_id, const string & a_data, uint8_t a_priority ) {
    MsgEntry_t * msg;

    if ( !m_msg_pool.size() ) {
        msg = new MsgEntry_t( a_id, a_data, a_priority );
    } else {
        msg = m_msg_pool.back();
        msg->reset( a_id, a_data, a_priority );
        m_msg_pool.pop_back();
    }

    return msg;
}


const Queue::Msg_t &
Queue::popImpl( unique_lock<mutex> & a_lock ) {
    // Lock must be held before calling

    while ( !m_count_queued ) {
        m_cv.wait( a_lock );
    }

    MsgEntry_t * entry = 0;

    for ( queue_list_t::iterator q = m_queue_list.begin(); q != m_queue_list.end(); ++q ){
        if ( !q->empty() ){
            entry = q->back();
            q->pop_back();
            break;
        }
    }

    if ( !entry ) {
        throw logic_error( "All queues empty when m_count_queued > 0" );
    }

    entry->state = MSG_RUNNING;
    entry->state_ts = std::chrono::system_clock::now();
    entry->message.token = m_rng();
    m_count_queued--;

    return entry->message;
}


void
Queue::ackImpl( const std::string & a_id, uint64_t a_token, bool a_requeue, size_t a_requeue_delay ) {
    // Lock must be held before calling

    msg_map_t::iterator e = m_msg_map.find( a_id );
    if ( e == m_msg_map.end() ) {
        throw runtime_error( "No message found matching ID" );
    }

    if ( e->second->message.token != a_token ) {
        throw runtime_error( "Invalid message token" );
    }

    if ( e->second->state != MSG_RUNNING ) {
        throw runtime_error( "Invalid message state" );
    }

    if ( a_requeue ) {
        timestamp_t now = std::chrono::system_clock::now();

        e->second->boosted = false;
        e->second->message.token = 0;

        if ( a_requeue_delay ) {
            insertDelayedMsg( e->second, now + std::chrono::milliseconds( a_requeue_delay ));
        } else {
            m_count_queued++;
            e->second->state = MSG_QUEUED;
            e->second->state_ts = now;
            m_queue_list[e->second->priority].push_front( e->second );
            m_cv.notify_one();
        }

    } else {
        m_msg_pool.push_back( e->second );
        m_msg_map.erase( e );
    }
}

void
Queue::insertDelayedMsg( MsgEntry_t * a_msg, const timestamp_t & a_requeue_ts ) {
    // Lock must be held before calling

    a_msg->state = MSG_DELAYED;
    a_msg->state_ts = a_requeue_ts;

    m_msg_delay.insert( a_msg );

    if ( *m_msg_delay.begin() == a_msg ) {
        m_delay_cv.notify_one();
    }
}

void
Queue::monitorThread() {
    auto poll_ms = chrono::milliseconds( m_poll_interval );
    timestamp_t now, fail_time, boost_time;
    msg_map_t::iterator m;
    deque<MsgEntry_t*>::iterator q;
    size_t notify;

    unique_lock<mutex> lock( m_mutex );

    while ( m_run ) {
        m_mon_cv.wait_for( lock, poll_ms );

        if ( !m_run ){
            return;
        }

        if ( m_err_cb ) {
            (*m_err_cb)( string("free: ") + to_string( freeCount()) + ", delayed: " + to_string( m_msg_delay.size() ));
        }

        now = std::chrono::system_clock::now();
        fail_time = now - std::chrono::milliseconds(m_fail_timeout);
        boost_time = now - std::chrono::milliseconds(m_boost_timeout);
        notify = 0;

        // Scan all messages for timeout on running state
        // and starving low-priority messages

        for ( m = m_msg_map.begin(); m != m_msg_map.end(); m++ ) {
            if ( m->second->state == MSG_RUNNING ) {
                if ( m->second->state_ts < fail_time ) {
                    if ( ++m->second->fail_count == m_max_retries ) {
                        // Fail message
                        m->second->state = MSG_FAILED;
                        m_count_failed++;
                        if ( m_err_cb ) {
                            (*m_err_cb)( string("FAIL MSG ID ") + m->first );
                        }
                    } else {
                        // Retry message
                        m->second->state = MSG_QUEUED;
                        m->second->message.token = 0;
                        m_queue_list[m->second->priority].push_front( m->second );
                        m_count_queued++;
                        notify++;

                        if ( m_err_cb ) {
                            (*m_err_cb)( string("RETRY MSG ID ") + m->first );
                        }
                    }
                }
            } else if ( m->second->state == MSG_QUEUED ) {
                if ( m->second->priority > 0 && !m->second->boosted && m->second->state_ts < boost_time ) {
                    // Find message in current queue
                    q = std::find( m_queue_list[m->second->priority].begin(), m_queue_list[m->second->priority].end(), m->second );
                    if ( q != m_queue_list[m->second->priority].end() ) {
                        if ( m_err_cb ) {
                            (*m_err_cb)( string("PRIORITY BOOST MSG ID ") + m->first );
                        }
                        m->second->boosted = true;
                        // Remove entry from current queue
                        m_queue_list[m->second->priority].erase( q );
                        // Push to front of high priority queue
                        m_queue_list[0].push_front( m->second );
                    } else {
                        if ( m_err_cb ) {
                            (*m_err_cb)( "Message entry not found in expected queue\n" );
                        }
                    }
                }
            }
        }

        if ( notify == 1 ) {
            m_cv.notify_one();
        } else if ( notify > 1 ) {
            m_cv.notify_all();
        }
    }
}

void
Queue::delayThread() {
    timestamp_t now;
    msg_delay_t::iterator m;

    unique_lock<mutex> lock( m_mutex );

    while ( m_run ) {
        // Calc delay time and (maybe) go to sleep
        if ( m_msg_delay.size() ) {
            now = std::chrono::system_clock::now();
            m = m_msg_delay.begin();
            if ( (*m)->state_ts > now ) {
                if ( m_err_cb ) {
                    (*m_err_cb)( "Waiting w/ timeout" );
                }

                m_delay_cv.wait_for( lock, (*m)->state_ts - now );
            }
        } else {
            if ( m_err_cb ) {
                (*m_err_cb)( "Waiting w/o timeout" );
            }
            m_delay_cv.wait( lock );
        }

        if ( !m_run ){
            return;
        }

        // See if one or messages are ready to queue
        if ( m_msg_delay.size() ) {
            now = std::chrono::system_clock::now();

            while ( m_msg_delay.size() ) {
                m = m_msg_delay.begin();

                if ( (*m)->state_ts <= now ) {
                    if ( m_err_cb ) {
                        (*m_err_cb)( string("Queuing delayed msg ID ") + (*m)->message.id );
                    }

                    // Msg is ready, push to queue
                    (*m)->state = MSG_QUEUED;
                    (*m)->state_ts = now;
                    m_queue_list[(*m)->priority].push_back( *m );
                    m_count_queued++;
                    m_cv.notify_one();

                    // Remove from delay set
                    m_msg_delay.erase( m );
                } else {
                    break;
                }
            }
        }
    }
}
