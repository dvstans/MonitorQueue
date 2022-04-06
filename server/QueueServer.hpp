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
    Poco::Net::HTTPServerParams::Ptr    m_server_params;
    Poco::Net::HTTPServer *             m_server;
    Queue                               m_queue;
};

}
