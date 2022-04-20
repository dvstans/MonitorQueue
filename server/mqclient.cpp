#include <stdexcept>
#include <iostream>
#include <chrono>
#include <Poco/URI.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include "libjson.hpp"

using namespace std;
using namespace Poco::Net;

void doRequest( HTTPClientSession& session, HTTPRequest& request, string * body, libjson::Value & reply ) {
    HTTPResponse response;

    request.setContentType( "application/json" );
    request.setContentLength( body?body->size():0 );

    ostream& ws = session.sendRequest(request);

    if ( body ){
        ws << *body;
        //ws.flush();
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
    } catch ( exception & e ) {
        cout << "FAILED - " << e.what() << endl;
    }
}

void doPush( HTTPClientSession & session, size_t offset, size_t count ) {
    libjson::Value reply;
    string body = "[";

    for ( size_t i = 0; i < count; i++ ) {
        if ( i > 0 ) {
            body += ",";
        }
        body += "{\"id\":\"" + to_string( offset + i ) + "\",\"pri\":0}";
    }
    body += "]";

    HTTPRequest request( HTTPRequest::HTTP_POST, "/push", HTTPMessage::HTTP_1_1 );

    doRequest( session, request, &body, reply );
}

void testPush( HTTPClientSession & session, size_t offset, size_t count ){
    cout << "testPush: ";

    try {
        doPush( session, offset, count );

        cout << "OK\n";
    } catch ( ... ) {
        cout << "FAILED\n";
    }
}

void doPop( HTTPClientSession & session, size_t offset, size_t count ){
    HTTPRequest request( HTTPRequest::HTTP_POST, "/pop", HTTPMessage::HTTP_1_1 );

    libjson::Value reply;
    string body, id;
    size_t id_int;
    uint64_t tok;

    for ( size_t i = 0; i < count; i++ ) {
        // Verify ID is within offset -> offset + count

        if ( i == 0 ){
            doRequest( session, request, 0, reply );
            request.setURI( "/pop_ack" );
        } else {
            body = "{\"id\":\"" + id + "\",\"tok\":" + to_string( tok ) + "}";
            doRequest( session, request, &body, reply );
        }

        // Verify reply
        libjson::Value::Object & obj = reply.asObject();
        id = obj.getString( "id" );
        tok = obj.getNumber( "tok" );
        id_int = stoi( id );

        //cout << "i: " << i << ", ID: " << id << endl;

        if ( id_int < offset || id_int >= offset + count ) {
            throw runtime_error( "Message ID received is out of expected range" );
        }
    }

    request.setURI( "/ack" );
    body = "{\"id\":\"" + id + "\",\"tok\":" + to_string( tok ) + "}";
    doRequest( session, request, &body, reply );
}

void testPop( HTTPClientSession & session, size_t offset, size_t count ){
    cout << "testPop: ";

    try {
        doPop( session, offset, count );

        cout << "OK\n";
    } catch ( exception & e ) {
        cout << "FAILED - ";
        cout << e.what() << endl;
    }
}

void testPushPopSpeed( HTTPClientSession & session ){
    cout << "testPushPopSpeed: ";

    try {
        auto t1 = chrono::system_clock::now();

        for ( size_t j = 0; j < 10; j++ ){
            doPush( session, j * 100, 100 );
            doPop( session, j * 100, 100 );
        }

        auto t2 = chrono::system_clock::now();
        chrono::duration<double> diff = t2 - t1;

        cout << diff.count() << " sec, " << (1000/diff.count()) << " req/sec\n";
    } catch ( exception & e ) {
        cout << "FAILED - ";
        cout << e.what() << endl;
    }
}

int main( int argc, char ** argv ) {
    try {
        /*string test = "{\"id\":\"10\",\"tok\":7345895478921745}";
        libjson::Value v;
        v.fromString( test );*/

        Poco::URI uri("http://localhost:8080");
        HTTPClientSession session( uri.getHost(), uri.getPort());

        testCount( session, 0, 0 );
        testPush( session, 0, 100 );
        testCount( session, 100, 0 );
        testPop( session, 0, 100 );
        testCount( session, 0, 0 );
        testPushPopSpeed( session );
    } catch ( const Poco::Exception & e ) {
        cerr << "Poco exception: " << e.displayText() << endl;
        abort();
    } catch ( const exception & e ) {
        cerr << "Exception: " << e.what() << endl;
        abort();
    }

    return 0;
}