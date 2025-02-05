/*
  Copyright Frank Bösing, 2017

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

#ifndef TEENSY64_TEENSY64_H
#define TEENSY64_TEENSY64_H

#include <Arduino.h>
#include <DMAChannel.h>
#include <SdFat.h>
#include <reSID.h>

#include "settings.h"

#ifdef VERSION
#undef VERSION
#endif

#define VERSION "10"

#include "ILI9341_t3n.h"
#include "keyboard_usb.h"

extern ILI9341_t3n tft;
extern USBHost myusb;

void initMachine();
void resetMachine() __attribute__ ((noreturn));
void resetExternal();

extern bool SDinitialized;

#if PAL == 1

static const float CRYSTAL = 17734475.0f;
static const float CLOCKSPEED = CRYSTAL / 18.0f; // 985248,61 Hz

static const unsigned int CYCLESPERRASTERLINE = 63;
static const unsigned int LINECNT = 312;
static const unsigned int VBLANK_FIRST = 300;
static const unsigned int VBLANK_LAST = 15;

#else // PAL == 1

static const float CRYSTAL = 14318180.0f;
static const float CLOCKSPEED = CRYSTAL / 14.0f; // 1022727,14 Hz

static const unsigned int CYCLESPERRASTERLINE = 64;
static const unsigned int LINECNT = 263; //Rasterlines
static const unsigned int VBLANK_FIRST = 13;
static const unsigned int VBLANK_LAST = 40;

#endif // PAL == 1

static const float LINEFREQ = CLOCKSPEED / CYCLESPERRASTERLINE;
static const float REFRESHRATE = LINEFREQ / LINECNT; //Hz
static const float LINETIMER_DEFAULT_FREQ = 1000000.0f/LINEFREQ;
static const float MCU_C64_RATIO= F_CPU / CLOCKSPEED; //MCU Cycles per C64 Cycle
static const float US_C64_CYCLE = 1000000.0f / CLOCKSPEED; // Duration (µs) of a C64 Cycle
static const float AUDIOSAMPLERATE =  LINEFREQ * 2; // (~32kHz)

static const unsigned int ISR_PRIORITY_RASTERLINE = 255;

//Pins

#define LED_INIT()  pinMode(13,OUTPUT)
#define LED_ON()    digitalWriteFast(13,1)
#define LED_OFF()   digitalWriteFast(13,0)
#define LED_TOGGLE  {GPIOC_PTOR=32;} // This toggles the Teensy Builtin LED pin 13

//ILI9341

#define TFT_DC          20
#define TFT_CS          21
#define TFT_RST         255  // 255 = unused, connected to 3.3V
#define TFT_SCLK        14
#define TFT_MOSI        28
#define TFT_MISO        39

#define PIN_RESET       25 //PTA5
#define PIN_SERIAL_ATN   4 //PTA13
#define PIN_SERIAL_CLK  26 //PTA14
#define PIN_SERIAL_DATA 27 //PTA15
#define PIN_SERIAL_SRQ  36 //PTC9

#if 0
#define WRITE_ATN_CLK_DATA(value) { \
    digitalWriteFast(PIN_SERIAL_ATN, (~value & CIA2_PRA_IEC_ATN_OUT)); \
    digitalWriteFast(PIN_SERIAL_CLK, (~value & CIA2_PRA_IEC_CLK_OUT)); \
    digitalWriteFast(PIN_SERIAL_DATA, (~value & CIA2_PRA_IEC_DATA_OUT)); \
}
#define READ_CLK_DATA() \
  ((digitalReadFast(PIN_SERIAL_CLK) ? CIA2_PRA_IEC_CLK_IN : 0) | \
   (digitalReadFast(PIN_SERIAL_DATA) ? CIA2_PRA_IEC_DATA_IN : 0))
#else // 0
#define WRITE_ATN_CLK_DATA(value) { \
    GPIOA_PCOR = ((value >> 3) & 0x07) << 13; \
    GPIOA_PSOR = ((~value >> 3) & 0x07) << 13;\
}
#define READ_CLK_DATA() \
    (((GPIOA_PDIR >> 14) & 0x03) << 6)
#endif // 0

#define PIN_JOY1_BTN     5 //PTD7
#define PIN_JOY1_1       2 //PTD0 up
#define PIN_JOY1_2       7 //PTD2 down
#define PIN_JOY1_3       8 //PTD3 left
#define PIN_JOY1_4       6 //PTD4 right
#define PIN_JOY1_A1     A12
#define PIN_JOY1_A2     A13

#define PIN_JOY2_BTN    24 //PTE26
#define PIN_JOY2_1       0 //PTB16 up
#define PIN_JOY2_2       1 //PTB17 down
#define PIN_JOY2_3      29 //PTB18 left
#define PIN_JOY2_4      30 //PTB19 right
#define PIN_JOY2_A1     A14
#define PIN_JOY2_A2     A15

#include "output_dac.h"
#include "cpu.h"

#endif // TEENSY64_TEENSY64_H