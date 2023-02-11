/*
	Copyright Frank Bösing, Karsten Fleischer, 2017, 2023

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

#include <Arduino.h>
#include <cstring>
#include "cia6526.h"

inline uint32_t CIA6526::tod() const {
    return TODfrozen ? TODfrozenMillis : ((millis() - TOD) % DAY_IN_MILLISECONDS);
}

void CIA6526::setAlarmTime() {
    // time is saved as 24 hours in latch
    TODalarm = latch.TOD10TH + latch.TODSEC * 10L + latch.TODMIN * 10L * 60L + latch.TODHR * 10L * 60L * 60L;
}

void CIA6526::write(uint32_t address, uint8_t value) {
    address &= 0x0F;

    switch(address) {
        case CIA_TALO:
            latch.TALO = value;
            break;

        case CIA_TAHI:
            latch.TAHI = value;

            if((r.CRA & CIA_CR_START) == 0) {
                r.TAHI = value;
            }
            break;

        case CIA_TBLO:
            latch.TBLO = value;
            break;

        case CIA_TBHI:
            latch.TBHI = value;

            if((r.CRB & CIA_CR_START) == 0) {
                r.TBHI = value;
            }
            break;

        case CIA_TOD10TH:
            value &= CIA_TOD10TH_MASK;

            if(r.CRB & CIA_CRB_ALARM) {
                latch.TOD10TH = value;

                setAlarmTime();
            } else {
                TODstopped = false;
                TOD = (millis() % DAY_IN_MILLISECONDS) -
                       (value * 100L + r.TODSEC * 10L * 100L +
                        r.TODMIN * 10L * 100L * 60L +
                        r.TODHR * 10L * 100L * 60L * 60L);
            }
            break;

        case CIA_TODSEC:
            value &= CIA_TODSEC_MASK;

            if(r.CRB & CIA_CRB_ALARM) {
                latch.TODSEC = bcdToDec(value);

                setAlarmTime();
            } else {
                r.TODSEC = bcdToDec(value);
            }
            break;

        case CIA_TODMIN:
            value &= CIA_TODMIN_MASK;

            if(r.CRB & CIA_CRB_ALARM) {
                latch.TODMIN = bcdToDec(value);

                setAlarmTime();
            } else {
                r.TODMIN = bcdToDec(value);
            }
            break;

        case CIA_TODHR:
            value &= CIA_TODHR_MASK;

            if(r.CRB & CIA_CRB_ALARM) {
                // save in latch as 24 hours
                latch.TODHR = bcdToDec(value & CIA_TODHR_HR_MASK) + (value & CIA_TODHR_PM ? 12 : 0);

                setAlarmTime();
            } else {
                // save in latch as 24 hours
                r.TODHR = bcdToDec(value & CIA_TODHR_HR_MASK) + (value & CIA_TODHR_PM ? 12 : 0);
                TODstopped = true; // TODO: how does the real 6526 behave on read when the TOD is stopped?
            }
            break;

        case CIA_SDR:
            r.SDR = value;
            r.ICR |= CIA_ICR_SP | (latch.ICR & CIA_ICR_SP ? CIA_ICR_IR : 0);
            break;

        case CIA_ICR:
            if(value & CIA_ICR_SET) {
                latch.ICR |= value & CIA_ICR_IRQ_MASK;

                if(r.ICR & latch.ICR & CIA_ICR_IRQ_MASK) {
                    r.ICR |= CIA_ICR_IR;
                }
            } else {
                latch.ICR &= ~value;
            }
            break;

        case CIA_CRA:
            r.CRA = value & ~CIA_CR_LOAD;

            if(value & CIA_CR_LOAD) {
                r.TA = latch.TA;
            }
            break;

        case CIA_CRB:
            r.CRB = value & ~CIA_CR_LOAD;

            if(value & CIA_CR_LOAD) {
                r.TB = latch.TB;
            }
            break;

        default:
            R[address] = value;
            break;
    }
}

uint8_t CIA6526::read(uint32_t address) {
    uint8_t ret;

    address &= 0x0f;

    switch(address) {
        case CIA_TOD10TH:
            ret = tod() % 1000 / 10;
            TODfrozen = true;
            break;

        case CIA_TODSEC:
            ret = decToBcd(tod() / 1000 % 60);
            break;

        case CIA_TODMIN:
            ret = decToBcd(tod() / (1000 * 60) % 60);
            break;

        case CIA_TODHR:
            TODfrozen = false;
            TODfrozenMillis = tod();
            TODfrozen = true;

            ret = TODfrozenMillis / (1000 * 60 * 60) % 24;

            // convert 24 hour time to AM/PM
            // TODO: 6526 chip seems to have a bug with the PM flag.
            // Investigate and modify code according to the bug. OTOH: who is ever going to use the TOD?
            if(ret >= 12) {
                ret = CIA_TODHR_PM | decToBcd(ret - 12);
            } else {
                ret = decToBcd(ret);
            }
            break;

        case CIA_ICR:
            ret = r.ICR & CIA_ICR_READ_MASK;
            r.ICR = 0;
            break;

        default:
            ret = R[address];
            break;
    }

    return ret;
}

void CIA6526::clock(uint16_t clk) {
    uint16_t t;
    uint8_t icr = r.ICR;
    uint8_t cra = r.CRA;
    uint8_t crb = r.CRB;

    if((cra & (CIA_CRA_INMODE | CIA_CR_START)) == CIA_CR_START) {
        t = r.TA;

        if(clk > t) { // underflow
            t = latch.TA - (clk - t);
            icr |= CIA_ICR_TA;

            if((cra & CIA_CR_RUNMODE) == CIA_CR_RUNMODE_ONESHOT) {
                cra &= ~CIA_CR_START;
            } else {
                t -= clk;
            }
        }

        r.TA = t;
    }

    if(crb & CIA_CR_START) {
        if((crb & CIA_CRB_INMODE_MASK) == CIA_CRB_INMODE_TA) {
            if(icr & CIA_ICR_TA) { // underflow, see above
                clk = 1;
            } else {
                goto skip;
            }
        }

        t = r.TB;

        if(clk > t) {
            t = latch.TB - (clk - t);
            icr |= CIA_ICR_TB;
        }

        if((crb & CIA_CR_RUNMODE) == CIA_CR_RUNMODE_ONESHOT) {
            crb &= ~CIA_CR_START;
        } else {
            t -= clk;
        }

        r.TB = t;
    }

skip:

    if(icr & latch.ICR & CIA_ICR_IRQ_MASK) {
        icr |= CIA_ICR_IR;
    }

    r.ICR = icr;
    r.CRA = cra;
    r.CRB = crb;
}

void CIA6526::checkRTCAlarm() {
    if((millis() - TOD) % DAY_IN_MILLISECONDS / 100 == TODalarm) {
        r.ICR |= CIA_ICR_ALRM | (latch.ICR & CIA_ICR_ALRM ? CIA_ICR_IR : 0);
    }
}

void CIA6526::reset() {
    memset(R, 0, sizeof(R));

    r.TA = r.TB = 0xffff;
}