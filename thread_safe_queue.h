// thread_safe_queue.h
#pragma once
#include <queue>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>

template<typename T>
class ThreadSafeQueue {
public:
    explicit ThreadSafeQueue(size_t max_size = 100)
        : max_size_(max_size) {}

    // Push 一个 item，无限等待
    bool Push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        //if (queue_.size() >= max_size_) return false;
        cv_push_.wait(lock, [this] {return queue_.size() < max_size_; });
        //std::cout << "queue size : " << queue_.size() << std::endl;
        queue_.push(std::move(item));
        cv_pop_.notify_one();
        return true;
    }

    // Push 一个 item，超时返回
    bool Push(T item, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        //if (queue_.size() >= max_size_) return false;
        cv_push_.wait_for(lock, timeout, [this] {return queue_.size() < max_size_; });
        //std::cout << "queue size : " << queue_.size() << std::endl;
        queue_.push(std::move(item));
        cv_pop_.notify_one();
        return true;
    }

    // 阻塞式 Pop：等待直到有数据
    void Pop(T& out_item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_pop_.wait(lock, [this] { return !queue_.empty(); });
        out_item = std::move(queue_.front());
        queue_.pop();
        cv_push_.notify_one();
    }

    bool TryPush(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= max_size_) return false;
        queue_.push(std::move(item));
        cv_pop_.notify_one();
        return true;
    }

    // 非阻塞 TryPop：有就取，没有返回 false
    bool TryPop(T& out_item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        out_item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            queue_.pop();
        }
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    //std::condition_variable cv_;
    std::condition_variable cv_push_;
    std::condition_variable cv_pop_;
    const size_t max_size_;
};
