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

#include "cpu.h"

#include "cia6526.h"
#include "cia1.h"

#define tod()       (cpu.cia1.TODfrozen ? cpu.cia1.TODfrozenMillis : (int)( (millis() - cpu.cia1.TOD) % 86400000l) )

void cia1_setAlarmTime() {
    cpu.cia1.TODAlarm = cpu.cia1.W[CIA_TOD10TH] + cpu.cia1.W[CIA_TODSEC] * 10L + cpu.cia1.W[CIA_TODMIN] * 600L +
                        cpu.cia1.W[CIA_TODHR] * 36000L;
}

void cia1_write(uint32_t address, uint8_t value) {
    address &= 0x0F;

    switch(address) {
        case CIA_TALO:
            cpu.cia1.W[CIA_TALO] = value;
            break;

        case CIA_TAHI:
            cpu.cia1.W[CIA_TAHI] = value;

            if((cpu.cia1.R[CIA_CRA] & CIA_CR_START) == 0) {
                cpu.cia1.R[CIA_TAHI] = value;
            }
            break;

        case CIA_TBLO:
            cpu.cia1.W[CIA_TBLO] = value;
            break;

        case CIA_TBHI:
            cpu.cia1.W[CIA_TBHI] = value;

            if((cpu.cia1.R[CIA_CRB] & CIA_CR_START) == 0) {
                cpu.cia1.R[CIA_TBHI] = value;
            }
            break;

        case CIA_TOD10TH:
            if((cpu.cia1.R[CIA_CRB] & CIA_CRB_ALARM) > 0) {
                value &= CIA_TOD10TH_MASK;
                cpu.cia1.W[CIA_TOD10TH] = value;

                cia1_setAlarmTime();
            } else {
                value &= CIA_TOD10TH_MASK;
                cpu.cia1.TODstopped = 0;

                //Translate set Time to TOD:
                cpu.cia1.TOD = (int) (millis() % 86400000L) -
                               (value * 100 + cpu.cia1.R[CIA_TODSEC] * 1000L + cpu.cia1.R[CIA_TODMIN] * 60000L +
                                cpu.cia1.R[CIA_TODHR] * 3600000L
                               );
            }
            break;

        case CIA_TODSEC:
            if((cpu.cia1.R[CIA_CRB] & CIA_CRB_ALARM) > 0) {
                cpu.cia1.W[CIA_TODSEC] = bcdToDec(value);

                cia1_setAlarmTime();
            } else {
                cpu.cia1.R[CIA_TODSEC] = bcdToDec(value);
            }
            break;

        case CIA_TODMIN:
            if((cpu.cia1.R[CIA_CRB] & CIA_CRB_ALARM) > 0) {
                cpu.cia1.W[CIA_TODMIN] = bcdToDec(value);

                cia1_setAlarmTime();
            } else {
                cpu.cia1.R[CIA_TODMIN] = bcdToDec(value);
            }
            break;

        case CIA_TODHR:
            if((cpu.cia1.R[CIA_CRB] & CIA_CRB_ALARM) > 0) {
                cpu.cia1.W[CIA_TODHR] = bcdToDec(value & CIA_TODHR_HR_MASK) + (value & CIA_TODHR_PM ? 12 : 0);

                cia1_setAlarmTime();
            } else {
                cpu.cia1.R[CIA_TODHR] = bcdToDec(value & CIA_TODHR_HR_MASK) + (value & CIA_TODHR_PM ? 12 : 0);
                cpu.cia1.TODstopped = 1;
            }
            break;

        case CIA_SDR:
            cpu.cia1.R[CIA_SDR] = value;
            cpu.cia1.R[CIA_ICR] |= CIA_ICR_SP | (cpu.cia1.W[CIA_ICR] & CIA_ICR_SP ? CIA_ICR_IR : 0);
            break;

        case CIA_ICR:
            if((value & 0x80) > 0) {
                cpu.cia1.W[CIA_ICR] |= value & CIA_ICR_IRQ_MASK;

                if(cpu.cia1.R[CIA_ICR] & cpu.cia1.W[CIA_ICR] & CIA_ICR_IRQ_MASK) {
                    cpu.cia1.R[CIA_ICR] |= CIA_ICR_IR;
                }
            } else {
                cpu.cia1.W[CIA_ICR] &= ~value;
            }
            break;

        case CIA_CRA:
            cpu.cia1.R[CIA_CRA] = value & ~CIA_CR_LOAD;

            if(value & CIA_CR_LOAD) {
                cpu.cia1.R16[CIA_TALO / 2] = cpu.cia1.W16[CIA_TALO / 2];
            }
            break;

        case CIA_CRB:
            cpu.cia1.R[CIA_CRB] = value & ~CIA_CR_LOAD;

            if(value & CIA_CR_LOAD) {
                cpu.cia1.R16[CIA_TBLO / 2] = cpu.cia1.W16[CIA_TBLO / 2];
            }
            break;

        default:
            cpu.cia1.R[address] = value;
            break;
    }
}

