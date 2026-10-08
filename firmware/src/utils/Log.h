#pragma once

// Minimal tagged serial logging: LOG("WIFI", "Connected: %s", ip)
// prints "[WIFI] Connected: 192.168.1.42".
//
// Keep normal operation quiet: log state changes, not every poll.

#include <Arduino.h>

#define LOG(tag, fmt, ...) Serial.printf("[" tag "] " fmt "\n", ##__VA_ARGS__)
