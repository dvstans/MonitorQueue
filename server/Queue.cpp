#include <stdexcept>
#include <iostream>
#include "Queue.hpp"

using namespace std;

#define MAX_FAIL 10 // TODO put in config object

Queue::Queue( uint8_t a_priorities, size_t a_capacity ) :
    m_capacity(a_capacity), m_count_queued(0)
{
    m_queues.resize( a_priorities );
}

Queue::~Queue() {
}

void
Queue::push( const std::string & a_id, const std::string & a_data, uint8_t a_priority ) {
    // Verify priority
    if ( a_priority >= m_queues.size() ) {
        throw runtime_error( "Invalid queue priority" );
    }

    lock_guard<mutex> lock(m_mutex);

    // Check for duplicate messages
    if ( m_messages.find( a_id ) != m_messages.end() ) {
        throw runtime_error( "Duplicate message ID" );
    }

    // Make sure capacity isn't exceeded
    if ( m_messages.size() == m_capacity ) {
        throw length_error( "Queue capacity exceeded" );
    }

    MsgEntry_t * entry = new MsgEntry_t {
        a_priority,
        0,
        false,
        std::chrono::system_clock::now(),
        timestamp_t(),
        make_shared<Msg_t>(Msg_t{ a_id, a_data })
    };

    m_messages[a_id] = entry;
    m_queues[a_priority].push_front( entry );

    m_count_queued++;

    m_cv.notify_one();
}

Queue::pMsg_t
Queue::popImpl( unique_lock<mutex> & a_lock ) {
    // Lock must be held before calling

    // m_msg_count is NOT the count of messages in the queue
    while ( !m_count_queued ) {
        m_cv.wait( a_lock );
    }

    MsgEntry_t * entry = 0;

    for ( queue_store_t::iterator q = m_queues.begin(); q != m_queues.end(); ++q ){
        if ( !q->empty() ){
            entry = q->back();
            q->pop_back();
            break;
        }
    }

    if ( !entry ) {
        std::cout << "m_count_queued: " << m_count_queued << std::endl;
        throw logic_error( "All queues empty when m_count_queued > 0" );
    }

    entry->active = true;
    entry->ts_active = std::chrono::system_clock::now();
    pMsg_t msg = entry->message;

    m_count_queued--;

    return msg;
}

Queue::pMsg_t
Queue::pop() {
    unique_lock<mutex> lock(m_mutex);

    return popImpl( lock );
}

void
Queue::ack( const std::string & a_id, MsgState_t a_state ) {
    unique_lock<mutex> lock(m_mutex);

    msg_state_t::iterator e = m_messages.find( a_id );
    if ( e == m_messages.end() ) {
        throw runtime_error( "No message found matching ID" );
    }

    switch ( a_state ) {
        case MSG_DONE:
            m_messages.erase( e );
            break;
        case MSG_CONT:
            e->second->active = false;
            m_queues[e->second->priority].push_back( e->second );
            m_count_queued++;
            m_cv.notify_one();
            break;
        case MSG_FAIL:
            // This state is for retrying transient failures reported by workers
            // Permanent failures will be stopped with the MSG_DONE state

            if ( e->second->fail_count++ > MAX_FAIL ) {
                // If too many failures, do not reschedule but keep in state to allow
                // external monitor process to decide what to do
                e->second->active = false;
            }

            break;
    }
}


Queue::pMsg_t
Queue::popAck( const std::string & a_id, MsgState_t a_state ) {
    unique_lock<mutex> lock(m_mutex);

    msg_state_t::iterator e = m_messages.find( a_id );
    if ( e == m_messages.end() ) {
        throw runtime_error( "No message found matching ID" );
    }

    switch ( a_state ) {
        case MSG_DONE:
            m_messages.erase( e );
            break;
        case MSG_CONT:
            e->second->active = false;
            m_queues[e->second->priority].push_back( e->second );
            m_count_queued++;
            m_cv.notify_one();
            break;
        case MSG_FAIL:
            // This state is for retrying transient failures reported by workers
            // Permanent failures will be stopped with the MSG_DONE state

            if ( e->second->fail_count++ > MAX_FAIL ) {
                // If too many failures, do not reschedule but keep in state to allow
                // external monitor process to decide what to do
                e->second->active = false;
            }

            break;
    }

    return popImpl( lock );
}


size_t
Queue::getMessageCount() {
    lock_guard<mutex> lock(m_mutex);
    return m_messages.size();
}

Queue::MsgIdList_t
Queue::getFailed() {
    MsgIdList_t failed;

    lock_guard<mutex> lock(m_mutex);
    return failed;
}

void
Queue::eraseFailed( const MsgIdList_t & a_msg_ids ) {
    lock_guard<mutex> lock(m_mutex);
}