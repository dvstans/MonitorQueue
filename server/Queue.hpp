#include <string>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <random>

/*
TODO / Think about
- support minimum queue wait time option (for polling tasks)
- Need to free memory
*/

class Queue {
public:
    struct Msg_t {
        std::string     id;
        uint64_t        token;
        std::string     data;
    };

    typedef std::vector<std::string> MsgIdList_t;
    typedef void (ErrorCB_t)( const std::string & msg );

    Queue( uint8_t a_priorities, size_t a_capacity, size_t a_poll_interval = 5000, size_t a_fail_timeout = 30000 );
    ~Queue();

    // API for publisher
    void            push( const std::string & a_id, const std::string & a_data, uint8_t a_priority, size_t a_delay = 0 );

    // API for consumer
    const Msg_t &   pop();
    void            ack( const std::string & a_id, uint64_t a_token, bool a_requeue = false, size_t a_delay = 0 );
    const Msg_t &   popAck( const std::string & a_id, uint64_t a_token, bool a_requeue = false, size_t a_delay = 0 );

    // Prevent implcit conversions of token types on Ack methods
    template <typename T>
    void            ack( const std::string & a_id, T a_token, bool a_requeue = false ) = delete;
    template <typename T>
    void            popAck( const std::string & a_id, T a_token, bool a_requeue = false ) = delete;


    // API for monitoring
    void            setErrorCallback( ErrorCB_t * a_callback );
    size_t          count();
    size_t          freeCount();
    size_t          workingCount();
    size_t          failedCount();
    MsgIdList_t     getFailed();
    MsgIdList_t     eraseFailed( const MsgIdList_t & a_msg_ids );

private:
    typedef std::chrono::time_point<std::chrono::system_clock> timestamp_t;

    enum MsgState_t {
        MSG_QUEUED = 0,
        MSG_RUNNING,
        MSG_DELAYED,
        MSG_FAILED
    };

    struct MsgEntry_t {
        MsgEntry_t( const std::string & a_id, const std::string & a_data, uint8_t a_priority ) :
            priority( a_priority ),
            boosted( false ),
            fail_count( 0 ),
            state( MSG_QUEUED ),
            state_ts( std::chrono::system_clock::now() ),
            message(Msg_t{ a_id, 0, a_data })
        {};

        void reset( const std::string & a_id, const std::string & a_data, uint8_t a_priority ) {
            priority = a_priority;
            boosted = false;
            fail_count = 0;
            state = MSG_QUEUED;
            state_ts = std::chrono::system_clock::now();
            message.id = a_id;
            message.token = 0;
            message.data = a_data;
        }

        uint8_t                 priority;   ///< Message priority
        bool                    boosted;    ///< True if priority has been boosted
        uint8_t                 fail_count; ///< Fail count
        MsgState_t              state;      ///< Queued, running, failed (for monitoring)
        timestamp_t             state_ts;   ///< Time when message changed state (for monitoring)
        Msg_t                   message;    ///< Message (shared ptr)
    };

    struct DelaySetCompare final
    {
        bool operator() ( const MsgEntry_t * left, const MsgEntry_t * right) const
        {
            return left->state_ts < right->state_ts;
        }
    };

    typedef std::vector<std::deque<MsgEntry_t*>> queue_list_t;
    typedef std::map<std::string,MsgEntry_t*> msg_map_t;
    typedef std::multiset<MsgEntry_t*,DelaySetCompare> msg_delay_t;
    typedef std::vector<MsgEntry_t*> msg_pool_t;

    MsgEntry_t * getMsgEntry( const std::string & a_id, const std::string & a_data, uint8_t a_priority );
    const Msg_t & popImpl( std::unique_lock<std::mutex> & a_lock );
    void ackImpl( const std::string & a_id, uint64_t a_token, bool a_requeue, size_t a_delay );
    void insertDelayedMsg( MsgEntry_t * a_msg, const timestamp_t & a_requeue_ts  );
    void monitorThread();
    void delayThread();

    size_t qcount(); // DEBUG

    std::mutex                  m_mutex;
    std::condition_variable     m_cv;
    size_t                      m_capacity;
    size_t                      m_count_queued;
    size_t                      m_count_failed;
    msg_pool_t                  m_msg_pool;
    msg_map_t                   m_msg_map;
    msg_delay_t                 m_msg_delay;
    queue_list_t                m_queue_list;
    std::mt19937_64             m_rng;
    size_t                      m_poll_interval;    ///< Internal monitoring poll interval in msec
    size_t                      m_fail_timeout;     ///< Message ACK fail timeout in msec (max runtime)
    size_t                      m_max_retries;
    size_t                      m_boost_timeout;    ///< Message priority boost timeout in msec
    std::thread                 m_monitor_thread;
    std::condition_variable     m_mon_cv;
    std::thread                 m_delay_thread;
    std::condition_variable     m_delay_cv;
    bool                        m_run;
    ErrorCB_t                 * m_err_cb;
};
