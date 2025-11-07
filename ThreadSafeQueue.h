#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

template<typename T>
class ThreadSafeQueue {
public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Queue.push(std::move(value));
        m_Condition.notify_one();
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_Queue.empty()) {
            return false;
        }
        value = std::move(m_Queue.front());
        m_Queue.pop();
        return true;
    }

    void wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_Condition.wait(lock, [this] { return !m_Queue.empty() || !m_IsActive; });
        if (!m_IsActive && m_Queue.empty()) return;
        value = std::move(m_Queue.front());
        m_Queue.pop();
    }

    void stop() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_IsActive = false;
        m_Condition.notify_all();
    }

private:
    std::queue<T> m_Queue;
    std::mutex m_Mutex;
    std::condition_variable m_Condition;
    bool m_IsActive = true;
};