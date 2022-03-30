#include <string>
#include <vector>
#include <map>
#include <deque>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <random>

/*
TODO / Think about
- starved low priority messages
- monitor / priority-boost thread
- support minimum queue wait time option (for polling tasks)
- Logging? Or registered error handlers?
- Revisit deque or list / forward_list?
- terminate mon thread w/o delay (use cv/notify)
*/

class Queue {
public:
    struct Msg_t {
        std::string     id;
        uint64_t        token;
        std::string     data;
    };

    typedef std::shared_ptr<Msg_t> pMsg_t;
    typedef std::vector<std::string> MsgIdList_t;

    Queue( uint8_t a_priorities, size_t a_capacity, size_t a_poll_interval = 5000, size_t a_fail_timeout = 30000 );
    ~Queue();

    // API for publisher
    void push( const std::string & a_id, const std::string & a_data, uint8_t a_priority );

    // API for consumer
    pMsg_t pop();
    void   ack( const std::string & a_id, uint64_t a_token, bool a_requeue = false );
    pMsg_t popAck( const std::string & a_id, uint64_t a_token, bool a_requeue = false );

    // Prevent implcit conversions of token types on Ack methods
    template <typename T>
    void ack( const std::string & a_id, T a_token, bool a_requeue = false ) = delete;
    template <typename T>
    void popAck( const std::string & a_id, T a_token, bool a_requeue = false ) = delete;


    // API for monitoring
    size_t getMessageCount();
    MsgIdList_t getFailed();
    void eraseFailed( const MsgIdList_t & a_msg_ids );

private:
    typedef std::chrono::time_point<std::chrono::system_clock> timestamp_t;

    enum MsgState_t {
        MSG_QUEUED,
        MSG_RUNNING,
        MSG_FAILED
    };

    struct MsgEntry_t {
        uint8_t                 priority;   ///< Message priority
        uint8_t                 fail_count; ///< Fail count
        MsgState_t              state;      ///< Queued, running, failed (for monitoring)
        timestamp_t             state_ts;   ///< Time when message changed state (for monitoring)
        pMsg_t                  message;    ///< Message (shared ptr)
    };

    typedef std::vector<std::deque<MsgEntry_t*>> queue_store_t;
    typedef std::map<std::string,MsgEntry_t*> msg_state_t;

    pMsg_t popImpl( std::unique_lock<std::mutex> & a_lock );
    void ackImpl( const std::string & a_id, uint64_t a_token, bool a_requeue );
    void monitorThread();

    std::mutex                  m_mutex;
    std::condition_variable     m_cv;
    size_t                      m_capacity;
    size_t                      m_count_queued;
    msg_state_t                 m_messages;
    queue_store_t               m_queues;
    std::mt19937_64             m_rng;
    size_t                      m_poll_interval;    ///< Internal monitoring poll interval in msec
    size_t                      m_fail_timeout;     ///< Message ACK fail timeout in msec (max runtime)
    size_t                      m_max_retries;
    size_t                      m_boost_timeout;    ///< Message priority boost timeout in msec
    std::thread                 m_monitor_thread;
    bool                        m_run;
};
