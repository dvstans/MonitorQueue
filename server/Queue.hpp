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

/* TODO
- Add mult-message push
- Add timeout option to push and pop methods
- Add non-blocking push and pop methods
- Template message data type or polymorphic msg class (or none at all)
*/

namespace MonQueue {

/** @brief Message queue class with progess monitoring
 *
 * The Queue class is a priority message queue with built-in consumer progress
 * monitoring and optional enqueue delay. Messages consist of a producer-
 * defined unique ID (string) and a data payload (string).
 *
 * Monitoring is based on a maximum consumer acknowledgement timeout. If this
 * limit is exceeded, the consumer is considered failed and the associated
 * message is re-enqueued for a retry. A retry limit can be specified, and if
 * exceeded on a given message, the message will be marked as failed and no
 * further processing will be attempted. Failed messages a retained internally
 * and consume queue capacity; thus the producer must monitor for, and handle,
 * failed messages.
 *
 * The Queue class is fully thread-safe.
 */
class Queue {
public:
    /// @brief Message structure for use by consumers
    struct Msg_t {
        std::string     id;     ///< Unique producer-specified message ID
        //std::string     data;   ///< Optional producer-specified data payload
        uint64_t        token;  ///< Queue defined message token required for ACK
    };

    typedef std::vector<std::string> MsgIdList_t;           ///< Message ID list type
    typedef void (ErrorCB_t)( const std::string & msg );    ///< Error callback type

    Queue(
        uint8_t a_priority_count,
        size_t a_msg_capacity,
        size_t a_msg_ack_timeout_msec,
        size_t a_msg_max_retries = 10,
        size_t a_msg_boost_timeout_msec = 60000,
        size_t a_monitor_period_msec = 5000,
        ErrorCB_t a_err_cb = 0
    );

    ~Queue();

    //----- Methods for use by publisher(s)

    void            push( const std::string & a_id /*, const std::string & a_data*/, uint8_t a_priority, size_t a_delay = 0 );

    //----- Methods for use by consumer(s)

    const Msg_t &   pop();
    void            ack( const std::string & a_id, uint64_t a_token, bool a_requeue = false, size_t a_delay = 0 );
    const Msg_t &   popAck( const std::string & a_id, uint64_t a_token, bool a_requeue = false, size_t a_delay = 0 );

    // Prevent implcit conversions of token types on Ack methods
    template <typename T>
    void            ack( const std::string & a_id, T a_token, bool a_requeue = false ) = delete;
    template <typename T>
    void            popAck( const std::string & a_id, T a_token, bool a_requeue = false ) = delete;


    //----- Methods for use by monitoring process

    void            setErrorCallback( ErrorCB_t * a_callback );
    size_t          getCapacity() const;
    void            getCounts( size_t & a_active, size_t & a_failed, size_t & a_free ) const;
    MsgIdList_t     getFailed() const;
    MsgIdList_t     eraseFailed( const MsgIdList_t & a_msg_ids );

private:
    /// General timestamp type
    typedef std::chrono::time_point<std::chrono::system_clock> timestamp_t;

    /// Internal message state
    enum MsgState_t {
        MSG_QUEUED = 0,     ///< Message is in a queue and ready for consumption
        MSG_RUNNING,        ///< Message has been de-queued by consumer
        MSG_DELAYED,        ///< Message is in the delay queue
        MSG_FAILED          ///< Message is failed
    };

    /// Internal message entry record
    struct MsgEntry_t {
        /// Constructor
        MsgEntry_t( const std::string & a_id, /*const std::string & a_data,*/ uint8_t a_priority ) :
            priority( a_priority ),
            boosted( false ),
            fail_count( 0 ),
            state( MSG_QUEUED ),
            state_ts( std::chrono::system_clock::now() ),
            message(Msg_t{ a_id, /*a_data,*/ 0 })
        {};

        /// Reset message for re-use
        void reset( const std::string & a_id, /*const std::string & a_data,*/ uint8_t a_priority ) {
            priority = a_priority;
            boosted = false;
            fail_count = 0;
            state = MSG_QUEUED;
            state_ts = std::chrono::system_clock::now();
            message.id = a_id;
            /*message.data = a_data;*/
            message.token = 0;
        }

        uint8_t                 priority;   ///< Message priority
        bool                    boosted;    ///< True if priority has been boosted
        uint8_t                 fail_count; ///< Fail count
        MsgState_t              state;      ///< Queued, running, failed (for monitoring)
        timestamp_t             state_ts;   ///< Time when message changed state (for monitoring)
        Msg_t                   message;    ///< Message data
    };

    /// Custom multiset comparator to sort message entries by time
    struct DelaySetCompare
    {
        bool operator() ( const MsgEntry_t * left, const MsgEntry_t * right) const
        {
            return left->state_ts < right->state_ts;
        }
    };

    // Typedefs used by implementation

    typedef std::vector<std::deque<MsgEntry_t*>>        queue_list_t;
    typedef std::map<std::string,MsgEntry_t*>           msg_map_t;
    typedef std::multiset<MsgEntry_t*,DelaySetCompare>  msg_delay_t;
    typedef std::vector<MsgEntry_t*>                    msg_pool_t;

    // Private methods (see source for documentation)

    MsgEntry_t *    getMsgEntry( const std::string & a_id, /*const std::string & a_data,*/ uint8_t a_priority );
    const Msg_t &   popImpl( std::unique_lock<std::mutex> & a_lock );
    void            ackImpl( const std::string & a_id, uint64_t a_token, bool a_requeue, size_t a_delay );
    void            insertDelayedMsg( MsgEntry_t * a_msg, const timestamp_t & a_requeue_ts  );
    void            monitorThread();
    void            delayThread();

    size_t                      m_capacity;         ///< Max message capacity (including failed)
    size_t                      m_fail_timeout;     ///< Message ACK fail timeout in msec (max runtime)
    size_t                      m_max_retries;      ///< Maximum per-message dequeue retries
    size_t                      m_boost_timeout;    ///< Message priority boost timeout in msec
    size_t                      m_poll_interval;    ///< Internal monitoring poll interval in msec
    ErrorCB_t                 * m_err_cb;           ///< Error callback function ptr
    size_t                      m_count_queued;     ///< Number of messages in queues
    size_t                      m_count_failed;     ///< Number of messages in failed state
    bool                        m_run;              ///< Run/stop flag for internal threads
    std::mt19937_64             m_rng;              ///< Random number generator for ACK tokens
    std::thread                 m_monitor_thread;   ///< Monitoring thread
    std::condition_variable     m_mon_cv;           ///< Monitoring cond var
    std::thread                 m_delay_thread;     ///< Delay thread
    std::condition_variable     m_delay_cv;         ///< Delay cond var
    mutable std::mutex          m_mutex;            ///< Mutex for all shared message structures
    std::condition_variable     m_pop_cv;           ///< Cond var for pop methods
    msg_pool_t                  m_msg_pool;         ///< Message entry memory pool
    msg_map_t                   m_msg_map;          ///< Message ID to entry index
    msg_delay_t                 m_msg_delay;        ///< Message delay queue
    queue_list_t                m_queue_list;       ///< Queue list (one queue per priority)
};

} // MonQueue namespace

