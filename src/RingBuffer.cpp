#include "RingBuffer.h"
#include <string.h>

static WeatherRecord buffer[MAX_RECORDS];
static uint8_t head = 0;
static uint8_t count = 0;

void ring_init() {
    head = 0;
    count = 0;
    memset(buffer, 0, sizeof(buffer));
}

bool ring_push(const WeatherRecord& rec) {
    buffer[head] = rec;
    head = (head + 1) % MAX_RECORDS;
    if (count < MAX_RECORDS) count++;
    return true;
}

bool ring_latest(WeatherRecord* out) {
    if (count == 0) return false;
    uint8_t idx = (head == 0) ? MAX_RECORDS - 1 : head - 1;
    *out = buffer[idx];
    return true;
}

int ring_get_all(WeatherRecord* out, int max) {
    int n = (count < max) ? count : max;
    uint8_t start = (count < MAX_RECORDS) ? 0 : head;
    for (int i = 0; i < n; i++) {
        out[i] = buffer[(start + i) % MAX_RECORDS];
    }
    return n;
}

int ring_count() {
    return count;
}
