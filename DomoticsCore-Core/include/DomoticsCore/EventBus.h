#pragma once

#include <functional>
#include <vector>
#include <map>
#include <queue>
#include <algorithm>
#include <cassert>
#include <type_traits>
#include <DomoticsCore/Platform_HAL.h>
#include <DomoticsCore/Logger.h>

// Minimal core event enum kept here to avoid extra headers.
namespace DomoticsCore { namespace Utils { enum class EventType : uint8_t { Custom = 1 }; }}

namespace DomoticsCore {
namespace Utils {

/**
 * BUG-41: what one queued event costs the heap. The queue is bounded by these
 * bytes rather than by an entry count, because 32 was only ever a stand-in for
 * them: it exists because an MQTTPublishEvent is 830 B, and applied to a 4-byte
 * event it does not measure the resource it protects.
 *
 * The model has the SHAPE of the allocator, and its constants are platform
 * constants measured on the boards (2026-09-17). That is what makes it safe:
 * the shape being right, each constant holds its measured value and the model
 * is an upper bound by equality on the target that runs it. An unmeasured
 * target takes the conservative set and never aims for equality.
 */
struct QueueCostShape {
#if defined(DOMOTICS_PLATFORM_ESP32)
    static constexpr size_t kNode     = 33;   // deque chunk 512 + 16, over 16 elements
    static constexpr size_t kOverhead = 16;   // TLSF header 4 + light poisoning 12
    static constexpr size_t kSsoChars = 14;   // measured: the step falls between 14 and 15
#elif defined(DOMOTICS_PLATFORM_ESP8266)
    static constexpr size_t kNode     = 29;   // deque chunk 512 over 18 elements, rounded up
    static constexpr size_t kOverhead = 8;    // umm, no poisoning
    static constexpr size_t kSsoChars = 10;   // measured: the step falls between 10 and 11
#else
    // Test host and any target no board has measured: never equality.
    static constexpr size_t kNode     = 80;
    static constexpr size_t kOverhead = 24;
    static constexpr size_t kSsoChars = 10;
#endif
    static constexpr size_t roundUp4(size_t n) { return ((n + 3) / 4) * 4; }
    // No minimum block: the board measurement refuted one on both platforms.
    static constexpr size_t block(size_t n) { return n == 0 ? 0 : kOverhead + roundUp4(n); }
    // Arduino String rounds its buffer to (len + 16) & ~0xf (WString.cpp:193);
    // below the SSO threshold it allocates nothing.
    static constexpr size_t topicBlock(size_t len) {
        return len <= kSsoChars ? 0 : kOverhead + ((len + 16) & ~(size_t)0xf);
    }
};

// Both live at namespace scope rather than inside EventBus, and the derived
// constants one layer out from the shape: a constexpr initialiser cannot call a
// function of a class that is not yet complete, and inside the enclosing class
// the member bodies are not available to constant evaluation either.
struct QueueCost : QueueCostShape {
    static constexpr size_t of(size_t payloadBytes, size_t topicLen) {
        return kNode + block(payloadBytes) + topicBlock(topicLen);
    }
    // The reference is a COMPLETE event, topic term included: without it the
    // budget holds 30 of them rather than 32 and the cliff moves.
    static constexpr size_t kReference   = kNode + block(830) + topicBlock(12);
    // The budget is 32 reference events unless the application says otherwise;
    // -DDOMOTICS_EVENTBUS_QUEUE_BYTES is the only way to move it (BUG-41).
#ifdef DOMOTICS_EVENTBUS_QUEUE_BYTES
    static constexpr size_t kBudgetBytes = (size_t)(DOMOTICS_EVENTBUS_QUEUE_BYTES);
#else
    static constexpr size_t kBudgetBytes = 32 * kReference;
#endif
    static constexpr size_t kMaxEntries  = 256;   // guard rail against model drift
};
static_assert(QueueCost::kBudgetBytes <= UINT16_MAX, "queuedBytes_ is a uint16_t");
// A budget under one reference event refuses every MQTT publish in silence, and a
// budget of zero divides by zero in the high-water mark. Both are reachable only
// through -DDOMOTICS_EVENTBUS_QUEUE_BYTES, so both are refused at compile time.
static_assert(QueueCost::kBudgetBytes >= QueueCost::kReference,
              "DOMOTICS_EVENTBUS_QUEUE_BYTES is below one reference event");

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
    // WARNING: Must not be called during poll() dispatch (single-threaded assumption).
    uint32_t subscribe(EventType type, Handler handler, void* owner = nullptr) {
        assert(!dispatching_ && "Cannot subscribe during EventBus dispatch");
        if (!handler) return 0;
        uint32_t id = nextId++;
        subscriptions[type].push_back({id, owner, std::move(handler)});
        return id;
    }

