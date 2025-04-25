# A proof of concept library for creating tasks on worker threads

Example
```c++
#include "observable/task.hpp"
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <exception>

obs::task tasks{};

int main() {
    auto t1 = tasks.run([]{
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        for (int iii = 0; iii < 4; iii++) {
            std::cout << "## hello from task 1 --- " << (iii + 1) << '\n'
                      << "   my thread id=" << std::this_thread::get_id() << '\n'
                      << '\n';
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        throw std::runtime_error("bad result!");
    });


    std::cout << "Waiting... " << '\n';
    
    try {
        t1.result();
    } catch (std::exception &ex) {
        std::cerr << "ERROR from task: " << ex.what() << '\n';
    }

    std::cout << "\nDone... " << '\n';
    return 0;
}
```
