#include <string>
#include <vector>
#include <map>
#include <deque>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <memory>

/*
TODO / Think about

- what if client acks with wrong/stale ID and acks another workers message?
- is a too may retry fail state needed? workers should detect/report failure to DB state
*/

class Queue {
public:
    struct Msg_t {
        std::string     id;
        std::string     data;
    };

    enum MsgState_t {
        MSG_DONE,
        MSG_CONT,
        MSG_FAIL
    };

    typedef std::shared_ptr<Msg_t> pMsg_t;
    typedef std::vector<std::string> MsgIdList_t;

    Queue( uint8_t a_priorities, size_t a_capacity );
    ~Queue();

    // API for publisher
    void push( const std::string & a_id, const std::string & a_data, uint8_t a_priority );

    // API for consumer
    pMsg_t pop();
    void   ack( const std::string & a_id, MsgState_t a_state );
    pMsg_t popAck( const std::string & a_id, MsgState_t a_state );

    // API for monitoring
    size_t getMessageCount();
    MsgIdList_t getFailed();
    void eraseFailed( const MsgIdList_t & a_msg_ids );

private:
    typedef std::chrono::time_point<std::chrono::system_clock> timestamp_t;

    struct MsgEntry_t {
        uint8_t                 priority;   ///< Message priority
        uint8_t                 fail_count; ///< Fail count
        bool                    active;     ///< True if message is active (being processes by consumer)
        timestamp_t             ts_rcvd;    ///< Receive time needed for fair scheduling
        timestamp_t             ts_active;  ///< Time when message was activated (for monitoring)
        pMsg_t                  message;    ///< Message (shared ptr)
    };

    typedef std::vector<std::deque<MsgEntry_t*>> queue_store_t;
    typedef std::map<std::string,MsgEntry_t*> msg_state_t;

    pMsg_t popImpl( std::unique_lock<std::mutex> & a_lock );

    std::mutex                  m_mutex;
    std::condition_variable     m_cv;
    size_t                      m_capacity;
    size_t                      m_count_queued;
    msg_state_t                 m_messages;
    queue_store_t               m_queues;
};
