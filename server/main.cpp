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

std::atomic<int> g_tot_procs{0};
std::atomic<int> g_tot_timeout{0};

struct HandlerStats {
    HandlerStats() : total(0) {}

    std::atomic<int>    total;
    vector<size_t>      worker;
    vector<size_t>      proc_order;
};


map<string,HandlerStats> g_stats;
mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());

void workerThread( Queue & queue, size_t id ) {
    cout << "worker " << id << endl;

    const Queue::Msg_t * msg = &queue.pop();
    size_t ms, loc_count = 0;
    bool cont, fail;
    string msg_id;
    uint64_t msg_tok;
    chrono::time_point<chrono::system_clock> now;

    do {
        // For debug only...
        msg_id = msg->id;
        msg_tok = msg->token;
        now = chrono::system_clock::now();

        if ( msg->id == "exit" ){
            cout << "worker " << id << " exit, count: " << loc_count << endl;
            queue.ack( msg->id, msg->token, true );
            return;
        } else if ( msg->id.size() == 2 && msg->id[0] == '5' ) {
            // Fail these messages
            this_thread::sleep_for(chrono::milliseconds( 2000 ));
            fail = true;
        } else {
            ms = rng() & 0x1F; // 1 - 31 msec delay
            fail = false;
            if ( ms == 0 ) { // when 0, hang to test monitor recovery
                //cout << "worker " << id << " hang" << endl;
                this_thread::sleep_for(chrono::milliseconds( 2000 ));
                fail = true;
            } else {
                this_thread::sleep_for(chrono::milliseconds( ms ));
            }
        }

        // Two worker threads can be processing the same message; however, one will
        // get an exception on ACK b/c it took too long to respond. For this reason
        // processing stats are updated only after a successful ACK

        HandlerStats & stats = g_stats[msg_id];

        if ( stats.total.load() == PROC_COUNT-1 ) {
            cont = false;
        } else {
            cont = true;
        }

        try {
            msg = &queue.popAck( msg_id, msg_tok, cont, (ms & 0xF) == 0xF?5000:0 );
            if ( fail ) {
                cout << "worker " << id << " EXPECTED AN EXCEPTION!!!\n";
                cout << "  msg ID: " << msg_id << ", tok: " << msg_tok << endl;
            }
            stats.total++;
            stats.worker[id]++;
            stats.proc_order.push_back( g_tot_procs );
            loc_count++;
            g_tot_procs++;
        } catch ( exception & e ) {
            //cout << "worker " << id << " popAck exception" << endl;
            if ( !fail ) {
                cout << "worker " << id << " UNEXPECTED EXCEPTION!!! " << e.what() << endl;
                cout << "  msg ID: " << msg_id << ", tok: " << msg_tok << endl;
            }else{
                g_tot_timeout++;
            }
            msg = &queue.pop();
        }
    } while ( true );
}

void printStats(){
    vector<size_t>::iterator i,o;

    for ( map<string,HandlerStats>::iterator m = g_stats.begin(); m != g_stats.end(); m++ ) {
        cout << m->first << " : (" << m->second.total << ") ";
        for ( i = m->second.worker.begin(); i != m->second.worker.end(); i++ ) {
            cout << "  " << *i;
        }
        cout << "\n";

        for ( o = m->second.proc_order.begin(); o != m->second.proc_order.end(); o++ ) {
            cout << "  " << *o;
        }

        cout << "\n";
    }
}

void logger( const string & a_msg ) {
    cout << "[QUEUE] " << a_msg << "\n";
}

int main( int argc, char ** argv ) {
    size_t  i, j;
    Queue   q( 3, 100, 250, 1000 );

    q.setErrorCallback( &logger );

    for ( i = 0; i < MSG_COUNT; i++ ) {
        g_stats[std::to_string(i)].worker.resize(WORKER_COUNT);
        g_stats[std::to_string(i)].proc_order.reserve(PROC_COUNT);
    }

    for ( j = 0; j < 100; j++ ) {
        // Make last 5 medium priority to test boost
        q.push(std::to_string(j),string(), j > 95?1:0);
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
    cout << "Total timeouts: " << g_tot_timeout << "\n";
    cout << "Queue msg count: " << q.count() << "\n";
    cout << "Queue working count: " << q.workingCount() << "\n";
    cout << "Queue failed msg count: " << q.failedCount() << "\n";

    Queue::MsgIdList_t failed = q.getFailed();
    cout << "  failed msg ID count: " << failed.size() << "\n";

    failed = q.eraseFailed( failed );
    cout << "  erased msg count: " << failed.size() << "\n";

    cout << "  new queue msg count: " << q.count() << "\n";

    printStats();
}
