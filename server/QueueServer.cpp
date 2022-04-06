#include <stdexcept>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServerParams.h>
#include "QueueServer.hpp"


using namespace std;
using namespace Poco;
using namespace Poco::Net;

namespace MonQueue {

class TestReqHandler : public HTTPRequestHandler {
public:
    TestReqHandler() {}
    ~TestReqHandler() {}

    void handleRequest( HTTPServerRequest & request, HTTPServerResponse & response ) {
        (void) request;

        // Do something
    }
};

class MonQueueReqHandlerFactory : public HTTPRequestHandlerFactory {
public:
    MonQueueReqHandlerFactory() {}
    ~MonQueueReqHandlerFactory() {}

    HTTPRequestHandler * createRequestHandler( const HTTPServerRequest & request ) {
        return new TestReqHandler();
    }
};

QueueServer::QueueServer() : m_server(0), m_queue( 3, 100, 30000 ) {
    m_server_params = HTTPServerParams::Ptr( new HTTPServerParams() );
    m_server_params->setServerName( "mqserver" );
}

QueueServer::~QueueServer() {
    if ( m_server ) {
        delete m_server;
    }
}

void
QueueServer::start(){
    if ( m_server ) {
        throw runtime_error("Server already running");
    }

    auto factory = HTTPRequestHandlerFactory::Ptr( new MonQueueReqHandlerFactory() );

    m_server = new HTTPServer( factory, 8080, m_server_params );
}

void
QueueServer::stop(){
}

} // namespace MonQueue
