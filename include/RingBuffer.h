#pragma once
#include <Arduino.h>
#include <mutex>
template <typename T, size_t N, const bool NEED_LOCK>
class RingBuffer {
public:
    RingBuffer() : _head(0), _tail(0) {}

    // Add element to "list"
    // Returns: true = success, false = buffer was empty
    bool push(const T& item) {
        if (NEED_LOCK) { noInterrupts(); } // critical section
        size_t nextHead = (_head + 1) % N;
        if (nextHead == _tail) {
            if (NEED_LOCK) { interrupts(); }
            return false; // buffer full
        }
        _items[_head] = item;
        _head = nextHead;
        if (NEED_LOCK) { interrupts(); }
        return true;
    }

#if 0
    // Get current item (item is not removed)
    // Returns: true = success, false = buffer was empty
    bool peek(T& item) const {
        if (NEED_LOCK) { noInterrupts(); }
        if (_head == _tail) {
            if (NEED_LOCK) { interrupts(); }
            return false; // empty
        }

        item = _items[_tail];
        if (NEED_LOCK) { interrupts(); }
        return true;
    }
#endif

    // Get current item and remove it
    // Returns: true = success, false = buffer was empty
    bool pop(T& item) {
        if (NEED_LOCK) { noInterrupts(); }
        if (_head == _tail) {
            if (NEED_LOCK) { interrupts(); }
            return false; // empty
        }
        item = _items[_tail];
        _tail = (_tail + 1) % N;
        if (NEED_LOCK) { interrupts(); }
        return true;
    }

    // Return current number of items
    size_t count() const {
        if (NEED_LOCK) { noInterrupts(); }
        size_t count = (_head >= _tail) ? (_head - _tail) : (N - _tail + _head);
        if (NEED_LOCK) { interrupts(); }
        return count;
    }

    size_t available() const {
        return N - count();
    }

    size_t size() const {
        return N;
    }

    void clear() {
        if (NEED_LOCK) { noInterrupts(); }
        _head = 0;
        _tail = 0;
        if (NEED_LOCK) { interrupts(); }
    }

private:
    T _items[N];
    volatile size_t _head;
    volatile size_t _tail;
};



# if 0
// Sample usage

struct Event {
    uint8_t type;
    uint32_t timestamp;
};
RingBuffer<Event, 16> eventQueue;

// Add item (e.g. in a SDK callback funtion)
Event e = {1, millis()};
eventQueue.push(e);

// Read item (not removing it)
Event first;
if (eventQueue.peek(first)) {
    Serial.println(first.type);
}

// Read item (removing it)
Event removed;
if (eventQueue.pop(removed)) {
    Serial.println(removed.timestamp);
}

#endif


/* vim:set ts=4 et: */