uint8_t cia1_read(uint32_t address) {
    uint8_t ret;

    address &= 0x0F;

    switch(address) {
        case CIA_PRA:
            ret = cia1PORTA();
            break;

        case CIA_PRB:
            ret = cia1PORTB();
            break;

        case CIA_TOD10TH:
            ret = tod() % 1000 / 10;
            cpu.cia1.TODfrozen = 0;
            break;

        case CIA_TODSEC:
            ret = decToBcd(tod() / 1000 % 60);
            break;

        case CIA_TODMIN:
            ret = decToBcd(tod() / (1000 * 60) % 60);
            break;

        case CIA_TODHR:
            cpu.cia1.TODfrozen = 0;
            cpu.cia1.TODfrozenMillis = tod();
            cpu.cia1.TODfrozen = 1;

            ret = cpu.cia1.TODfrozenMillis / (1000 * 3600) % 24;

            if(ret >= 12) {
                ret = 128 | decToBcd(ret - 12);
            } else {
                ret = decToBcd(ret);
            }
            break;

        case CIA_ICR:
            ret = cpu.cia1.R[CIA_ICR] & CIA_ICR_READ_MASK;
            cpu.cia1.R[CIA_ICR] = 0;
            break;

        default:
            ret = cpu.cia1.R[address];
            break;
    }

    return ret;
}

#if 0
void cia1_clock(int clk) {

    uint32_t cnta, cntb, cra, crb;

    //Timer A
    cra = cpu.cia1.R[CIA_CRA];
    crb = cpu.cia1.R[CIA_CRB];

    if (( cra & 0x21) == 0x01) {
        cnta = cpu.cia1.R[CIA_TALO] | cpu.cia1.R[CIA_TAHI] << 8;
        cnta -= clk;
        if (cnta > 0xffff) { //Underflow
            cnta = cpu.cia1.W[CIA_TALO] | cpu.cia1.W[CIA_TAHI] << 8; // Reload Timer
            if (cra & 0x08) { // One Shot
                cpu.cia1.R[CIA_CRA] &= 0xfe; //Stop timer
            }

            //Interrupt:
            cpu.cia1.R[CIA_ICR] |= 1 /*| (cpu.cia1.W[0x1a] & 0x01) */| ((cpu.cia1.W[CIA_ICR] & 0x01) << 7);

            if ((crb & 0x61)== 0x41) { //Timer B counts underflows of Timer A
                cntb = cpu.cia1.R[CIA_TBLO] | cpu.cia1.R[CIA_TBHI] << 8;
                cntb--;
                if (cntb > 0xffff) { //underflow
                    cpu.cia1.R[CIA_TALO] = cnta;
                    cpu.cia1.R[CIA_TAHI] = cnta >> 8;
                    goto underflow_b;
                }
            }
        }

        cpu.cia1.R[CIA_TALO] = cnta;
        cpu.cia1.R[CIA_TAHI] = cnta >> 8;

    }

    //Timer B
    if (( crb & 0x61) == 0x01) {
        cntb = cpu.cia1.R[CIA_TBLO] | cpu.cia1.R[CIA_TBHI] << 8;
        cntb -= clk;
        if (cntb > 0xffff) { //underflow
underflow_b:
            cntb = cpu.cia1.W[CIA_TBLO] | cpu.cia1.W[CIA_TBHI] << 8; // Reload Timer
            if (crb & 0x08) { // One Shot
                cpu.cia1.R[CIA_CRB] &= 0xfe; //Stop timer
            }

            //Interrupt:
            cpu.cia1.R[CIA_ICR] |= 2 /*|  (cpu.cia1.W[0x1a] & 0x02) */ | ((cpu.cia1.W[CIA_ICR] & 0x02) << 6);

        }

        cpu.cia1.R[CIA_TBLO] = cntb;
        cpu.cia1.R[CIA_TBHI] = cntb >> 8;

    }
}
#else

