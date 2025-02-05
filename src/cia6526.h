/*
  Copyright Frank Bösing, Karsten Fleischer, 2017 - 2023

	This file is part of Teensy64.

    Teensy64 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Teensy64 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Teensy64.  If not, see <http://www.gnu.org/licenses/>.

    Diese Datei ist Teil von Teensy64.

    Teensy64 ist Freie Software: Sie können es unter den Bedingungen
    der GNU General Public License, wie von der Free Software Foundation,
    Version 3 der Lizenz oder (nach Ihrer Wahl) jeder späteren
    veröffentlichten Version, weiterverbreiten und/oder modifizieren.

    Teensy64 wird in der Hoffnung, dass es nützlich sein wird, aber
    OHNE JEDE GEWÄHRLEISTUNG, bereitgestellt; sogar ohne die implizite
    Gewährleistung der MARKTFÄHIGKEIT oder EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.
    Siehe die GNU General Public License für weitere Details.

    Sie sollten eine Kopie der GNU General Public License zusammen mit diesem
    Programm erhalten haben. Wenn nicht, siehe <http://www.gnu.org/licenses/>.

*/

#pragma once

#ifndef TEENSY64_CIA6526_H
#define TEENSY64_CIA6526_H

#include <cstdint>

static inline uint8_t decToBcd(uint8_t x) {
    return ((x / 10 * 16) | (x % 10));
}

static inline uint8_t bcdToDec(uint8_t x) {
    return ((x / 16 * 10) | (x % 16));
}

#define CIA_PRA         0x00
#define CIA_PRB         0x01
#define CIA_DDRA        0x02
#define CIA_DDRB        0x03
#define CIA_TALO        0x04
#define CIA_TAHI        0x05
#define CIA_TBLO        0x06
#define CIA_TBHI        0x07
#define CIA_TOD10TH     0x08
#define CIA_TODSEC      0x09
#define CIA_TODMIN      0x0a
#define CIA_TODHR       0x0b
#define CIA_SDR         0x0c
#define CIA_ICR         0x0d
#define CIA_CRA         0x0e
#define CIA_CRB         0x0f
#define CIA_NUMREGS     0x10

// underflow signals when CIA_CR_PBON has been set
#define CIA_PRB_TA  0x40
#define CIA_PRB_TB  0x80

// TOD
#define CIA_TOD10TH_MASK    0x0f
#define CIA_TODSEC_MASK     0x7f
#define CIA_TODMIN_MASK     0x7f
#define CIA_TODHR_MASK      0x9f    // hours including PM flag
#define CIA_TODHR_HR_MASK   0x1f    // only hours
#define CIA_TODHR_PM        0x80    // only PM flag

// interrupt control register bits
#define CIA_ICR_IRQ_MASK    0x1f
#define CIA_ICR_TA          0x01
#define CIA_ICR_TB          0x02
#define CIA_ICR_ALRM        0x04
#define CIA_ICR_SP          0x08    // serial port
#define CIA_ICR_FLG         0x10    // FLAG data line
#define CIA_ICR_IR          0x80    // read
#define CIA_ICR_SET         0x80    // write
#define CIA_ICR_READ_MASK   (CIA_ICR_IRQ_MASK | CIA_ICR_IR)

// timer control registers
// CR bits that have same meaning for CRA and CRB
#define CIA_CR_START    0x01    // 1 = START TIMER, 0 = STOP TIMER, CRA: Timer A, CRB: Timer B
#define CIA_CR_PBON     0x02    // 1 = TIMERx output appears on PBy, 0 = PBy normal operation
                                // CRA: x=A, y=6, CRB: x=B, y=7
#define CIA_CR_OUTMODE  0x04    // 1 = TOGGLE, 0 = PULSE
#define CIA_CR_RUNMODE  0x08    // 1 = ONE-SHOT, 0 = CONTINOUS
#define CIA_CR_LOAD     0x10    // 1 = FORCE LOAD

// values for masked CIA_CR_OUTMODE
#define CIA_CR_OUTMODE_TOGGLE   0x04
#define CIA_CR_OUTMODE_PULSE    0x00

// values for masked CIA_CR_RUNMODE
#define CIA_CR_RUNMODE_ONESHOT  0x08
#define CIA_CR_RUNMODE_CONT     0x00

// bits special to CRA
#define CIA_CRA_INMODE  0x20    // 1 = TIMER A counts positive CNT transitions, 0 = TIMER A counts phi2 pulses.
#define CIA_CRA_SPMODE  0x40    // 1 = SERIAL PORT output (CNT sources shift clock).
                                // 0 = SERIAL PORT input (external shift clock required).
#define CIA_CRA_TODIN   0x80    // 1 = 50Hz clock required on TOD pin for accurate time.
                                // 0 = 60Hz clock required on TOD pin for accurate time.

// bits special to CRB
#define CIA_CRB_INMODE_MASK 0x60
#define CIA_CRB_INMODE_PHI2 0x00    // TIMER B counts phi2 pulses.
#define CIA_CRB_INMODE_CNT  0x20    // TIMER B counts positive CNT transitions.
#define CIA_CRB_INMODE_TA   0x40    // TIMER B counts TIMER A underflow pulses.
#define CIA_CRB_INMODE_TACH 0x60    // TIMER B counts TIMER A underflow pulses while CNT is high.
#define CIA_CRB_ALARM       0x80    // 1 = writing to TOD registers sets ALARM.
                                    // 0 = writing to TOD registers sets TOD clock.

