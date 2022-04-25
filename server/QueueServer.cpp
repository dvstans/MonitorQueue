#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <Poco/Exception.h>
#include <Poco/Timespan.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPServerParams.h>
#include "QueueServer.hpp"
#include "libjson.hpp"


using namespace std;
using namespace Poco;
using namespace Poco::Net;

namespace MonQueue {

void logger( const std::string & msg ) {
    cerr << "[MQSERVER] " << msg << endl;
}

class Handler : public HTTPRequestHandler {
  public:

    Handler( Queue & a_queue ) : m_queue( a_queue ) {
    }

    ~Handler() {
    }

    void handleRequest( HTTPServerRequest & a_request, HTTPServerResponse & a_response ) {
        Poco::URI uri( a_request.getURI() );
        RouteMap_t::iterator r = m_route_map.find( uri.getPath() );

        if ( r != m_route_map.end() ) {
            (this->*(r->second))( a_request, a_response );
        } else {
            sendResponse( a_response, 0, HTTPResponse::HTTP_NOT_FOUND );
        }
    }

  private:
    std::string readBody( HTTPServerRequest & a_request ) {
        string body;
        ssize_t size = a_request.getContentLength();

        if ( size > 0 ) {
            body.resize( size );
            a_request.stream().read( &body[0], size );
        }

        return body;
    }

    /** @brief Ping request to verify server running
     *
     * No request/reply parameters or payload.
     */
    void PingRequest( HTTPServerRequest & a_request, HTTPServerResponse & a_response ) {
        if ( a_request.getMethod() == "POST" ) {
            sendResponse( a_response, 0, HTTPResponse::HTTP_OK );
        } else {
            sendResponse( a_response, 0, HTTPResponse::HTTP_METHOD_NOT_ALLOWED );
        }
    }

    /** @brief Push one or more messages into queue
     *
     * Request is POST, body is JSON array:
     *
     *   [{ id: <string>, pri: <uint>, del: <uint> (optional) }]
     *
     * Response is empty (success), or JSON error document
     */
    void PushRequest( HTTPServerRequest & a_request, HTTPServerResponse & a_response ) {
        //cout << "PushRequest" << endl;

        if ( a_request.getMethod() == "POST" ) {
            size_t active, failed, free;
            libjson::Value req_json;

            try {
                string body = readBody( a_request );
                req_json.fromString( body );

                libjson::Value::Array & arr = req_json.asArray();
                for ( libjson::Value::ArrayIter m = arr.begin(); m != arr.end(); m++ ) {
                    libjson::Value::Object & msg = m->asObject();
                    msg.has("id");
                    msg.has("pri");
                    msg.has("del");

                    // TODO This is a hack until push has a built-in wait/timeout
                    while ( true ) {
                        m_queue.getCounts( active, failed, free );
                        if ( free ) {
                            break;
                        }
                        this_thread::sleep_for(chrono::milliseconds( 100 ));
                    }

                    m_queue.push( msg.getString("id"), (uint8_t)msg.getNumber("pri"), (size_t)(msg.has("del")?m->asNumber():0) );
                }

                sendResponse( a_response, 0, HTTPResponse::HTTP_OK );
            } catch( exception & e ) {
                string payload = string( "{\"type\":\"error\",\"message\":\"" ) + e.what() + "\"}";
                sendResponse( a_response, &payload, HTTPResponse::HTTP_BAD_REQUEST );
            }
        } else {
            sendResponse( a_response, 0, HTTPResponse::HTTP_METHOD_NOT_ALLOWED );
        }
    }

    /** @brief Push one or more messages into queue
     *
     * Request is POST, there are no params
     *
     * Response is a JSON message doc or JSON error document:
     *
     *   { type: msg, id: <string>, tok: <uint> }
     */
    void PopRequest( HTTPServerRequest & a_request, HTTPServerResponse & a_response ) {
        //cout << "PopRequest" << endl;

        if ( a_request.getMethod() == "POST" ) {
            const Queue::Msg_t & msg = m_queue.pop();

            string payload = "{\"type\":\"msg\",\"id\":\"";
            payload += msg.id;
            payload += "\",\"tok\":\"";
            payload += msg.token;
            payload += "\"}";

            sendResponse( a_response, &payload, HTTPResponse::HTTP_OK );
        } else {
            sendResponse( a_response, 0, HTTPResponse::HTTP_METHOD_NOT_ALLOWED );
        }
    }

