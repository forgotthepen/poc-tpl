#include "observable/task.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <exception>


int main(int argc, char* *argv) {
    obs::task tasks{};

    auto t1 = tasks.run([]{
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        for (int iii = 0; iii < 4; iii++) {
            std::cout << ">> hello from task 1 --- @" << (iii + 1) << '\n'
                      << "   my thread id=" << std::this_thread::get_id() << '\n'
                      << '\n';
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        return 7.5;
    });

    auto t2 = tasks.run([]{
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        for (int iii = 0; iii < 4; iii++) {
            std::cout << "## hello from task 2 --- @" << (iii + 1) << '\n'
                      << "   my thread id=" << std::this_thread::get_id() << '\n'
                      << '\n';
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        throw std::runtime_error("bad result!");
        return "oh no!";
    });

    auto t3 = tasks.run([&]{
        t2.get_future().wait();

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        for (int iii = 0; iii < 4; iii++) {
            std::cout << "^^ hello from task 3 --- @" << (iii + 1) << '\n'
                      << "   my thread id=" << std::this_thread::get_id() << '\n'
                      << '\n';
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });


    std::cout << "waiting..." << '\n';
    tasks.when_all(t1, t2, t3);

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
