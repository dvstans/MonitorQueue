#include <stdexcept>
#include <iostream>
#include <Poco/URI.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include "libjson.hpp"

using namespace std;
using namespace Poco::Net;

void doRequest( HTTPClientSession& session, HTTPRequest& request, string * body, libjson::Value & reply ) {
    HTTPResponse response;

    ostream& ws = session.sendRequest(request);
    if ( body ){
        request.setContentType( "application/json" );
        request.setContentLength( body->size() );
        ws << *body;
    }

    istream& rs = session.receiveResponse(response);

    //cout << response.getStatus() << " " << response.getReason() << std::endl;

    string reply_body( istreambuf_iterator<char>( rs ), {});

    //cout << "reply: " << reply_body << endl;

    reply.fromString( reply_body );

    if ( response.getStatus() != HTTPResponse::HTTP_OK ) {
        throw runtime_error( string("Request failed: ") + reply_body );
    }
}

void testCount( HTTPClientSession & session, size_t active, size_t failed ){
    cout << "testCount: ";

    try {
        libjson::Value reply;

        HTTPRequest request( HTTPRequest::HTTP_GET, "/count", HTTPMessage::HTTP_1_1 );

        doRequest( session, request, 0, reply );

        libjson::Value::Object & obj = reply.asObject();

        if ( !obj.has( "type" )) throw runtime_error( "Response missing 'type' field" );
        if ( obj.asString() != "count" ) throw runtime_error( "Response has wrong 'type' value" );
        if ( !obj.has( "capacity" )) throw runtime_error( "Response missing 'capacity' field" );
        if ( obj.type() != libjson::Value::VT_NUMBER ) throw runtime_error( "'capacity' has wrong type" );
        if ( !obj.has( "active" )) throw runtime_error( "Response missing 'active' field" );
        if ( obj.type() != libjson::Value::VT_NUMBER ) throw runtime_error( "'active' has wrong type" );
        if ( obj.asNumber() != active ) throw runtime_error( "'active' has wrong value" );
        if ( !obj.has( "failed" )) throw runtime_error( "Response missing 'failed' field" );
        if ( obj.type() != libjson::Value::VT_NUMBER ) throw runtime_error( "'failed' has wrong type" );
        if ( obj.asNumber() != failed ) throw runtime_error( "'failed' has wrong value" );
        if ( !obj.has( "free" )) throw runtime_error( "Response missing 'free' field" );
        if ( obj.type() != libjson::Value::VT_NUMBER ) throw runtime_error( "'free' has wrong type" );

        cout << "OK\n";
    } catch ( ... ) {
        cout << "FAILED\n";
    }
}

void testPush( HTTPClientSession & session, size_t offset, size_t count ){
    cout << "testPush: ";

    try {
        libjson::Value reply;
        string body = "[";

        for ( size_t i = 0; i < count; i++ ) {
            if ( i > 0 ) {
                body += ",";
            }
            body += "{\"id\":\"" + to_string( offset + count ) + "\",\"pri\":0}";
        }
        body += "]";

        HTTPRequest request( HTTPRequest::HTTP_POST, "/push", HTTPMessage::HTTP_1_1 );

        doRequest( session, request, &body, reply );

        // No reply

        cout << "OK\n";
    } catch ( ... ) {
        cout << "FAILED\n";
    }
}

int main( int argc, char ** argv ) {
    try {
        Poco::URI uri("http://localhost:8080");
        HTTPClientSession session( uri.getHost(), uri.getPort());

        testCount( session, 0, 0 );
        testPush( session, 0, 1 );
        testCount( session, 1, 0 );

    } catch ( const Poco::Exception & e ) {
        cerr << "Poco exception: " << e.displayText() << endl;
        abort();
    } catch ( const exception & e ) {
        cerr << "Exception: " << e.what() << endl;
        abort();
    }

    return 0;
}