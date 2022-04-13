#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <Poco/Exception.h>
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
        //cout << "Handler ctor " << this << endl;

        /* unit test - move to test code
        string msg_json;
        libjson::Value req_json;

        for ( int i = 0; i < 100000; i++ ){
            m_queue.push( to_string( i ), 0, 0 );
            const Queue::Msg_t & msg = m_queue.pop();

            msg_json = string("{\"type\":\"msg\",\"id\":\"") + msg.id + "\",\"tok\":" + to_string( msg.token ) + "}";

            try {
                req_json.fromString( msg_json );
                libjson::Value::Object & ack = req_json.asObject();

                //cout << "tok" << ack.getNumber("tok") << ", as int: " << (uint64_t)ack.getNumber("tok") << "\n";
                m_queue.ack( ack.getString("id"), (uint64_t)ack.getNumber("tok"), false, 0 );

            } catch ( exception & e ) {
                cout << "exception: " << e.what() << endl;
            }
        }
        */
    }

    ~Handler() {
        //cout << "Handler dtor " << this << endl;
    }

    void handleRequest( HTTPServerRequest & a_request, HTTPServerResponse & a_response ) {
        Poco::URI uri( a_request.getURI() );
        RouteMap_t::iterator r = m_route_map.find( uri.getPath() );

        if ( r != m_route_map.end() ) {
            (this->*(r->second))( a_request, a_response );
        } else {
            prepResponse( a_response, HTTPResponse::HTTP_NOT_FOUND );
        }
    }

  private:

    /** @brief Push one or more messages into queue
     *
     * Request is POST, body is JSON array:
     *
     *   [{ id: <string>, pri: <uint>, del: <uint> (optional) }]
     *
     * Response is empty (success), or JSON error document
     */
    void PushRequest( HTTPServerRequest & a_request, HTTPServerResponse & a_response ) {
        cout << "PushRequest" << endl;

        if ( a_request.getMethod() == "POST" ) {
            size_t  active, failed, free;
            string body( istreambuf_iterator<char>( a_request.stream() ), {});
            libjson::Value req_json;

            try {
                req_json.fromString( body );

                libjson::Value::Array & arr = req_json.asArray();
                for ( libjson::Value::ArrayIter m = arr.begin(); m != arr.end(); m++ ) {
                    libjson::Value::Object & msg = m->asObject();

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

                prepResponse( a_response, HTTPResponse::HTTP_OK );
            } catch( exception & e ) {
                ostream & out = prepResponse( a_response, HTTPResponse::HTTP_BAD_REQUEST );
                out << "{\"type\":\"error\",\"message\":\"" << e.what() << "\"";
            }
        } else {
            prepResponse( a_response, HTTPResponse::HTTP_METHOD_NOT_ALLOWED );
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
        cout << "PopRequest" << endl;

        if ( a_request.getMethod() == "POST" ) {
            const Queue::Msg_t & msg = m_queue.pop();

            ostream & out = prepResponse( a_response, HTTPResponse::HTTP_OK );
            out << "{\"type\":\"msg\",\"id\":\"" << msg.id << "\",\"tok\":"<< msg.token <<"}";
        } else {
            prepResponse( a_response, HTTPResponse::HTTP_METHOD_NOT_ALLOWED );
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
            string body( istreambuf_iterator<char>( a_request.stream() ), {});
            libjson::Value req_json;

            try {
                req_json.fromString( body );
                libjson::Value::Object & ack = req_json.asObject();

                cout << "tok" << ack.getNumber("tok") << ", as int: " << (uint64_t)ack.getNumber("tok") << "\n";
                m_queue.ack(
                    ack.getString("id"),
                    (uint64_t)ack.getNumber("tok"),
                    ack.has("que")?ack.asBool():false,
                    (size_t)(ack.has("del")?ack.asNumber():0)
                );

                prepResponse( a_response, HTTPResponse::HTTP_OK );
            } catch( exception & e ) {
                ostream & out = prepResponse( a_response, HTTPResponse::HTTP_BAD_REQUEST );
                out << "{\"type\":\"error\",\"message\":\"" << e.what() << "\"";
            }
        } else {
            prepResponse( a_response, HTTPResponse::HTTP_METHOD_NOT_ALLOWED );
        }
    }

    void PopAckRequest( HTTPServerRequest & a_request, HTTPServerResponse & a_response ) {
        if ( a_request.getMethod() == "POST" ) {
            string body( istreambuf_iterator<char>( a_request.stream() ), {});
            libjson::Value req_json;

            try {
                req_json.fromString( body );
                libjson::Value::Object & ack = req_json.asObject();

                cout << "tok" << ack.getNumber("tok") << ", as int: " << (uint64_t)ack.getNumber("tok") << "\n";

                m_queue.ack(
                    ack.getString("id"),
                    (uint64_t)ack.getNumber("tok"),
                    ack.has("que")?ack.asBool():false,
                    (size_t)(ack.has("del")?ack.asNumber():0)
                );

                const Queue::Msg_t & msg = m_queue.pop();

                ostream & out = prepResponse( a_response, HTTPResponse::HTTP_OK );
                out << "{\"type\":\"msg\",\"id\":\"" << msg.id << "\",\"tok\":"<< msg.token <<"}";
            } catch( exception & e ) {
                ostream & out = prepResponse( a_response, HTTPResponse::HTTP_BAD_REQUEST );
                out << "{\"type\":\"error\",\"message\":\"" << e.what() << "\"";
            }
        } else {
            prepResponse( a_response, HTTPResponse::HTTP_METHOD_NOT_ALLOWED );
        }
    }

    void CountRequest( HTTPServerRequest & a_request, HTTPServerResponse & a_response ) {
        if ( a_request.getMethod() == "GET" ) {
            try {
                size_t active, failed, free;

                m_queue.getCounts( active, failed, free );

                ostream & out = prepResponse( a_response, HTTPResponse::HTTP_OK );
                out << "{\"type\":\"count\",\"capacity\":" << m_queue.getCapacity()
                    << ",\"active\":" << active
                    << ",\"failed\":" << failed
                    << ",\"free\":" << free
                    << "}";

            } catch( exception & e ) {
                ostream & out = prepResponse( a_response, HTTPResponse::HTTP_BAD_REQUEST );
                out << "{\"type\":\"error\",\"message\":\"" << e.what() << "\"";
            }
        } else {
            prepResponse( a_response, HTTPResponse::HTTP_METHOD_NOT_ALLOWED );
        }
    }

    void GetFailedRequest( HTTPServerRequest & a_request, HTTPServerResponse & a_response ) {
        if ( a_request.getMethod() == "GET" ) {
            try {
                Queue::MsgIdList_t failed = m_queue.getFailed();

                ostream & out = prepResponse( a_response, HTTPResponse::HTTP_OK );
                out << "{\"type\":\"failed\",\"ids\":[";
                for ( Queue::MsgIdList_t::iterator i = failed.begin(); i != failed.end(); i++ ) {
                    if ( i != failed.begin() ){
                        out << ",";
                    }
                    out << "\"" << *i << "\"";
                }
                out << "]}";

            } catch( exception & e ) {
                ostream & out = prepResponse( a_response, HTTPResponse::HTTP_BAD_REQUEST );
                out << "{\"type\":\"error\",\"message\":\"" << e.what() << "\"";
            }
        } else {
            prepResponse( a_response, HTTPResponse::HTTP_METHOD_NOT_ALLOWED );
        }
    }

    void EraseFailedRequest( HTTPServerRequest & a_request, HTTPServerResponse & a_response ) {
        if ( a_request.getMethod() == "POST" ) {
            string body( istreambuf_iterator<char>( a_request.stream() ), {});
            libjson::Value req_json;

            try {
                req_json.fromString( body );
                libjson::Value::Array & req_ids = req_json.asArray();
                Queue::MsgIdList_t ids;

                for ( libjson::Value::ArrayIter i = req_ids.begin(); i != req_ids.end(); i++ ) {
                    ids.push_back( i->asString() );
                }

                Queue::MsgIdList_t erased = m_queue.eraseFailed( ids );

                ostream & out = prepResponse( a_response, HTTPResponse::HTTP_OK );
                out << "{\"type\":\"erased\",\"ids\":[";
                for ( Queue::MsgIdList_t::iterator i = erased.begin(); i != erased.end(); i++ ) {
                    if ( i != erased.begin() ){
                        out << ",";
                    }
                    out << "\"" << *i << "\"";
                }
                out << "]}";

            } catch( exception & e ) {
                ostream & out = prepResponse( a_response, HTTPResponse::HTTP_BAD_REQUEST );
                out << "{\"type\":\"error\",\"message\":\"" << e.what() << "\"";
            }
        } else {
            prepResponse( a_response, HTTPResponse::HTTP_METHOD_NOT_ALLOWED );
        }
    }


    ostream & prepResponse( HTTPServerResponse & a_response, HTTPResponse::HTTPStatus a_status ) {
        a_response.setStatus( a_status );
        a_response.setContentType( "json/html" );

        return a_response.send();
    }

    friend QueueServer;

    typedef void (Handler::*Endpoint_t)(HTTPServerRequest &, HTTPServerResponse &);
    typedef map<string,Endpoint_t> RouteMap_t;

    static RouteMap_t m_route_map;

    static void setupRouteMap() {
        if ( !m_route_map.size() ) {
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


QueueServer::QueueServer() : m_queue( 3, 100, 5000, 3 ) {
    try {
        m_queue.setErrorCallback( &logger );

        Handler::setupRouteMap();

        m_server_params = new HTTPServerParams();
        m_server_params->setServerName( "mqserver" );
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
