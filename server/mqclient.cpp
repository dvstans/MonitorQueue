#include <stdexcept>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <Poco/Timespan.h>
#include <Poco/URI.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include "libjson.hpp"

using namespace std;
using namespace Poco::Net;

void doRequest( HTTPClientSession& session, HTTPRequest& request, string * body, libjson::Value & reply ) {
    HTTPResponse response;

    request.setContentLength( body?body->size():0 );
    if ( body ){
        request.setContentType( "application/json" );
    }

    ostream& ws = session.sendRequest(request);

    if ( body ){
        ws << *body;
        ws.flush();
        //cout << "body[" << *body << "]" << endl;
    }

    istream& rs = session.receiveResponse(response);

    //cout << response.getStatus() << " " << response.getReason() << std::endl;

    ssize_t size = response.getContentLength();

    //cout << "reply cont len: " << size << endl;

    if ( size > 0 ) {
        string reply_body;
        reply_body.resize( size );
        rs.read( &reply_body[0], size );

        //cout << "reply[" << reply_body << "]" << endl;

        if ( response.getStatus() != HTTPResponse::HTTP_OK ) {
            throw runtime_error( string("Request failed: ") + reply_body );
        }

        reply.fromString( reply_body );
    } else {
        reply.clear();

        if ( response.getStatus() != HTTPResponse::HTTP_OK ) {
            throw runtime_error( string("Request failed: ") + to_string( response.getStatus() ));
        }
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
        body += "{\"id\":\"";
        body += to_string( offset + i );
        body += "\",\"pri\":0}";
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
    string body, id, tok;
    size_t id_int;

    for ( size_t i = 0; i < count; i++ ) {
        // Verify ID is within offset -> offset + count

        if ( i == 0 ){
            doRequest( session, request, 0, reply );
            request.setURI( "/pop_ack" );
        } else {
            body = "{\"id\":\"";
            body += id;
            body += "\",\"tok\":\"";
            body += tok;
            body += "\"}";

            doRequest( session, request, &body, reply );
        }

        // Verify reply
        libjson::Value::Object & obj = reply.asObject();
        id = obj.getString( "id" );
        tok = obj.getString( "tok" );
        id_int = stoi( id );

        //cout << "i: " << i << ", ID: " << id << endl;

        if ( id_int < offset || id_int >= offset + count ) {
            throw runtime_error( "Message ID received is out of expected range" );
        }
    }

    request.setURI( "/ack" );
    body = "{\"id\":\"" + id + "\",\"tok\":\"" + tok + "\"}";
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

void doGetFailed( HTTPClientSession & session, vector<string> & ids ) {
    libjson::Value reply;

    ids.clear();

    HTTPRequest request( HTTPRequest::HTTP_GET, "/failed", HTTPMessage::HTTP_1_1 );
    doRequest( session, request, 0, reply );

    libjson::Value::Object & obj = reply.asObject();
    libjson::Value::Array & arr = obj.getArray( "ids" );
    for ( libjson::Value::ArrayIter i = arr.begin(); i != arr.end(); i++ ) {
        ids.push_back( i->asString() );
        //cout << " " << i->asString() << endl;
    }
}

void doEraseFailed( HTTPClientSession & session, vector<string> & ids ) {
    libjson::Value reply;
    string body;

    body = "[";
    for ( vector<string>::iterator i = ids.begin(); i != ids.end(); i++ ) {
        body += "\"";
        body += *i;
        body += "\"";
    }
    body += "]";

    HTTPRequest request( HTTPRequest::HTTP_POST, "/failed/erase", HTTPMessage::HTTP_1_1 );
    doRequest( session, request, &body, reply );
}

void testFailureHanding( HTTPClientSession & session ){
    cout << "testFailureHanding: ";
    cout.flush();

    libjson::Value reply;

    try {
        doPush( session, 0, 1 );

        HTTPRequest request( HTTPRequest::HTTP_POST, "/pop", HTTPMessage::HTTP_1_1 );

        // Pop twice, assuming max retries == 1
        for ( int i = 0; i < 2; i++ ) {
            doRequest( session, request, 0, reply );

            // Wait 2 seconds (assuming 1 sec ack timeout)
            this_thread::sleep_for( chrono::seconds( 2 ));
        }

        // Get count and verify 1 failed message
        HTTPRequest count_request( HTTPRequest::HTTP_GET, "/count", HTTPMessage::HTTP_1_1 );
        doRequest( session, count_request, 0, reply );
        libjson::Value::Object & obj = reply.asObject();

        //cout << "act: " << obj.getNumber( "active" ) << ", failed: " << obj.getNumber( "failed" ) << endl;

        if ( obj.getNumber( "failed" ) != 1 ){
            throw runtime_error( "Incorrect failed count" );
        }

        if ( obj.getNumber( "active" ) != 0 ){
            throw runtime_error( "Incorrect active count" );
        }

        vector<string> ids;
        doGetFailed( session, ids );
        doEraseFailed( session, ids );

        doRequest( session, count_request, 0, reply );
        libjson::Value::Object & obj2 = reply.asObject();

        //cout << "act: " << obj2.getNumber( "active" ) << ", failed: " << obj2.getNumber( "failed" ) << endl;

        if ( obj2.getNumber( "failed" ) != 0 ){
            throw runtime_error( "Incorrect failed count after erase" );
        }

        if ( obj2.getNumber( "active" ) != 0 ){
            throw runtime_error( "Incorrect active count after erase" );
        }

        cout << "OK\n";
    } catch ( exception & e ) {
        cout << "FAILED - ";
        cout << e.what() << endl;
    }
}

void testPingSpeed( HTTPClientSession & session ){
    cout << "testPingSpeed: ";
    cout.flush();

    try {
        libjson::Value reply;
        HTTPRequest request( HTTPRequest::HTTP_POST, "/ping", HTTPMessage::HTTP_1_1 );

        auto t1 = chrono::system_clock::now();

        for ( size_t j = 0; j < 5000; j++ ){
            doRequest( session, request, 0, reply );
        }

        auto t2 = chrono::system_clock::now();
        chrono::duration<double> diff = t2 - t1;

        cout << diff.count() << " sec, " << (5000/diff.count()) << " req/sec\n";
    } catch ( exception & e ) {
        cout << "FAILED - ";
        cout << e.what() << endl;
    }
}

void testPushPopSpeed( HTTPClientSession & session ){
    cout << "testPushPopSpeed: ";
    cout.flush();

    try {
        auto t1 = chrono::system_clock::now();

        for ( size_t j = 0; j < 40; j++ ){
            //cout << "push" << endl;
            doPush( session, j * 100, 100 );
            //cout << "pop" << endl;
            doPop( session, j * 100, 100 );
        }

        auto t2 = chrono::system_clock::now();
        chrono::duration<double> diff = t2 - t1;

        cout << diff.count() << " sec, " << (4000/diff.count()) << " req/sec\n";
    } catch ( exception & e ) {
        cout << "FAILED - ";
        cout << e.what() << endl;
    }
}

int main( int argc, char ** argv ) {
    try {
        Poco::URI uri("http://localhost:8080");
        HTTPClientSession session( uri.getHost(), uri.getPort());
        session.setKeepAlive( true );
        session.setKeepAliveTimeout( Poco::Timespan( 5, 0 ));

#if 1
        testCount( session, 0, 0 );
        testPush( session, 0, 100 );
        testCount( session, 100, 0 );
        testPop( session, 0, 100 );
        testCount( session, 0, 0 );
        testFailureHanding( session );
        testPingSpeed( session );
        testPushPopSpeed( session );
#else
        testPingSpeed( session );
        testPushPopSpeed( session );
#endif
    } catch ( const Poco::Exception & e ) {
        cerr << "Poco exception: " << e.displayText() << endl;
        abort();
    } catch ( const exception & e ) {
        cerr << "Exception: " << e.what() << endl;
        abort();
    }

    return 0;
}