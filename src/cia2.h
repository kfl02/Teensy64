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

#ifndef TEENSY64_CIA2_H
#define TEENSY64_CIA2_H

#include <Arduino.h>

#define CIA2_PRA_VIC_BANK_MASK  0x03
#define CIA2_PRA_TXD            0x04
#define CIA2_PRA_IEC_ATN_OUT    0x08
#define CIA2_PRA_IEC_CLK_OUT    0x10
#define CIA2_PRA_IEC_DATA_OUT   0x20
#define CIA2_PRA_IEC_CLK_IN     0x40
#define CIA2_PRA_IEC_DATA_IN    0x80
#define CIA2_PRA_IEC_OUT_MASK   (CIA2_PRA_IEC_ATN_OUT | CIA2_PRA_IEC_CLK_OUT | CIA2_PRA_IEC_DATA_OUT)
#define CIA2_PRA_IEC_IN_MASK    (CIA2_PRA_IEC_CLK_IN | CIA2_PRA_IEC_DATA_IN)

void cia2_clock(uint16_t clk) __attribute__ ((hot));
void cia2_checkRTCAlarm() __attribute__ ((hot));
void cia2_write(uint32_t address, uint8_t value) __attribute__ ((hot));
uint8_t cia2_read(uint32_t address) __attribute__ ((hot));
void resetCia2(void);

#endif
