#pragma once

#include <atomic>
#include <memory>
#include "common/ring_buffer.hpp"
#include "common/types.hpp"

namespace quant_hub {
namespace execution {

class EventProcessor {
public:
    EventProcessor(size_t bufferSize);
    ~EventProcessor();

    // Event processing
    void start();
    void stop();
    bool publish(const Event& event);
    void subscribe(EventType type, EventHandler handler);
    void unsubscribe(EventType type, const std::string& handlerId);

    // Buffer management
    size_t getBufferSize() const;
    size_t getAvailableSpace() const;
    bool isFull() const;
    bool isEmpty() const;

private:
    void processEvents();
    void clearHandlers();

    std::unique_ptr<RingBuffer<Event>> buffer_;
    std::atomic<bool> running_;
    std::map<EventType, std::vector<EventHandler>> handlers_;
};

} // namespace execution
} // namespace quant_hub
