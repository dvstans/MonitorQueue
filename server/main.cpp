//#include "QueueServer.hpp"
#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <chrono>
#include "Queue.hpp"

#define WORKER_COUNT    8
#define PROC_COUNT      10
#define MSG_COUNT       10000

using namespace std;

size_t g_tot_procs = 0;
map<string,vector<size_t>> g_msgs;

Queue::MsgState_t
handleMsg( Queue::pMsg_t & msg, size_t id ) {
    g_tot_procs++;
    vector<size_t> & count = g_msgs[msg->id];
    //g_msg[msg->id][0]++;
    //g_msg[msg->id][id]++;

    count[id]++;

    if ( ++count[0] > PROC_COUNT ) {
        return Queue::MSG_DONE;
    } else {
        return Queue::MSG_CONT;
    }
}

void workerThread( Queue & queue, size_t id ) {
    cout << "worker " << id << endl;

    Queue::pMsg_t msg = queue.pop();
    Queue::MsgState_t state = handleMsg( msg, id );

    while ( true ) {
        msg = queue.popAck( msg->id, state );

        if ( msg->id == "exit" ){
            cout << "worker exit " << id << endl;
            queue.ack( msg->id, Queue::MSG_CONT );
            return;
        }else {
            state = handleMsg( msg, id );
        }
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

    //for ( i = 0; i < WORKER_COUNT; i++ ) {
        q.push("exit",string(),0);
    //}

    cout << "wait for workers" << endl;

    for ( i = 0; i < WORKER_COUNT; i++ ) {
        workers[i]->join();
    }
}
