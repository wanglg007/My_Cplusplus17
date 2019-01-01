//
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/bind.hpp>
#include <iostream>

boost::mutex global_stream_lock;

void WorkerThread(boost::shared_ptr<boost::asio::io_service> iosvc, int counter) {
    global_stream_lock.lock();
    std::cout << "Thread " << counter << " Start.\n";
    global_stream_lock.unlock();

    while (true) {
        try {
            boost::system::error_code ec;
            iosvc->run(ec);
            if (ec) {
                global_stream_lock.lock();
                std::cout << "Error Message: " << ec << ".\n";
                global_stream_lock.unlock();
            }
            break;
        }
        catch (std::exception &ex) {
            global_stream_lock.lock();
            std::cout << "Exception Message: " << ex.what() << ".\n";
            global_stream_lock.unlock();
        }
    }

    global_stream_lock.lock();
    std::cout << "Thread " << counter << " End.\n";
    global_stream_lock.unlock();
}

void ThrowAnException(boost::shared_ptr<boost::asio::io_service> iosvc) {
    global_stream_lock.lock();
    std::cout << "Throw Exception\n";
    global_stream_lock.unlock();

    iosvc->post(boost::bind(&ThrowAnException, iosvc));

    throw (std::runtime_error("The Exception !!!"));
}

int main(void) {
    boost::shared_ptr<boost::asio::io_service> io_svc(
            new boost::asio::io_service
    );

    boost::shared_ptr<boost::asio::io_service::work> worker(
            new boost::asio::io_service::work(*io_svc)
    );

    global_stream_lock.lock();
    std::cout << "The program will exit once all work has finished.\n";
    global_stream_lock.unlock();

    boost::thread_group threads;
    for (int i = 1; i <= 5; i++)
        threads.create_thread(boost::bind(&WorkerThread, io_svc, i));

    io_svc->post(boost::bind(&ThrowAnException, io_svc));

    threads.join_all();

    return 0;
}

/**
 * If we run the program, we will see that it will not exit, and we have to press Ctrl + C to stop the program.
 *
 */