    /** @brief Push one or more messages into queue
     *
     * Request is POST, body is JSON array:
     *
     *   { id: <string>, tok: <uint>, que: <bool> (optional), del: <uint> (optional) }]
     *
     * Response is empty (success), or JSON error document
     */
    void AckRequest( HTTPServerRequest & a_request, HTTPServerResponse & a_response ) {
        if ( a_request.getMethod() == "POST" ) {
            libjson::Value req_json;

            try {
                string body = readBody( a_request );
                req_json.fromString( body );
                libjson::Value::Object & ack = req_json.asObject();

                //cout << "tok" << ack.getNumber("tok") << ", as int: " << (uint64_t)ack.getNumber("tok") << "\n";
                m_queue.ack(
                    ack.getString("id"),
                    ack.getString("tok"),
                    ack.has("que")?ack.asBool():false,
                    (size_t)(ack.has("del")?ack.asNumber():0)
                );

                sendResponse( a_response, 0, HTTPResponse::HTTP_OK );
            } catch( exception & e ) {
                string payload = string( "{\"type\":\"error\",\"message\":\"" ) + e.what() + "\"}";
                sendResponse( a_response, &payload, HTTPResponse::HTTP_BAD_REQUEST );
            }
        } else {
            sendResponse( a_response, 0, HTTPResponse::HTTP_METHOD_NOT_ALLOWED );
        }
    }

    void PopAckRequest( HTTPServerRequest & a_request, HTTPServerResponse & a_response ) {
        if ( a_request.getMethod() == "POST" ) {
            libjson::Value req_json;

            try {
                string body = readBody( a_request );
                req_json.fromString( body );
                libjson::Value::Object & ack = req_json.asObject();

                //cout << "tok" << ack.getNumber("tok") << ", as int: " << (uint64_t)ack.getNumber("tok") << "\n";

                m_queue.ack(
                    ack.getString("id"),
                    ack.getString("tok"),
                    ack.has("que")?ack.asBool():false,
                    (size_t)(ack.has("del")?ack.asNumber():0)
                );

                const Queue::Msg_t & msg = m_queue.pop();

                string payload = "{\"type\":\"msg\",\"id\":\"";
                payload += msg.id;
                payload += "\",\"tok\":\"";
                payload += msg.token;
                payload += "\"}";

                sendResponse( a_response, &payload, HTTPResponse::HTTP_OK );
            } catch( exception & e ) {
                string payload = string( "{\"type\":\"error\",\"message\":\"" ) + e.what() + "\"}";
                sendResponse( a_response, &payload, HTTPResponse::HTTP_BAD_REQUEST );
            }
        } else {
            sendResponse( a_response, 0, HTTPResponse::HTTP_METHOD_NOT_ALLOWED );
        }
    }

    void CountRequest( HTTPServerRequest & a_request, HTTPServerResponse & a_response ) {
        if ( a_request.getMethod() == "GET" ) {
            try {
                size_t active, failed, free;

                m_queue.getCounts( active, failed, free );

                string payload = "{\"type\":\"count\",\"capacity\":";
                payload += to_string( m_queue.getCapacity() );
                payload += ",\"active\":";
                payload += to_string( active );
                payload += ",\"failed\":";
                payload += to_string( failed );
                payload += ",\"free\":";
                payload += to_string( free );
                payload += "}";

                sendResponse( a_response, &payload, HTTPResponse::HTTP_OK );
            } catch( exception & e ) {
                string payload = string( "{\"type\":\"error\",\"message\":\"" ) + e.what() + "\"}";
                sendResponse( a_response, &payload, HTTPResponse::HTTP_BAD_REQUEST );
            }
        } else {
            sendResponse( a_response, 0, HTTPResponse::HTTP_METHOD_NOT_ALLOWED );
        }
    }

    void GetFailedRequest( HTTPServerRequest & a_request, HTTPServerResponse & a_response ) {
        if ( a_request.getMethod() == "GET" ) {
            try {
                Queue::MsgIdList_t failed = m_queue.getFailed();

                string payload = "{\"type\":\"failed\",\"ids\":[";
                for ( Queue::MsgIdList_t::iterator i = failed.begin(); i != failed.end(); i++ ) {
                    if ( i != failed.begin() ){
                        payload += ",";
                    }
                    payload += "\"";
                    payload += *i;
                    payload += "\"";
                }
                payload += "]}";

                sendResponse( a_response, &payload, HTTPResponse::HTTP_OK );

            } catch( exception & e ) {
                string payload = string( "{\"type\":\"error\",\"message\":\"" ) + e.what() + "\"}";
                sendResponse( a_response, &payload, HTTPResponse::HTTP_BAD_REQUEST );
            }
        } else {
            sendResponse( a_response, 0, HTTPResponse::HTTP_METHOD_NOT_ALLOWED );
        }
    }

