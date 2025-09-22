#pragma once

#include <Arduino.h>
#include <functional>
#include <vector>
#include <map>
#include <queue>
#include <algorithm>
// Minimal core event enum kept here to avoid extra headers.
namespace DomoticsCore { namespace Utils { enum class EventType : uint8_t { Custom = 1 }; }}

namespace DomoticsCore {
namespace Utils {

class EventBus {
public:
    using Handler = std::function<void(const void* /*payload*/)>;

    struct Subscription {
        uint32_t id;
        void* owner; // component or object owning the subscription
        Handler handler;
    };

    struct QueuedEvent {
        // Either a typed event or a topic-based event. If topic is non-empty, it takes precedence.
        EventType type{EventType::Custom};
        String topic{};
        // Copy of payload bytes; we keep a small vector to store arbitrary payloads
        std::vector<uint8_t> data;
    };

    EventBus() : nextId(1) {}

    // Subscribe to an event type. Returns a subscription id.
    uint32_t subscribe(EventType type, Handler handler, void* owner = nullptr) {
        if (!handler) return 0;
        uint32_t id = nextId++;
        subscriptions[type].push_back({id, owner, std::move(handler)});
        return id;
    }

    // Subscribe to a topic string (e.g., "wifi.connected"). Returns a subscription id.
    uint32_t subscribe(const String& topic, Handler handler, void* owner = nullptr) {
        if (!handler || topic.length() == 0) return 0;
        uint32_t id = nextId++;
        topicSubscriptions[topic].push_back({id, owner, std::move(handler)});
        return id;
    }

    // Unsubscribe by id
    void unsubscribe(uint32_t id) {
        for (auto& kv : subscriptions) {
            auto& vec = kv.second;
            vec.erase(std::remove_if(vec.begin(), vec.end(), [id](const Subscription& s){ return s.id == id; }), vec.end());
        }
        for (auto& kv : topicSubscriptions) {
            auto& vec = kv.second;
            vec.erase(std::remove_if(vec.begin(), vec.end(), [id](const Subscription& s){ return s.id == id; }), vec.end());
        }
    }

    // Unsubscribe all belonging to a given owner pointer
    void unsubscribeOwner(void* owner) {
        if (!owner) return;
        for (auto& kv : subscriptions) {
            auto& vec = kv.second;
            vec.erase(std::remove_if(vec.begin(), vec.end(), [owner](const Subscription& s){ return s.owner == owner; }), vec.end());
        }
        for (auto& kv : topicSubscriptions) {
            auto& vec = kv.second;
            vec.erase(std::remove_if(vec.begin(), vec.end(), [owner](const Subscription& s){ return s.owner == owner; }), vec.end());
        }
    }

    // Publish an event with an arbitrary payload type (copy).
    template<typename PayloadT>
    void publish(EventType type, const PayloadT& payload) {
        QueuedEvent qe;
        qe.type = type;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&payload);
        qe.data.assign(p, p + sizeof(PayloadT));
        enqueue(std::move(qe));
    }

    // Publish without payload (or with external storage) — sends nullptr to handlers
    void publish(EventType type) {
        QueuedEvent qe;
        qe.type = type;
        enqueue(std::move(qe));
    }

    // Topic-based publish (with payload copy)
    template<typename PayloadT>
    void publish(const String& topic, const PayloadT& payload) {
        if (topic.length() == 0) return;
        QueuedEvent qe;
        qe.topic = topic;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&payload);
        qe.data.assign(p, p + sizeof(PayloadT));
        enqueue(std::move(qe));
    }

    // Topic-based publish without payload
    void publish(const String& topic) {
        if (topic.length() == 0) return;
        QueuedEvent qe;
        qe.topic = topic;
        enqueue(std::move(qe));
    }

    // Dispatch queued events; call from main loop
    void poll(size_t maxPerPoll = 8) {
        size_t processed = 0;
        while (!queue.empty() && processed < maxPerPoll) {
            QueuedEvent qe = std::move(queue.front());
            queue.pop();
            processed++;

            const void* payloadPtr = nullptr;
            if (!qe.data.empty()) payloadPtr = qe.data.data();

            if (qe.topic.length() > 0) {
                auto itT = topicSubscriptions.find(qe.topic);
                if (itT != topicSubscriptions.end()) {
                    auto handlers = itT->second;
                    for (const auto& sub : handlers) {
                        if (sub.handler) sub.handler(payloadPtr);
                    }
                }
            } else {
                auto it = subscriptions.find(qe.type);
                if (it != subscriptions.end()) {
                    auto handlers = it->second;
                    for (const auto& sub : handlers) {
                        if (sub.handler) sub.handler(payloadPtr);
                    }
                }
            }
        }
    }

    // Optional: clear all
    void reset() {
        while (!queue.empty()) queue.pop();
        subscriptions.clear();
        topicSubscriptions.clear();
        nextId = 1;
    }

private:
    void enqueue(QueuedEvent&& qe) {
        // Basic backpressure: cap queue length
        if (queue.size() < 32) {
            queue.push(std::move(qe));
        } else {
            // drop oldest
            queue.pop();
            queue.push(std::move(qe));
        }
    }

    std::map<EventType, std::vector<Subscription>> subscriptions;
    std::map<String, std::vector<Subscription>> topicSubscriptions;
    std::queue<QueuedEvent> queue;
    uint32_t nextId;
};

} // namespace Utils
} // namespace DomoticsCore
