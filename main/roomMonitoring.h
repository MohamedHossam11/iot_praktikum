#ifndef ROOMMONITORING_H
#define ROOMMONITORING_H

#include <stdint.h>

#define EXAMPLE_ESP_WIFI_SSID "CAPS-Seminar-Room"
#define EXAMPLE_ESP_WIFI_PASS "caps-schulz-seminar-room-wifi"
#define SNTP_SERVER "ntp1.in.tum.de"

#define interruptPinOut 25   // GPIO25=RTC_GPIO6 Outer barrier interrupt pin, gelb, right outer 4 from top
#define interruptPinIn 26    // GPIO26=RTC_GPIO7 Inner barrier interrupt pin, weiss, right outer 3 from top
#define interruptPinOutRTC 6 // RTC pin number of interruptPinOut
#define interruptPinInRTC 7  // RTC pin number of interruptPinIn
#define DISPLAY_POWER 18     // Provides ePaper display with power
#define CAPACITY_OF_ROOM 30  // Max capacity
#define DEBOUNCING_DELAY 200 // ms
#define RESET_PERIOD 1000    // ms: resets state if no other interrupt happened
#define DEVICE_KEY "..."
#define DEVICE_TOPIC "..."


typedef struct event
{
    uint64_t timeStamp;
    bool outerBarrier;
} event_t;

extern int prediction;
extern RTC_NOINIT_ATTR int count;
extern RTC_NOINIT_ATTR event_t events[200];
extern RTC_NOINIT_ATTR int eventCount;
extern RTC_NOINIT_ATTR uint64_t timeAtStart;
extern RTC_NOINIT_ATTR uint64_t lastDebounceTimeInput1;
extern RTC_NOINIT_ATTR uint64_t lastDebounceTimeInput2;
extern RTC_NOINIT_ATTR int thisIsTheStart;

#define WITH_DISPLAY
// #define WITH_NETWORK
// #define WITH_PUBLICATION
// #define WITH_CALCULATION

// #define LIGHT_SLEEP
// #define DEEP_SLEEP
#define WAKEUP_STUB

#ifdef WAKEUP_STUB
#define DEEP_SLEEP
#endif

#endif
