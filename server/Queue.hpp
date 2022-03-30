#include <string>
#include <vector>
#include <map>
#include <deque>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <random>

/*
TODO / Think about
- starved low priority messages
- monitor / priority-boost thread
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

    Queue( uint8_t a_priorities, size_t a_capacity );
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
        timestamp_t             ts_rcvd;    ///< Receive time needed for fair scheduling
        timestamp_t             ts_run;     ///< Time when message started running (for monitoring)
        pMsg_t                  message;    ///< Message (shared ptr)
    };

    typedef std::vector<std::deque<MsgEntry_t*>> queue_store_t;
    typedef std::map<std::string,MsgEntry_t*> msg_state_t;

    pMsg_t popImpl( std::unique_lock<std::mutex> & a_lock );
    void ackImpl( const std::string & a_id, uint64_t a_token, bool a_requeue );

    std::mutex                  m_mutex;
    std::condition_variable     m_cv;
    size_t                      m_capacity;
    size_t                      m_count_queued;
    msg_state_t                 m_messages;
    queue_store_t               m_queues;
    std::mt19937_64             m_rng;
};