    void EraseFailedRequest( HTTPServerRequest & a_request, HTTPServerResponse & a_response ) {
        if ( a_request.getMethod() == "POST" ) {
            libjson::Value req_json;

            try {
                string body = readBody( a_request );
                req_json.fromString( body );
                libjson::Value::Array & req_ids = req_json.asArray();
                Queue::MsgIdList_t ids;

                for ( libjson::Value::ArrayIter i = req_ids.begin(); i != req_ids.end(); i++ ) {
                    ids.push_back( i->asString() );
                }

                Queue::MsgIdList_t erased = m_queue.eraseFailed( ids );

                string payload = "{\"type\":\"erased\",\"ids\":[";
                for ( Queue::MsgIdList_t::iterator i = erased.begin(); i != erased.end(); i++ ) {
                    if ( i != erased.begin() ){
                        payload += ",";
                    }
                    payload += "\"";
                    payload += *i;
                    payload += "\"";
                }
                payload += "]}";

                sendResponse( a_response, &payload, HTTPResponse::HTTP_OK );
            } catch( exception & e ) {
                string payload = string( "{\"type\":\"error\",\"message\":\"" ) + e.what() + "\"}";
                sendResponse( a_response, &payload, HTTPResponse::HTTP_BAD_REQUEST );
            }
        } else {
            sendResponse( a_response, 0, HTTPResponse::HTTP_METHOD_NOT_ALLOWED );
        }
    }

    void sendResponse( HTTPServerResponse & a_response, string * a_payload, HTTPResponse::HTTPStatus a_status ) {
        a_response.setStatus( a_status );
        a_response.setContentType("application/json");

        if ( a_payload ){
            a_response.sendBuffer( &(*a_payload)[0], a_payload->size());
        } else {
            a_response.sendBuffer("",0);
        }
    }

    friend QueueServer;

    typedef void (Handler::*Endpoint_t)(HTTPServerRequest &, HTTPServerResponse &);
    typedef map<string,Endpoint_t> RouteMap_t;

    static RouteMap_t m_route_map;

    static void setupRouteMap() {
        if ( !m_route_map.size() ) {
            m_route_map["/ping"] = &Handler::PingRequest;
            m_route_map["/push"] = &Handler::PushRequest;
            m_route_map["/pop"] = &Handler::PopRequest;
            m_route_map["/ack"] = &Handler::AckRequest;
            m_route_map["/pop_ack"] = &Handler::PopAckRequest;
            m_route_map["/count"] = &Handler::CountRequest;
            m_route_map["/failed"] = &Handler::GetFailedRequest;
            m_route_map["/failed/erase"] = &Handler::EraseFailedRequest;
        }
    }

    Queue & m_queue;
};

Handler::RouteMap_t Handler::m_route_map;

class HandlerFactory : public Poco::Net::HTTPRequestHandlerFactory {
  public:

    HandlerFactory( Queue & a_queue ) : m_queue( a_queue ) {
    }

    Poco::Net::HTTPRequestHandler * createRequestHandler( const Poco::Net::HTTPServerRequest & request ) {
        return new Handler( m_queue );
    }

  private:

    Queue &     m_queue;
};


QueueServer::QueueServer(
    uint8_t a_priority_count,
    size_t a_msg_capacity,
    size_t a_msg_ack_timeout_msec,
    size_t a_msg_max_retries,
    size_t a_msg_boost_timeout_msec,
    size_t a_monitor_period_msec
) :
    m_queue(
        a_priority_count,
        a_msg_capacity,
        a_msg_ack_timeout_msec,
        a_msg_max_retries,
        a_msg_boost_timeout_msec,
        a_monitor_period_msec
    )
{
    try {
        m_queue.setErrorCallback( &logger );

        Handler::setupRouteMap();

        m_server_params = new HTTPServerParams();
        m_server_params->setServerName( "mqserver" );
        m_server_params->setKeepAlive(true);
        m_server_params->setKeepAliveTimeout( Timespan( 5, 0 ));
        m_server_params->setMaxKeepAliveRequests( 10 );

        m_server = new HTTPServer( new HandlerFactory( m_queue ), ServerSocket(8080), m_server_params );
    } catch ( const Poco::Exception & e ) {
        cout << "ctor exception: " << e.displayText() << endl;
        throw;
    } catch ( const exception & e ) {
        cout << "ctor exception 2: " << e.what() << endl;
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
    } catch ( const Poco::Exception & e ) {
        cout << "start exception: " << e.displayText() << endl;
        throw;
    }
}

void
QueueServer::stop(){
}


} // namespace MonQueue
