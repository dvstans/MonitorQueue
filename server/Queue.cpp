#include <stdexcept>
#include "Queue.hpp"

using namespace std;

Queue::Queue( uint8_t a_priorities, size_t a_capacity ) :
    m_msg_capacity(a_capacity), m_msg_count(0)
{
    m_queues.resize( a_priorities );
}

Queue::~Queue() {
}

void
Queue::push( const std::string & a_id, const std::string & a_payload, uint8_t a_priority ) {
    // Verify priority
    if ( a_priority >= m_queues.size() ) {
        throw runtime_error( "Invalid queue priority" );
    }

    lock_guard<mutex> lock(m_mutex);

    // Check for duplicate messages
    if ( m_msg_state.find( a_id ) != m_msg_state.end() ) {
        throw runtime_error( "Duplicate message ID" );
    }

    // Make sure capacity isn't exceeded
    if ( m_msg_count == m_msg_capacity ) {
        throw length_error( "Queue capacity exceeded" );
    }

    MsgEntry_t * entry = new MsgEntry_t {
        a_priority,
        0,
        false,
        std::chrono::system_clock::now(),
        timestamp_t(),
        make_shared<Msg_t>(new Msg_t { a_id, a_payload })
    };

    m_msg_state[a_id] = entry;
    m_queues[a_priority].push_front( entry );

    m_msg_count++;

    m_cv.notify_one();
}

Queue::pMsg_t
Queue::pop() {
    unique_lock<mutex> lock(m_mutex);

    while ( !m_msg_count ) {
        m_cv.wait( lock );
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
        throw logic_error( "All queues empty when m_msg_count > 0" );
    }

    entry->active = true;
    entry->ts_active = std::chrono::system_clock::now();
    pMsg_t msg = entry->message;

    return msg;
}

/*
Queue::pMsg_t
Queue::ackPop( const std::string & a_id, bool a_success ) {
    lock_guard<mutex> lock(m_mutex);
}
*/

size_t
Queue::getMessageCount() {
    lock_guard<mutex> lock(m_mutex);
    return m_msg_count;
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