#include <map>
#include <Poco/URI.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include "Queue.hpp"

namespace MonQueue {

class QueueServer {
  public:

    QueueServer(
        uint8_t a_priority_count,
        size_t a_msg_capacity,
        size_t a_msg_ack_timeout_msec,
        size_t a_msg_max_retries,
        size_t a_msg_boost_timeout_msec,
        size_t a_monitor_period_msec
    );

    ~QueueServer();

    void start();
    void stop();

  private:

    Poco::Net::HTTPServerParams *       m_server_params;
    Poco::Net::HTTPServer *             m_server;
    Queue                               m_queue;

    friend class HandlerFactory;
    friend class Handler;
};

}
