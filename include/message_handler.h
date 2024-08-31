#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <mutex>

#include "coke/condition.h"

struct Message {
    std::string data;
    std::size_t offset;
};

class FixedQueue {
public:
    FixedQueue(std::size_t que_size) {
        size = que_size;
        first_off = 0;
        last_off = 0;
        vec.resize(que_size);
    }

    std::size_t get_first_off() const {
        return first_off;
    }

    std::size_t get_last_off() const {
        return last_off;
    }

    void get(std::size_t off, std::size_t max, std::vector<Message> &v) const {
        if (off >= last_off)
            return;

        if (off < first_off)
            off = first_off;

        while (max-- && off < last_off) {
            v.emplace_back(vec[off%size], off);
            ++off;
        }
    }

    void put(std::string &&msg) {
        if (first_off + size == last_off)
            ++first_off;

        vec[last_off%size] = std::move(msg);
        ++last_off;
    }

private:
    std::size_t size;
    std::size_t first_off;
    std::size_t last_off;
    std::vector<std::string> vec;
};

class MessageHandler {
public:
    MessageHandler(std::string topic, std::size_t que_size)
        : topic(topic), que(que_size)
    { }

    std::string get_topic() const {
        return topic;
    }

    void put(std::string &&msg) {
        {
            std::lock_guard<std::mutex> lg(mtx);
            que.put(std::move(msg));
        }

        cv.notify_all();
    }

    bool try_get(std::size_t off, std::size_t max, std::vector<Message> &v) {
        std::lock_guard<std::mutex> lg(mtx);
        if (off >= que.get_last_off())
            return false;

        que.get(off, max, v);
        return true;
    }

    coke::Task<int> get(std::size_t off, std::size_t max, coke::NanoSec nsec,
                        std::vector<Message> &v)
    {
        std::unique_lock<std::mutex> lk(mtx);
        int ret = coke::TOP_SUCCESS;

        if (off >= que.get_last_off()) {
            ret = co_await cv.wait_for(lk, nsec, [this, off]() {
                return off < que.get_last_off();
            });
        }

        if (off >= que.get_last_off())
            co_return ret;

        que.get(off, max, v);
        co_return coke::TOP_SUCCESS;
    }

private:
    std::string topic;
    FixedQueue que;
    std::mutex mtx;
    coke::TimedCondition cv;
};

#endif // MESSAGE_HANDLER_H
