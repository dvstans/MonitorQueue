#include <map>
#include <Poco/URI.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include "Queue.hpp"

namespace MonQueue {

class QueueServer {
  public:

    QueueServer();
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