    // Subscribe to a topic string (e.g., "wifi.connected"). Returns a subscription id.
    // If replayLast is true and a sticky event exists for this topic, the handler is invoked immediately once.
    // WARNING: Must not be called during poll() dispatch (single-threaded assumption).
    uint32_t subscribe(const String& topic, Handler handler, void* owner = nullptr, bool replayLast = false) {
        assert(!dispatching_ && "Cannot subscribe during EventBus dispatch");
        if (!handler || topic.length() == 0) return 0;
        uint32_t id = nextId++;
        if (isWildcard(topic)) {
            wildcardTopicSubscriptions[topic].push_back({id, owner, std::move(handler)});
        } else {
            topicSubscriptions[topic].push_back({id, owner, std::move(handler)});
            if (replayLast) {
                auto it = lastByTopic.find(topic);
                if (it != lastByTopic.end()) {
                    // Avoid duplicate if there are pending queued events for this topic
                    int pending = 0;
                    auto itp = pendingByTopic.find(topic);
                    if (itp != pendingByTopic.end()) pending = itp->second;
                    if (pending <= 0) {
                        const void* payloadPtr = it->second.empty() ? nullptr : it->second.data();
                        // Call immediately
                        auto& vec = topicSubscriptions[topic];
                        for (const auto& sub : vec) {
                            if (sub.id == id && sub.handler) { sub.handler(payloadPtr); break; }
                        }
                    }
                }
            }
        }
        return id;
    }

    // Unsubscribe by id
    // WARNING: Must not be called during poll() dispatch (single-threaded assumption).
    void unsubscribe(uint32_t id) {
        assert(!dispatching_ && "Cannot unsubscribe during EventBus dispatch");
        auto pred = [id](const Subscription& s){ return s.id == id; };
        if (pruneMap(subscriptions, pred)) return;
        if (pruneMap(topicSubscriptions, pred)) return;
        pruneMap(wildcardTopicSubscriptions, pred);
    }

    // Unsubscribe all belonging to a given owner pointer
    // WARNING: Must not be called during poll() dispatch (single-threaded assumption).
    void unsubscribeOwner(void* owner) {
        assert(!dispatching_ && "Cannot unsubscribeOwner during EventBus dispatch");
        if (!owner) return;
        auto pred = [owner](const Subscription& s){ return s.owner == owner; };
        pruneMap(subscriptions, pred);
        pruneMap(topicSubscriptions, pred);
        pruneMap(wildcardTopicSubscriptions, pred);
    }

    // Publish an event with an arbitrary payload type (copy).
    template<typename PayloadT>
    void publish(EventType type, const PayloadT& payload) {
        static_assert(std::is_trivially_copyable<PayloadT>::value,
                      "EventBus payload must be trivially copyable");
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
        // BUG-30: the same guard the EventType overload above has carried since
        // BUG-1, and for the same reason — this reinterpret_cast copies the
        // object's bytes, so a type that owns anything (a String's pointer, length
        // and capacity) is dispatched after the original has been destroyed and
        // its storage freed.
        //
        // It could not be added until there was somewhere for legitimate
        // variable-length callers to go: refusing them with no alternative would
        // have been a worse defect than the one it prevents. publish(topic, const
        // void*, size_t) below is that alternative, and everything that used to
        // need this overload for a String now uses it.
        static_assert(std::is_trivially_copyable<PayloadT>::value,
                      "EventBus payload must be trivially copyable. For a String or "
                      "any other owning type, publish the bytes instead: "
                      "emit(topic, s.c_str(), s.length() + 1, sticky). See BUG-30.");
        if (topic.length() == 0) return;
        QueuedEvent qe;
        qe.topic = topic;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&payload);
        qe.data.assign(p, p + sizeof(PayloadT));
        enqueue(std::move(qe));
    }

    // Topic-based publish with a variable-length payload copy.
    // The caller retains ownership; the queued event owns its byte copy.
    void publish(const String& topic, const void* payload, size_t payloadSize) {
        if (topic.length() == 0 || payload == nullptr || payloadSize == 0) return;
        QueuedEvent qe;
        qe.topic = topic;
        const uint8_t* p = static_cast<const uint8_t*>(payload);
        qe.data.assign(p, p + payloadSize);
        enqueue(std::move(qe));
    }

