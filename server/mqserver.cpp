#include <chrono>
#include <thread>
#include <QueueServer.hpp>

int main( int arc, char ** argv ) {
    MonQueue::QueueServer mqserver;

    mqserver.start();

    while( true ) {
        std::this_thread::sleep_for( std::chrono::seconds( 2 ));
    }

    return 0;
}
