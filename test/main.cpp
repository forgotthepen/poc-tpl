#include "observable/task.hpp"
#include <iostream>
#include <chrono>
#include <exception>


int main(int argc, char* *argv) {
    obs::task tasks{};
    tasks.set_max_workers(6);

    auto t1 = tasks.run([&]{
        tasks.delay(std::chrono::milliseconds(100)).wait();
        for (int iii = 0; iii < 2; iii++) {
            std::cout << ">> hello from task 1 --- @" << (iii + 1) << '\n'
                      << "   my thread id=" << std::this_thread::get_id() << '\n'
                      << '\n';

            tasks.delay(std::chrono::seconds(1)).wait();
        }
        return 7.5;
    });

    auto t2 = tasks.run([&]{
        tasks.delay(std::chrono::milliseconds(200)).wait();
        for (int iii = 0; iii < 2; iii++) {
            std::cout << "## hello from task 2 --- @" << (iii + 1) << '\n'
                      << "   my thread id=" << std::this_thread::get_id() << '\n'
                      << '\n';

            tasks.delay(std::chrono::seconds(1)).wait();
        }

        throw std::runtime_error("bad result!");
        return "oh no!";
    });

    auto t3 = tasks.run([&]{
        t2.get_future().wait();

        tasks.delay(std::chrono::milliseconds(300)).wait();
        for (int iii = 0; iii < 2; iii++) {
            std::cout << "^^ hello from task 3 --- @" << (iii + 1) << '\n'
                      << "   my thread id=" << std::this_thread::get_id() << '\n'
                      << '\n';

            tasks.delay(std::chrono::seconds(1)).wait();
        }
    });


    std::cout << "waiting..." << '\n';
    tasks.when_all(t1, t2, t3);
    
    auto tdel = tasks.delay(std::chrono::seconds(60));
    tasks.run([&]{
        std::cout << " > waiting 2 sec inside task" << '\n';
        tasks.delay(std::chrono::seconds(2)).wait();
        
        std::cout << " > cancelling large delay from task" << '\n';
        tdel.cancel();
    });
    
    std::cout << "waiting for a large delay..." << '\n';
    tdel.wait();
    std::cout << "large delay is finished=" << tdel.is_done() << '\n';

    tasks.when_all(tdel);

    try {
        auto res = t1.result();
        std::cout << "task 1 result=" << res << '\n';
    } catch (std::exception &ex) {
        std::cerr << "ERROR from task 1: " << ex.what() << '\n';
    }

    try {
        auto res = t2.result();
        std::cout << "task 2 result=" << res << '\n';
    } catch (std::exception &ex) {
        std::cerr << "ERROR from task 2: " << ex.what() << '\n';
    }

    try {
        t3.result();
        std::cout << "task 3 finished!" << '\n';
    } catch (std::exception &ex) {
        std::cerr << "ERROR from task 3: " << ex.what() << '\n';
    }

    std::cout << "\nDone... " << '\n';
    return 0;
}
