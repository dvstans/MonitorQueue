#include <iostream>
#include <chrono>
#include <thread>
#include <QueueServer.hpp>
#include <boost/program_options.hpp>

using namespace std;
namespace po = boost::program_options;

const char * VERSION = "1.0";

int main( int a_argc, char ** a_argv ) {
    uint16_t port = 8080;
    uint8_t priority_count = 3;
    size_t msg_capacity = 100;
    size_t msg_ack_timeout_msec = 60000;
    size_t msg_max_retries = 5;
    size_t msg_boost_timeout_msec = 300000;
    size_t monitor_period_msec = 5000;

    po::options_description opts( "Options" );

    opts.add_options()
        ("help,?", "Show help")
        ("version,v", "Show version number")
        ("port,P",po::value<uint16_t>( &port ),"Port number")
        ("priorities,p",po::value<uint8_t>( &priority_count ),"Number of priorities")
        ("capacity,c",po::value<size_t>( &msg_capacity ),"Message capacity")
        ("ack-timeout,a",po::value<size_t>( &msg_ack_timeout_msec ),"Client ack timeout (msec)")
        ("max-retries,r",po::value<size_t>( &msg_max_retries ),"Max retries before fail")
        ("boost-timeout,b",po::value<size_t>( &msg_boost_timeout_msec ),"Priority boost timeout (msec)")
        ("monitor-period,m",po::value<size_t>( &monitor_period_msec ),"Client monitor poll period (msec)")
        ;

    try {
        po::variables_map opt_map;
        po::store( po::command_line_parser( a_argc, a_argv ).options( opts ).run(), opt_map );
        po::notify( opt_map );

        if ( opt_map.count( "help" ) )
        {
            cout << "Monitor Queue Server, ver. " << VERSION << "\n";
            cout << "Usage: mqserver [options]\n";
            cout << opts << endl;
            return 0;
        }

        if ( opt_map.count( "version" ))
        {
            cout << VERSION << endl;
            return 0;
        }
    }
    catch( po::unknown_option & e )
    {
        cerr << "Options error: " << e.what() << "\n";
        return 1;
    }

    MonQueue::QueueServer mqserver(
        priority_count,
        msg_capacity,
        msg_ack_timeout_msec,
        msg_max_retries,
        msg_boost_timeout_msec,
        monitor_period_msec
    );

    mqserver.start();

    while( true ) {
        std::this_thread::sleep_for( std::chrono::seconds( 2 ));
    }

    return 0;
}
