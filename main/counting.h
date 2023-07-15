#ifndef COUNTING_H
#define COUNTING_H

void IRAM_ATTR outISR(void* arg);
void IRAM_ATTR inISR(void* arg);
void outHandler();
void inHandler();

extern RTC_NOINIT_ATTR bool inFlag, outFlag;
extern RTC_NOINIT_ATTR uint64_t timestampOut,timestampIn;

#endif
