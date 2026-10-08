#pragma once

#include <mutex>

#include "GrpcWebTransport.h"
#include "StarlinkClient.h"
#include "StarlinkSnapshot.h"
#include "StarlinkStatus.h"

namespace starlink {

// Polls the dish on the network task with retry/backoff and publishes
// thread-safe snapshots.
class StarlinkService {
public:
    StarlinkService() : _client(_transport) {}

    // Network task only. `networkUp`: station connected and not in setup.
    void loop(uint32_t now, bool networkUp, const char* host);

    StarlinkSnapshot snapshot() const;  // any task

private:
    void poll(uint32_t now);
    void publish();  // also re-evaluates health and logs changes

    GrpcWebTransport _transport;
    StarlinkClient _client;

    // Network-task state
    LinkState _state = LinkState::Waiting;
    char _host[16] = "";
    uint32_t _nextPoll = 0;
    uint16_t _failures = 0;
    const char* _lastError = "";
    ClientResult _lastResult = ClientResult::Ok;
    Health _health;
    StarlinkStatus _last;
    bool _hasStatus = false;
    uint32_t _lastOkMs = 0;
    uint32_t _lastRttMs = 0;
    uint32_t _lastSummaryMs = 0;
    uint32_t _pollsSinceSummary = 0;

    mutable std::mutex _mutex;
    StarlinkSnapshot _snapshot;  // guarded by _mutex
};

}  // namespace starlink
