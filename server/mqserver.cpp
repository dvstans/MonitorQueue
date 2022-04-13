#include <chrono>
#include <thread>
#include <QueueServer.hpp>

//#include <iostream>
//#include <unistd.h>

int main( int arc, char ** argv ) {
    //std::cout << "pid: " << getpid() << std::endl;

    MonQueue::QueueServer mqserver;

    mqserver.start();

    while( true ) {
        std::this_thread::sleep_for( std::chrono::seconds( 2 ));
    }

    return 0;
}
