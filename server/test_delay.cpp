#include <iostream>
#include <string>
#include <map>
#include <thread>
#include <chrono>
#include <mutex>
#include "Queue.hpp"

using namespace std;

map<string,chrono::time_point<chrono::system_clock>> deque_ts;
mutex g_map_mutex;

const size_t WORKER_COUNT = 4;

void workerThread( Queue & queue, size_t id ) {
    cout << "worker " << id << " started\n";

    chrono::time_point<chrono::system_clock> now;
    const Queue::Msg_t * msg = &queue.pop();

    do {
        cout << "MSG " << msg->id << "\n";

        if ( msg->id == "exit" ) {
            break;
        }

        now = chrono::system_clock::now();

        {
            lock_guard<mutex> lock(g_map_mutex);
            deque_ts[msg->id] = now;
        }

        try {
            msg = &queue.popAck( msg->id, msg->token );
        } catch ( exception & e ) {
            // Not expecting any exceptions
            cerr << "Unexpected exception: " << e.what() << endl;
            abort();
        }
    } while ( true );
}

void logger( const string & a_msg ) {
    cerr << "[QUEUE] " << a_msg << "\n";
}

int main( int argc, char ** argv ) {
    size_t  i;
    Queue   q( 3, 100, 250, 1000 );
    vector<thread*> workers;

    q.setErrorCallback( &logger );

    cout << "create workers" << endl;

    for ( i = 0; i < WORKER_COUNT; i++ ) {
        workers.push_back( new thread( workerThread, std::ref(q), i ));
    }


    cout << "MESSAGE DELAY TESTING\n";

    chrono::time_point<chrono::system_clock> start = std::chrono::system_clock::now();

    q.push( "4000", string(), 0, 4000 );
    q.push( "3000", string(), 0, 3000 );
    q.push( "5000", string(), 0, 5000 );
    q.push( "1000", string(), 0, 1000 );
    q.push( "2000", string(), 0, 2000 );

    // Wait for msgs to be processed
    while ( q.count()) {
        this_thread::sleep_for(chrono::milliseconds( 500 ));
    }

    if ( deque_ts.size() != 5 ) {
        cerr << "Invalid result set size" << endl;
        abort();
    }

    // Make sure msg dequeue time matches delay time to within 20 msec
    int ms,diff;
    bool fail = false;
    for ( map<string,chrono::time_point<chrono::system_clock>>::iterator i = deque_ts.begin(); i != deque_ts.end(); i++ ) {
        cout << "ID " << i->first << " TS " << i->second.time_since_epoch().count() << "\n";
        ms = stoi(i->first);
        diff = fabs( chrono::duration_cast<chrono::milliseconds>( i->second - start ).count() - ms );
        if ( diff > 20 ) {
            cerr << "Incorrect delay time for msg " << i->first << ": " << diff << endl;
            fail = true;
        }
    }

    if ( fail ){
        abort();
    }

    q.push( "exit", string(), 0 );

    cout << "Waiting for workers\n";

    for ( i = 0; i < WORKER_COUNT; i++ ) {
        workers[i]->join();
    }
}