    // Topic-based publish without payload
    void publish(const String& topic) {
        if (topic.length() == 0) return;
        QueuedEvent qe;
        qe.topic = topic;
        enqueue(std::move(qe));
    }

    // Sticky publish: store last payload for the topic and publish as usual
    template<typename PayloadT>
    void publishSticky(const String& topic, const PayloadT& payload) {
        if (topic.length() == 0) return;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&payload);
        lastByTopic[topic] = std::vector<uint8_t>(p, p + sizeof(PayloadT));
        publish(topic, payload);
    }
    void publishSticky(const String& topic, const void* payload, size_t payloadSize) {
        if (topic.length() == 0 || payload == nullptr || payloadSize == 0) return;
        const uint8_t* p = static_cast<const uint8_t*>(payload);
        lastByTopic[topic] = std::vector<uint8_t>(p, p + payloadSize);
        publish(topic, payload, payloadSize);
    }
    void publishSticky(const String& topic) {
        if (topic.length() == 0) return;
        lastByTopic[topic].clear();
        lastByTopic[topic].shrink_to_fit();
        publish(topic);
    }

    // Dispatch queued events; call from main loop.
    // Single-threaded assumption: handlers must NOT call subscribe/unsubscribe
    // during dispatch. The dispatching_ flag guards this in debug builds.
    void poll(size_t maxPerPoll = 8) {
        size_t processed = 0;
        dispatching_ = true;
        while (!queue.empty() && processed < maxPerPoll) {
            QueuedEvent qe = std::move(queue.front());
            queue.pop();
            queuedBytes_ -= static_cast<uint16_t>(QueueCost::of(qe.data.size(), qe.topic.length()));
            processed++;

            const void* payloadPtr = nullptr;
            if (!qe.data.empty()) payloadPtr = qe.data.data();

            if (qe.topic.length() > 0) {
                // Exact topic subscribers
                auto itT = topicSubscriptions.find(qe.topic);
                if (itT != topicSubscriptions.end()) {
                    const auto& handlers = itT->second;
                    for (const auto& sub : handlers) {
                        if (sub.handler) sub.handler(payloadPtr);
                    }
                }
                // Wildcard subscribers (prefix match e.g., "sensor/*")
                for (const auto& kv : wildcardTopicSubscriptions) {
                    if (matchesWildcard(qe.topic, kv.first)) {
                        const auto& handlers = kv.second;
                        for (const auto& sub : handlers) {
                            if (sub.handler) sub.handler(payloadPtr);
                        }
                    }
                }
                releasePending(qe.topic);
            } else {
                auto it = subscriptions.find(qe.type);
                if (it != subscriptions.end()) {
                    const auto& handlers = it->second;
                    for (const auto& sub : handlers) {
                        if (sub.handler) sub.handler(payloadPtr);
                    }
                }
            }
        }
        dispatching_ = false;
    }

    /** @brief Events dropped on queue overflow since construction or reset() (BUG-36, LO-5). */
    uint32_t getDroppedCount() const { return droppedEvents_; }
    /** @brief Bytes the queue currently holds, as QueueCost models them (BUG-41). */
    uint16_t getQueuedBytes() const { return queuedBytes_; }
    /** @brief Highest occupancy reached since construction or reset(), in percent of the budget. */
    uint8_t getQueueHighWaterPct() const { return highWaterPct_; }

    // Contract: reset() must leave the EventBus in the exact same state
    // as a freshly constructed instance. If you add new members, update this method.
    // Note: dispatching_ is not reset because the assert guarantees it is already false.
    void reset() {
        assert(!dispatching_ && "Cannot reset during EventBus dispatch");
        queue = std::queue<QueuedEvent>();
        subscriptions.clear();
        topicSubscriptions.clear();
        wildcardTopicSubscriptions.clear();
        nextId = 1;
        lastByTopic.clear();
        pendingByTopic.clear();
        droppedEvents_ = 0;
        queuedBytes_ = 0;
        highWaterPct_ = 0;
    }

