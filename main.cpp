#include <functional>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <string>
#include <thread>
#include <map>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <future>
#include <vector>

struct User {
    std::string password;
    std::string school;
    std::string phone;
};

// 使用 shared_mutex 保护共享资源
std::shared_mutex mtx;
std::map<std::string, User> users;
// 修改点：使用 chrono 存储时间点
std::map<std::string, std::chrono::steady_clock::time_point> has_login; 

std::string do_register(std::string username, std::string password, std::string school, std::string phone) {
    std::unique_lock lck(mtx); // 写操作，使用独占锁
    User user = {password, school, phone};
    if (users.emplace(username, user).second)
        return "注册成功";
    else
        return "用户名已被注册";
}

std::string do_login(std::string username, std::string password) {
    std::unique_lock lck(mtx); // 涉及修改 has_login，使用独占锁
    auto now = std::chrono::steady_clock::now();
    
    if (has_login.find(username) != has_login.end()) {
        auto diff = now - has_login.at(username);
        auto sec = std::chrono::duration_cast<std::chrono::seconds>(diff).count();
        return std::to_string(sec) + "秒内登录过";
    }
    
    if (users.find(username) == users.end())
        return "用户名错误";
    if (users.at(username).password != password)
        return "密码错误";
        
    has_login[username] = now;
    return "登录成功";
}

std::string do_queryuser(std::string username) {
    std::shared_lock lck(mtx); // 纯读取，使用共享锁，允许并发读
    if (users.find(username) == users.end()) return "未找到用户";
    
    auto &user = users.at(username);
    std::stringstream ss;
    ss << "用户名: " << username << std::endl;
    ss << "学校:" << user.school << std::endl;
    ss << "电话: " << user.phone << std::endl;
    return ss.str();
}

struct ThreadPool {
    // 使用 vector 存储 future 以便后续 join
    std::vector<std::future<void>> futures;

    void create(std::function<void()> start) {
        // 使用 async 异步执行，launch::async 确保开启新线程
        futures.push_back(std::async(std::launch::async, start));
    }
    
    // 等待所有任务完成
    void wait_all() {
        for (auto &f : futures) {
            f.wait(); 
        }
    }
};

ThreadPool tpool;

namespace test {
std::string username[] = {"张心欣", "王鑫磊", "彭于斌", "胡原名"};
std::string password[] = {"hellojob", "anti-job42", "cihou233", "reCihou_!"};
std::string school[] = {"九百八十五大鞋", "浙江大鞋", "剑桥大鞋", "麻绳理工鞋院"};
std::string phone[] = {"110", "119", "120", "12315"};
}

int main() {
    for (int i = 0; i < 4096; i++) { // 适当减少循环次数以便观察结果
        tpool.create([&] {
            std::cout << do_register(test::username[rand() % 4], test::password[rand() % 4], test::school[rand() % 4], test::phone[rand() % 4]) << std::endl;
        });
        tpool.create([&] {
            std::cout << do_login(test::username[rand() % 4], test::password[rand() % 4]) << std::endl;
        });
        tpool.create([&] {
            // query 前需要确保用户可能已存在，否则 at() 会抛异常
            try {
                std::cout << do_queryuser(test::username[rand() % 4]) << std::endl;
            } catch (...) {}
        });
    }

    // 等待所有线程结束
    tpool.wait_all();
    std::cout << "所有任务已完成。" << std::endl;
    return 0;
}