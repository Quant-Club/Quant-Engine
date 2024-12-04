#pragma once

#include <thread>
#include <atomic>
#include <map>
#include <vector>
#include <functional>
#include <condition_variable>
#include "event_processor.hpp"
#include "common/ring_buffer.hpp"
#include "common/logger.hpp"

namespace quant_hub {
namespace execution {

class EventProcessorImpl : public EventProcessor {
public:
    explicit EventProcessorImpl(size_t bufferSize = 1024)
        : buffer_(bufferSize)
        , running_(false)
        , processingThread_()
        , sequenceBarrier_(0)
        , nextSequence_(0)
    {
        LOG_INFO("Initializing event processor with buffer size: ", bufferSize);
    }

    ~EventProcessorImpl() {
        stop();
    }

    void start() override {
        if (running_) return;
        
        running_ = true;
        processingThread_ = std::thread(&EventProcessorImpl::processEvents, this);
        LOG_INFO("Event processor started");
    }

    void stop() override {
        if (!running_) return;
        
        running_ = false;
        condVar_.notify_all();
        
        if (processingThread_.joinable()) {
            processingThread_.join();
        }
        LOG_INFO("Event processor stopped");
    }

    bool publish(const Event& event) override {
        if (!running_) {
            LOG_WARNING("Cannot publish event: processor not running");
            return false;
        }

        // Wait until there's space in the buffer
        while (buffer_.isFull()) {
            std::this_thread::yield();
        }

        if (!buffer_.push(event)) {
            LOG_ERROR("Failed to publish event: buffer full");
            return false;
        }

        condVar_.notify_one();
        return true;
    }

    void subscribe(EventType type, EventHandler handler) override {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        handlers_[type].push_back(handler);
        LOG_INFO("Subscribed handler for event type: ", static_cast<int>(type));
    }

    void unsubscribe(EventType type, const std::string& handlerId) override {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        auto it = handlers_.find(type);
        if (it != handlers_.end()) {
            // Remove handler with matching ID
            // Note: This is a simplified implementation
            handlers_[type].erase(
                std::remove_if(handlers_[type].begin(), handlers_[type].end(),
                    [&handlerId](const EventHandler& h) {
                        return true; // In real implementation, compare handler IDs
                    }), 
                handlers_[type].end());
        }
    }

    size_t getBufferSize() const override {
        return buffer_.capacity();
    }

    size_t getAvailableSpace() const override {
        return buffer_.capacity() - buffer_.size();
    }

    bool isFull() const override {
        return buffer_.isFull();
    }

    bool isEmpty() const override {
        return buffer_.isEmpty();
    }

private:
    void processEvents() {
        Event event;
        while (running_) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condVar_.wait(lock, [this] { 
                    return !running_ || !buffer_.isEmpty(); 
                });

                if (!running_) break;

                if (!buffer_.pop(event)) {
                    continue;
                }
            }

            // Process the event
            processEvent(event);

            // Update sequence
            sequenceBarrier_.store(nextSequence_++, std::memory_order_release);
        }
    }

    void processEvent(const Event& event) {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        auto it = handlers_.find(event.type);
        if (it != handlers_.end()) {
            for (const auto& handler : it->second) {
                try {
                    handler(event);
                } catch (const std::exception& e) {
                    LOG_ERROR("Error processing event: ", e.what());
                }
            }
        }
    }

    RingBuffer<Event> buffer_;
    std::atomic<bool> running_;
    std::thread processingThread_;
    
    std::mutex mutex_;
    std::condition_variable condVar_;
    
    std::mutex handlersMutex_;
    std::map<EventType, std::vector<EventHandler>> handlers_;
    
    std::atomic<uint64_t> sequenceBarrier_;
    uint64_t nextSequence_;
};

} // namespace execution
} // namespace quant_hub
