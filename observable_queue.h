// observable_queue.h
#pragma once
#include <iostream>
#include <vector>
#include <functional>
#include <mutex>
#include <chrono>
#include <atomic>
#include "thread_safe_queue.h"

// 定义 ID 类型
using ObserverId = int64_t;

template<typename T>
class ObservableQueue {
public:
    // 观察者回调函数
    using Observer = std::function<void(const T&)>;

    explicit ObservableQueue(size_t max_size = 100)
        : queue_(new ThreadSafeQueue<T>(max_size)) {}

    // 注册观察者（同步回调）
    ObserverId Subscribe(Observer observer)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ObserverId id = next_id_++; //生成唯一id
        // 存储 ID 和回调函数的配对
        //observers_.push_back({ id, observer }); // 原地构造，
        observers_.emplace_back(id, observer);  // 最明确、最高效的写法
        return id;
    }

    // Unsubscribe 方法，根据 ID 移除观察者
    void Unsubscribe(ObserverId id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        observers_.erase(
            std::remove_if(observers_.begin(), observers_.end(),
                [id](const std::pair<ObserverId, Observer>& item) {
                    return item.first == id;
                }),
            observers_.end()
                    );
    }

    // Push 并通知观察者
    bool Push(T item)
    {
        // 注意：观察者收到的是 item 的拷贝（对 shared_ptr 是安全的）
        T item_copy = item;
        if (!queue_->Push(std::move(item))) return false;

        NotifyObservers(item_copy);
        return true;
    }

    // Push 并通知观察者，带超时
    bool Push(T item, std::chrono::milliseconds timeout)
    {
        // 注意：观察者收到的是 item 的拷贝（对 shared_ptr 是安全的）
        T item_copy = item;
        if (!queue_->Push(std::move(item), timeout)) return false;

        NotifyObservers(item_copy);
        return true;
    }

    bool TryPush(T item)
    {
        // 注意：观察者收到的是 item 的拷贝（对 shared_ptr 是安全的）
        T item_copy = item;
        if (!queue_->TryPush(std::move(item))) return false;

        NotifyObservers(item_copy);
        return true;
    }

    // 阻塞 Pop
    void Pop(T& out_item) {
        queue_->Pop(out_item);
    }

    // 非阻塞 TryPop
    bool TryPop(T& out_item) {
        return queue_->TryPop(out_item);
    }

    void Clear() {
        queue_->Clear();
    }

    size_t Size() const {
        return queue_->Size();

    }

private:
    std::unique_ptr<ThreadSafeQueue<T>> queue_;
    std::vector<std::pair<ObserverId, Observer>> observers_;
    std::atomic<ObserverId> next_id_{ 1 };
    mutable std::mutex mutex_;

    void NotifyObservers(const T& item)
    {
        std::vector<std::pair<ObserverId, Observer>> observers_copy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            observers_copy = observers_;
        }

        for (const auto& pair : observers_copy)
        {
            pair.second(item);
        }
    }

};