private:
    void enqueue(QueuedEvent&& qe) {
        // BUG-41: backpressure is a byte budget, not an entry count.
        const size_t cost = QueueCost::of(qe.data.size(), qe.topic.length());
        // An event larger than the whole budget can never be queued: evicting
        // for it would empty the queue and still fail. Name it instead.
        if (cost > QueueCost::kBudgetBytes) {
            ++droppedEvents_;
            DLOG_W(LOG_CORE, "EventBus refused '%s': %u B over a %u B budget",
                   qe.topic.c_str(), (unsigned)cost, (unsigned)QueueCost::kBudgetBytes);
            return;
        }
        while (!queue.empty() && (queuedBytes_ + cost > QueueCost::kBudgetBytes ||
                                  queue.size() >= QueueCost::kMaxEntries)) {
            // BUG-36: the dropped event is still counted as pending for its
            // topic unless it is released here; a stale count blocks the
            // topic's sticky replay for the life of the process.
            const QueuedEvent& front = queue.front();
            queuedBytes_ -= static_cast<uint16_t>(QueueCost::of(front.data.size(), front.topic.length()));
            releasePending(front.topic);
            queue.pop();
            ++droppedEvents_;
        }
        queuedBytes_ += static_cast<uint16_t>(cost);
        const uint8_t pct = static_cast<uint8_t>(static_cast<size_t>(queuedBytes_) * 100u / QueueCost::kBudgetBytes);
        if (pct > highWaterPct_) highWaterPct_ = pct;
        queue.push(std::move(qe));
        // Track pending by topic to help skip duplicate sticky replay
        const QueuedEvent& back = queue.back();
        if (back.topic.length() > 0) {
            pendingByTopic[back.topic] = pendingByTopic[back.topic] + 1;
        }
    }

    void releasePending(const String& topic) {
        if (topic.length() == 0) return;
        auto itp = pendingByTopic.find(topic);
        if (itp == pendingByTopic.end()) return;
        if (itp->second > 1) itp->second -= 1;
        else pendingByTopic.erase(itp);
    }

    static bool isWildcard(const String& topic) {
        // Support prefix wildcard: e.g., "sensor/*"
        int idx = HAL::indexOf(topic, '*');
        return (idx >= 0);
    }

    static bool matchesWildcard(const String& concrete, const String& pattern) {
        int star = HAL::indexOf(pattern, '*');
        if (star < 0) return false; // not a wildcard pattern
        // Allow only prefix+"*" patterns for simplicity
        String prefix = HAL::substring(pattern, 0, star);
        if (star != (int)pattern.length() - 1) {
            // If pattern has chars after '*', require full match (very simple contains)
            String suffix = HAL::substring(pattern, star + 1);
            return HAL::startsWith(concrete, prefix) && HAL::endsWith(concrete, suffix);
        }
        return HAL::startsWith(concrete, prefix);
    }

    template<typename Map, typename Pred>
    static bool pruneMap(Map& m, Pred pred) {
        bool found = false;
        for (auto it = m.begin(); it != m.end(); ) {
            auto& vec = it->second;
            size_t oldSize = vec.size();
            vec.erase(std::remove_if(vec.begin(), vec.end(), pred), vec.end());
            if (vec.size() != oldSize) {
                vec.shrink_to_fit();
                found = true;
            }
            if (vec.empty()) it = m.erase(it);
            else ++it;
        }
        return found;
    }

    // Internal state — if you add a new member, update reset() to clear it.
    std::map<EventType, std::vector<Subscription>> subscriptions;
    std::map<String, std::vector<Subscription>> topicSubscriptions;
    std::map<String, std::vector<Subscription>> wildcardTopicSubscriptions;
    std::queue<QueuedEvent> queue;
    uint32_t nextId;
    // Sticky last payload per topic
    std::map<String, std::vector<uint8_t>> lastByTopic;
    // Pending counts per topic to prevent duplicate sticky replay
    std::map<String, int> pendingByTopic;
    uint32_t droppedEvents_ = 0;   // BUG-36: events popped on overflow, since construction or reset()
    // BUG-41: these two sit before dispatching_ on purpose — after it the uint16_t
    // lands on an odd offset and sizeof(EventBus) grows by four bytes.
    uint16_t queuedBytes_ = 0;
    uint8_t highWaterPct_ = 0;
    bool dispatching_ = false;
};

// BUG-41: queuedBytes_ and highWaterPct_ must land in the padding that already
// followed droppedEvents_. Asserted on the targets a board measured; a new one
// reaching here red is the guard working, not a mistake.
#if defined(DOMOTICS_PLATFORM_ESP32) || defined(DOMOTICS_PLATFORM_ESP8266)
static_assert(sizeof(EventBus) == 172, "EventBus grew: the two counters are not in the padding");
#endif

} // namespace Utils
} // namespace DomoticsCore
