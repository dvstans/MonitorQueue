//#include "QueueServer.hpp"
#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include "Queue.hpp"

#define WORKER_COUNT    8
#define PROC_COUNT      10
#define MSG_COUNT       250

using namespace std;

size_t g_tot_procs = 0;
map<string,vector<size_t>> g_msgs;
mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());

bool
handleMsg( Queue::pMsg_t & msg, size_t id ) {
    g_tot_procs++;
    vector<size_t> & count = g_msgs[msg->id];

    count[id]++;

    if ( ++count[0] == PROC_COUNT ) {
        return false;
    } else {
        return true;
    }
}

void workerThread( Queue & queue, size_t id ) {
    cout << "worker " << id << endl;

    Queue::pMsg_t msg = queue.pop();
    size_t ms, loc_count = 0;
    bool cont;

    do {
        if ( msg->id == "exit" ){
            cout << "worker " << id << " exit, count: " << loc_count << endl;
            queue.ack( msg->id, msg->token, true );
            return;
        }else {
            ms = rng() & 0x1F; // 0 - 32 msec delay
            this_thread::sleep_for(chrono::milliseconds( ms ));
            cont = handleMsg( msg, id );
            loc_count++;
        }
        msg = queue.popAck( msg->id, msg->token, cont );
    } while ( true );
}

void printStats(){
    vector<size_t>::iterator i;
    for ( map<string,vector<size_t>>::iterator m = g_msgs.begin(); m != g_msgs.end(); m++ ) {
        cout << m->first << " :";
        for ( i = m->second.begin(); i != m->second.end(); i++ ) {
            cout << "  " << *i;
        }
        cout << "\n";
    }
}

int main( int argc, char ** argv ) {
    //QueueServer qs;
    //qs.listen();

    size_t  i, j;
    Queue   q( 3, 100 );

    for ( i = 0; i < MSG_COUNT; i++ ) {
        g_msgs[std::to_string(i)].resize(WORKER_COUNT+1);
    }

    //q.push( "1", "data", 0 );
    //Queue::pMsg_t msg = q.pop();
    //cout << "ID: " << msg->id << ", data: " << msg->data << endl;

    for ( j = 0; j < 100; j++ ) {
        q.push(std::to_string(j),string(),0);
    }

    vector<thread*> workers;

    cout << "create workers" << endl;

    for ( i = 0; i < WORKER_COUNT; i++ ) {
        workers.push_back( new thread( workerThread, std::ref(q), i + 1 ));
    }

    while ( j < MSG_COUNT ) {
        if (( i = q.getMessageCount()) < 50 ) {
            cout << "push " << i << ", " << j << endl;
            for ( i = j + 100 - i; j < i && j < MSG_COUNT; j++ ) {
                q.push(std::to_string(j),string(),0);
            }
        }

        this_thread::sleep_for(chrono::milliseconds( 5 ));
    }

    while ( q.getMessageCount() == 100 ) {
        this_thread::sleep_for(chrono::milliseconds( 10 ));
    }

    q.push("exit",string(),0);

    cout << "wait for workers" << endl;

    for ( i = 0; i < WORKER_COUNT; i++ ) {
        workers[i]->join();
    }

    printStats();
}
