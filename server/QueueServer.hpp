#include <map>
#include <Poco/URI.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServerRequest.h>
#include "Queue.hpp"

namespace MonQueue {

class QueueServer {
public:
    QueueServer();
    ~QueueServer();

    void start();
    void stop();

private:
    typedef Poco::Net::HTTPRequestHandler * (*HandlerFactory_t)();
    typedef std::map<std::string,HandlerFactory_t> RouteMap_t;

    class RequestHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory {
    public:
        Poco::Net::HTTPRequestHandler * createRequestHandler( const Poco::Net::HTTPServerRequest & request ) {
            Poco::URI uri( request.getURI() );

            //cout << "uri: " << uri << "\n";
            //cout << "method: " << request.getMethod() << "\n";

            RouteMap_t::iterator r = m_route_map.find( uri.getPath() );
            if ( r != m_route_map.end() ) {
                return r->second();
            }

            return 0;
        }
    };

    Poco::Net::HTTPServerParams *       m_server_params;
    Poco::Net::HTTPServer *             m_server;
    Queue                               m_queue;

    static RouteMap_t                   m_route_map;
};

}
