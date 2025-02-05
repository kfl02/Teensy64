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

#ifndef TEENSY64_CPU_H
#define TEENSY64_CPU_H

#include <Arduino.h>

#include "teensy64.h"
#include "roms.h"
#include "patches.h"
#include "util.h"
#include "pla.h"
#include "vic.h"
#include "keyboard.h"
#include "cia1.h"
#include "cia2.h"


#include <reSID.h>

extern AudioPlaySID playSID;
extern AudioOutputAnalog audioout;

#define BASE_STACK     0x100

struct tio {
    uint32_t gpioa;
    uint32_t gpiob;
    uint32_t gpioc;
    uint32_t gpiod;
    uint32_t gpioe;
}__attribute__((packed, aligned(4)));

struct tcpu {
    uint32_t exactTimingStartTime{};
    uint8_t exactTiming{};

    //6502 CPU registers
    uint8_t sp{};
    uint8_t a{};
    uint8_t x{};
    uint8_t y{};
    uint8_t cpustatus{};
    uint8_t nmi{};
    uint16_t pc{};

    //helper variables
    uint16_t reladdr{};
    uint16_t ea{};

    uint16_t lineCyclesAbs{}; //for debug
    uint16_t ticks{};
    unsigned lineCycles{};
    unsigned long lineStartTime{};

    r_rarr_ptr_t plamap_r{}; //Memory-Mapping read
    w_rarr_ptr_t plamap_w{}; //Memory-Mapping write
    uint8_t _exrom: 1;
    uint8_t _game: 1;
    uint8_t nmiLine{};
    uint8_t swapJoysticks{};

    tvic vic;
    tcia cia1{};
    tcia cia2{};

    uint8_t RAM[1 << 16]{};

    uint8_t cartrigeLO[1]{}; //TODO
    uint8_t cartrigeHI[1]{}; //TODO
};

extern struct tio io;
extern struct tcpu cpu;

void cpu_reset();
void cpu_nmi();
void cpu_clearNmi();
void cpu_clock(int cycles);
void cpu_setExactTiming();
void cpu_disableExactTiming();
void cia_clockt(int ticks);

#endif // TEENSY64_CPU_H