void cia1_clock(uint16_t clk) {
    uint16_t t;
    uint32_t regFEDC = cpu.cia1.R32[CIA_SDR / 4];


    if(((regFEDC >> 16) & 0x21) == 0x1) {
        t = cpu.cia1.R16[CIA_TALO / 2];

        if(clk > t) { //underflow ?
            t = cpu.cia1.W16[CIA_TALO / 2] - (clk - t);
            regFEDC |= 0x00000100;
            if(regFEDC & 0x00080000) { regFEDC &= 0xfffeffff; } //One-Shot
        } else {
            t -= clk;
        }

        cpu.cia1.R16[CIA_TALO / 2] = t;
    }


    // TIMER B
    if(regFEDC & 0x01000000) {
        if((regFEDC & 0x60000000) == 0x40000000) {

            if(regFEDC & 0x00000100) { //unterlauf TimerA?
                clk = 1;
            } else {
                goto tend;
            }
        }

        t = cpu.cia1.R16[CIA_TBLO / 2];

        if(clk > t) { //underflow ?
            t = cpu.cia1.W16[CIA_TBLO / 2] - (clk - t);
            regFEDC |= 0x00000200;
            if(regFEDC & 0x08000000) { regFEDC &= 0xfeffffff; }
        } else {
            t -= clk;
        }
        cpu.cia1.R16[CIA_TBLO / 2] = t; //One-Shot

    }

    tend:


    // INTERRUPT ?
    if(regFEDC & cpu.cia1.W32[CIA_SDR / 4] & 0x0f00) {
        regFEDC |= 0x8000;
        cpu.cia1.R32[CIA_SDR / 4] = regFEDC;
    } else { cpu.cia1.R32[CIA_SDR / 4] = regFEDC; }
}

#endif

void cia1_checkRTCAlarm() { // call @ 1/10 sec interval minimum
    if((millis() - cpu.cia1.TOD) % 86400000L / 100 == cpu.cia1.TODAlarm) {
        cpu.cia1.R[CIA_ICR] |= CIA_ICR_ALRM | (cpu.cia1.W[CIA_ICR] & CIA_ICR_ALRM ? CIA_ICR_IR : 0);
    }
}

void cia1FLAG() {
    cpu.cia1.R[CIA_ICR] |= CIA_ICR_FLG | (cpu.cia1.W[CIA_ICR] & CIA_ICR_FLG ? CIA_ICR_IR : 0);
}

void resetCia1() {
    initJoysticks();
    initKeyboard();

    memset((uint8_t * ) & cpu.cia1.R, 0, sizeof(cpu.cia1.R));

    cpu.cia1.W[CIA_TALO] = cpu.cia1.R[CIA_TALO] = 0xff;
    cpu.cia1.W[CIA_TAHI] = cpu.cia1.R[CIA_TAHI] = 0xff;
    cpu.cia1.W[CIA_TBLO] = cpu.cia1.R[CIA_TBLO] = 0xff;
    cpu.cia1.W[CIA_TBHI] = cpu.cia1.R[CIA_TBHI] = 0xff;

    //FLAG pin CIA1 - Serial SRQ (input only)
    pinMode(PIN_SERIAL_SRQ, OUTPUT_OPENDRAIN);
    digitalWriteFast(PIN_SERIAL_SRQ, 1);
    attachInterrupt(digitalPinToInterrupt(PIN_SERIAL_SRQ), cia1FLAG, FALLING);
}