class CIA6526 {
protected:
    const uint32_t DAY_IN_MILLISECONDS = 1000 * 60 * 60 * 24;

    union {
        uint8_t R[CIA_NUMREGS];
        struct {
            union {
                uint8_t PRA;
                struct {
                    uint8_t PA0 : 1;
                    uint8_t PA1 : 1;
                    uint8_t PA2 : 1;
                    uint8_t PA3 : 1;
                    uint8_t PA4 : 1;
                    uint8_t PA5 : 1;
                    uint8_t PA6 : 1;
                    uint8_t PA7 : 1;
                };
            };
            union {
                uint8_t PRB;
                struct {
                    uint8_t PB0 : 1;
                    uint8_t PB1 : 1;
                    uint8_t PB2 : 1;
                    uint8_t PB3 : 1;
                    uint8_t PB4 : 1;
                    uint8_t PB5 : 1;
                    uint8_t PB6 : 1;
                    uint8_t PB7 : 1;
                };
            };
            union {
                uint8_t DDRA;
                struct {
                    uint8_t DPA0 : 1;
                    uint8_t DPA1 : 1;
                    uint8_t DPA2 : 1;
                    uint8_t DPA3 : 1;
                    uint8_t DPA4 : 1;
                    uint8_t DPA5 : 1;
                    uint8_t DPA6 : 1;
                    uint8_t DPA7 : 1;
                };
            };
            union {
                uint8_t DDRB;
                struct {
                    uint8_t DPB0 : 1;
                    uint8_t DPB1 : 1;
                    uint8_t DPB2 : 1;
                    uint8_t DPB3 : 1;
                    uint8_t DPB4 : 1;
                    uint8_t DPB5 : 1;
                    uint8_t DPB6 : 1;
                    uint8_t DPB7 : 1;
                };
            };
            union {
                uint16_t TA;
                struct {
                    uint8_t TALO;
                    uint8_t TAHI;
                };
            };
            union {
                uint16_t TB;
                struct {
                    uint8_t TBLO;
                    uint8_t TBHI;
                };
            };
            struct {
                uint8_t TOD10TH : 4;
                uint8_t : 4;
            };
            struct {
                uint8_t TODSEC : 7;
                uint8_t : 1;
            };
            struct {
                uint8_t TODMIN : 7;
                uint8_t : 1;
            };
            uint8_t TODHR;
            uint8_t SDR;
            union {
                uint8_t ICR;
                union {
                    struct {
                        uint8_t TA : 1;
                        uint8_t TB : 1;
                        uint8_t ALRM : 1;
                        uint8_t SP : 1;
                        uint8_t FLG : 1;
                        uint8_t : 2;
                        uint8_t IR : 1;
                    } ICRDATA;
                };
            };
            union {
                uint8_t CRA;
                struct {
                    uint8_t START : 1;
                    uint8_t PBON : 1;
                    uint8_t OUTMODE : 1;
                    uint8_t RUNMODE : 1;
                    uint8_t LOAD : 1;
                    uint8_t INMODE : 1;
                    uint8_t SPMODE : 1;
                    uint8_t TODIN : 1;
                } A;
            };
            union {
                uint8_t CRB;
                struct {
                    uint8_t START : 1;
                    uint8_t PBON : 1;
                    uint8_t OUTMODE : 1;
                    uint8_t RUNMODE : 1;
                    uint8_t LOAD : 1;
                    uint8_t INMODE : 2;
                    uint8_t ALARM : 1;
                } B;
            };
        } r;
    };
    union {
        struct {
            union {
                uint16_t TA;
                struct {
                    uint8_t TALO;
                    uint8_t TAHI;
                };
            };
            union {
                uint16_t TB;
                struct {
                    uint8_t TBLO;
                    uint8_t TBHI;
                };
            };
            struct {
                uint8_t TOD10TH : 4;
                uint8_t : 4;
            };
            struct {
                uint8_t TODSEC : 7;
                uint8_t : 1;
            };
            struct {
                uint8_t TODMIN : 7;
                uint8_t : 1;
            };
            struct {
                uint8_t TODHR : 5;
                uint8_t : 2;
                uint8_t TODPM : 1;
            };
            union {
                uint8_t ICR;
                union {
                    struct {
                        uint8_t TA : 1;
                        uint8_t TB : 1;
                        uint8_t ALRM : 1;
                        uint8_t SP : 1;
                        uint8_t FLG : 1;
                        uint8_t : 2;
                        uint8_t S_C : 1;
                    } ICRMASK;
                };
            };
        } latch;
    };
    uint32_t TOD;
    uint32_t TODfrozenMillis;
    uint32_t TODalarm;
    bool TODstopped;
    bool TODfrozen;

    void setAlarmTime();
    uint32_t tod() const;

public:
    CIA6526() = default;
    virtual ~CIA6526() = default;

    void clock(uint16_t clk) __attribute__ ((hot));
    void checkRTCAlarm() __attribute__ ((hot));
    virtual void write(uint32_t address, uint8_t value) __attribute__ ((hot));
    virtual uint8_t read(uint32_t address) __attribute__ ((hot));
    void reset();
};

#endif //TEENSY64_CIA6526_H
