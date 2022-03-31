//#include "QueueServer.hpp"
#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>
#include "Queue.hpp"

#define WORKER_COUNT    8
#define PROC_COUNT      10
#define MSG_COUNT       250

using namespace std;

//size_t g_tot_procs = 0;
//size_t g_tot_err = 0;
std::atomic<int> g_tot_procs{0};
std::atomic<int> g_tot_err{0};

struct HandlerStats {
    HandlerStats() : total(0) {}

    std::atomic<int>    total;
    vector<size_t>      worker;
};

//map<string,vector<size_t>> g_msgs;
map<string,HandlerStats> g_stats;
mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());

bool
handleMsg( Queue::Msg_t & msg, size_t id ) {
    g_tot_procs++;
    HandlerStats & stats = g_stats[msg.id];

    stats.worker[id]++;

    if ( stats.total.fetch_add(1) == PROC_COUNT-1 ) {
        if ( stats.total != PROC_COUNT ) {
            cout << "ERROR --- STOPPING ON COUNT " << stats.total << endl;
        }
        return false;
    } else {
        return true;
    }
}

void workerThread( Queue & queue, size_t id ) {
    cout << "worker " << id << endl;

    // COPY the message - do NOT hold the pointer as they are shared
    Queue::Msg_t msg = *queue.pop();
    size_t ms, loc_count = 0;
    bool cont, fail;
    string msg_id;
    uint64_t msg_tok;

    do {
        if ( msg.id == "exit" ){
            cout << "worker " << id << " exit, count: " << loc_count << endl;
            queue.ack( msg.id, msg.token, true );
            return;
        }else {
            ms = rng() & 0x1F; // 1 - 31 msec delay
            fail = false;
            if ( ms == 0 ) { // when 0, hang to test monitor recovery
                //cout << "worker " << id << " hang" << endl;
                this_thread::sleep_for(chrono::milliseconds( 2000 ));
                fail = true;
            } else {
                this_thread::sleep_for(chrono::milliseconds( ms ));
                cont = handleMsg( msg, id );
                loc_count++;
            }
        }
        try {
            msg_id = msg.id;
            msg_tok = msg.token;
            msg = *queue.popAck( msg.id, msg.token, cont );
            if ( fail ) {
                cout << "worker " << id << " EXPECTED AN EXCEPTION!!!\n";
                cout << "  msg ID: " << msg_id << ", tok: " << msg_tok << endl;
            }
        } catch ( exception & e ) {
            //cout << "worker " << id << " popAck exception" << endl;
            if ( !fail ) {
                cout << "worker " << id << " UNEXPECTED EXCEPTION!!! " << e.what() << endl;
                cout << "  msg ID: " << msg.id << ", tok: " << msg.token << endl;
            }
            g_tot_err++;
            msg = *queue.pop();
        }
    } while ( true );
}

void printStats(){
    vector<size_t>::iterator i;
    for ( map<string,HandlerStats>::iterator m = g_stats.begin(); m != g_stats.end(); m++ ) {
        cout << m->first << " : (" << m->second.total << ") ";
        for ( i = m->second.worker.begin(); i != m->second.worker.end(); i++ ) {
            cout << "  " << *i;
        }
        cout << "\n";
    }
}

int main( int argc, char ** argv ) {
    //QueueServer qs;
    //qs.listen();

    size_t  i, j;
    Queue   q( 3, 100, 250, 1000 );

    for ( i = 0; i < MSG_COUNT; i++ ) {
        g_stats[std::to_string(i)].worker.resize(WORKER_COUNT);
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
        workers.push_back( new thread( workerThread, std::ref(q), i ));
    }

    while ( j < MSG_COUNT ) {
        if (( i = q.freeCount()) > 20 ) {
            cout << "push " << i << ", " << j << endl;
            for ( i = j + i; j < i && j < MSG_COUNT; j++ ) {
                q.push(std::to_string(j),string(),0);
            }
        }

        this_thread::sleep_for(chrono::milliseconds( 5 ));
    }

    while ( q.workingCount()) {
        this_thread::sleep_for(chrono::milliseconds( 500 ));
    }

    q.push("exit",string(),0);

    cout << "wait for workers" << endl;

    for ( i = 0; i < WORKER_COUNT; i++ ) {
        workers[i]->join();
    }

    cout << "Total msg processed: " << g_tot_procs << "\n";
    cout << "Total errors: " << g_tot_err << "\n";
    cout << "Queue msg count: " << q.count() << "\n";
    cout << "Queue working count: " << q.workingCount() << "\n";
    cout << "Queue failed msg count: " << q.failedCount() << "\n";

    printStats();
}
