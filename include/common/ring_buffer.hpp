#pragma once

#include <atomic>
#include <array>
#include <memory>
#include <cassert>

namespace quant_hub {

template<typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t size)
        : size_(size)
        , buffer_(std::make_unique<T[]>(size))
        , readIndex_(0)
        , writeIndex_(0)
    {
        assert(size > 0 && "Buffer size must be positive");
    }

    bool push(const T& item) {
        size_t currentWrite = writeIndex_.load(std::memory_order_relaxed);
        size_t nextWrite = (currentWrite + 1) % size_;
        
        if (nextWrite == readIndex_.load(std::memory_order_acquire)) {
            return false;  // Buffer is full
        }

        buffer_[currentWrite] = item;
        writeIndex_.store(nextWrite, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t currentRead = readIndex_.load(std::memory_order_relaxed);
        
        if (currentRead == writeIndex_.load(std::memory_order_acquire)) {
            return false;  // Buffer is empty
        }

        item = buffer_[currentRead];
        readIndex_.store((currentRead + 1) % size_, std::memory_order_release);
        return true;
    }

    bool peek(T& item) const {
        size_t currentRead = readIndex_.load(std::memory_order_relaxed);
        
        if (currentRead == writeIndex_.load(std::memory_order_acquire)) {
            return false;  // Buffer is empty
        }

        item = buffer_[currentRead];
        return true;
    }

    bool isEmpty() const {
        return readIndex_.load(std::memory_order_acquire) == 
               writeIndex_.load(std::memory_order_acquire);
    }

    bool isFull() const {
        size_t nextWrite = (writeIndex_.load(std::memory_order_acquire) + 1) % size_;
        return nextWrite == readIndex_.load(std::memory_order_acquire);
    }

    size_t size() const {
        size_t read = readIndex_.load(std::memory_order_acquire);
        size_t write = writeIndex_.load(std::memory_order_acquire);
        
        if (write >= read) {
            return write - read;
        } else {
            return size_ - (read - write);
        }
    }

    size_t capacity() const {
        return size_ - 1;  // One slot is always kept empty
    }

private:
    const size_t size_;
    std::unique_ptr<T[]> buffer_;
    std::atomic<size_t> readIndex_;
    std::atomic<size_t> writeIndex_;
};

} // namespace quant_hub
