#ifndef Teensy64_cia1_h_
#define Teensy64_cia1_h_

#include <Arduino.h>

// Joystick bits for both ports
#define CIA1_PR_JOY_UP      0x01
#define CIA1_PR_JOY_DOWN    0x02
#define CIA1_PR_JOY_LEFT    0x04
#define CIA1_PR_JOY_RIGHT   0x08
#define CIA1_PR_JOY_BTN     0x10

// TODO: Paddles

// Lightpen
#define CIA1_PRB_LP 0x10


struct tcia {
    union {
        uint8_t R[0x10];
        uint16_t R16[0x10 / 2];
        uint32_t R32[0x10 / 4];
    };
    union {
        uint8_t W[0x10];
        uint16_t W16[0x10 / 2];
        uint32_t W32[0x10 / 4];
    };
    uint32_t TOD;
    uint32_t TODfrozenMillis;
    uint32_t TODAlarm;
    uint8_t TODstopped;
    uint8_t TODfrozen;
};

void cia1_clock(int clk) __attribute__ ((hot));
void cia1_checkRTCAlarm() __attribute__ ((hot));
void cia1_write(uint32_t address, uint8_t value) __attribute__ ((hot));
uint8_t cia1_read(uint32_t address) __attribute__ ((hot));
void resetCia1(void);


#endif
