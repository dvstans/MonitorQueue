#include <iostream>
#include <stdexcept>
#include <Poco/Exception.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPServerParams.h>
#include "QueueServer.hpp"


using namespace std;
using namespace Poco;
using namespace Poco::Net;

namespace MonQueue {

class HelloHandler : public HTTPRequestHandler {
public:
    static HTTPRequestHandler * factory() {
        return new HelloHandler();
    }

    void handleRequest( HTTPServerRequest & request, HTTPServerResponse & response ) {
        (void) request;

        response.setStatus(HTTPResponse::HTTP_OK);
        response.setContentType("text/html");
        ostream& out = response.send();
        out << "Hello World!\n";
        out.flush();
    }
};

class GoodbyeHandler : public HTTPRequestHandler {
public:
    static HTTPRequestHandler * factory() {
        return new GoodbyeHandler();
    }

    void handleRequest( HTTPServerRequest & request, HTTPServerResponse & response ) {
        (void) request;

        response.setStatus(HTTPResponse::HTTP_OK);
        response.setContentType("text/html");
        ostream& out = response.send();
        out << "Goodbye World!\n";
        out.flush();
    }
};

// Static route map
QueueServer::RouteMap_t   QueueServer::m_route_map;

QueueServer::QueueServer() : m_queue( 3, 100, 30000 ) {
    try {
        if ( !m_route_map.size() ) {
            m_route_map["/hello/world"] = &HelloHandler::factory;
            m_route_map["/goodbye/world"] = &GoodbyeHandler::factory;
        }

        m_server_params = new HTTPServerParams();
        m_server_params->setServerName( "mqserver" );
        m_server = new HTTPServer( new RequestHandlerFactory(), ServerSocket(8080), m_server_params );
    } catch ( Poco::Exception & e ) {
        cout << "exception: " << e.displayText() << endl;
        throw;
    }
}

QueueServer::~QueueServer() {
    delete m_server;
}

void
QueueServer::start(){
    try {
        m_server->start();
    } catch ( Poco::Exception & e ) {
        cout << "exception: " << e.displayText() << endl;
    }
}

void
QueueServer::stop(){
}

} // namespace MonQueue
