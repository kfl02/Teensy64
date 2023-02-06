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
    return TODfrozen ? TODfrozenMillis : (uint32_t)( (millis() - TOD) % DAY_IN_MILLISECONDS);
}

void CIA6526::setAlarmTime() {
    TODalarm = w.tod10ths + w.todsec * 10l + w.todmin * 10l * 60l + w.todhr * 10l * 60l * 60l;
}

void CIA6526::write(uint32_t address, uint8_t value) {
    address &= 0x0F;

    switch(address) {
        case CIA_TALO:
        case CIA_TBLO:
            W[address] = value;
            break;

        case CIA_TAHI:
            w.tahi = value;

            if((r.cra & CIA_CR_START) == 0) {
                r.tahi = value;
            }
            break;

        case CIA_TBHI:
            w.tbhi = value;

            if((r.crb & CIA_CR_START) == 0) {
                r.tbhi = value;
            }
            break;

        case CIA_TOD10THS:
            value &= CIA_TOD10THS_MASK;

            if(r.crb & CIA_CRB_ALARM) {
                w.tod10ths = value;

                setAlarmTime();
            } else {
                TODstopped = false;
                TOD = (uint32_t) (millis() % DAY_IN_MILLISECONDS) -
                       (value * 100l + r.todsec * 10l * 100l +
                       r.todmin * 10l * 100l * 60l +
                       r.todhr * 10l * 100l * 60l * 60l);
            }
            break;

        case CIA_TODSEC:
            value &= CIA_TODSEC_MASK;

            if(r.crb & CIA_CRB_ALARM) {
                w.todsec = bcdToDec(value);

                setAlarmTime();
            } else {
                r.todsec = bcdToDec(value);
            }
            break;

        case CIA_TODMIN:
            value &= CIA_TODMIN_MASK;

            if(r.crb & CIA_CRB_ALARM) {
                w.todmin = bcdToDec(value);

                setAlarmTime();
            } else {
                r.todmin = bcdToDec(value);
            }
            break;

        case CIA_TODHR:
            value &= CIA_TODHR_MASK;

            if(r.crb & CIA_CRB_ALARM) {
                w.todhr = bcdToDec(value & CIA_TODHR_HR_MASK) + (value & CIA_TODHR_PM ? 12 : 0);

                setAlarmTime();
            } else {
                r.todhr = bcdToDec(value & CIA_TODHR_HR_MASK) + (value & CIA_TODHR_PM ? 12 : 0);
                TODstopped = true;
            }
            break;

        case CIA_SDR:
            r.sdr = value;
            r.icr |= CIA_ICR_SP | (w.icr & CIA_ICR_SP ? CIA_ICR_IR : 0);
            break;

        case CIA_ICR:
            if(value & CIA_ICR_SET) {
                w.icr |= value & CIA_ICR_IRQ_MASK;

                if(r.icr & w.icr & CIA_ICR_IRQ_MASK) {
                    r.icr |= CIA_ICR_IR;
                }
            } else {
                w.icr &= ~value;
            }
            break;

        case CIA_CRA:
            r.cra = value & ~CIA_CR_LOAD;

            if(value & CIA_CR_LOAD) {
                r.ta = w.ta;
            }
            break;

        case CIA_CRB:
            r.crb = value & ~CIA_CR_LOAD;

            if(value & CIA_CR_LOAD) {
                r.tb = w.tb;
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
        case CIA_TOD10THS:
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

            if(ret >= 12) {
                ret = CIA_TODHR_PM | decToBcd(ret - 12);
            } else {
                ret = decToBcd(ret);
            }
            break;

        case CIA_ICR:
            ret = r.icr & CIA_ICR_READ_MASK;
            r.icr = 0;
            break;

        default:
            ret = R[address];
            break;
    }

    return ret;
}

void CIA6526::clock(int clk) {
    int32_t t;
    uint8_t icr = r.icr;
    uint8_t cra = r.cra;
    uint8_t crb = r.crb;

    if((cra & (CIA_CRA_INMODE | CIA_CR_START)) == CIA_CR_START) {
        t = r.ta;

        if(clk > t) { // underflow
            t = w.ta - (clk - t);
            icr |= CIA_ICR_TA;

            if((cra & CIA_CR_RUNMODE) == CIA_CR_RUNMODE_ONESHOT) {
                cra &= ~CIA_CR_START;
            } else {
                t -= clk;
            }
        }

        r.ta = t;
    }

    if(crb & CIA_CR_START) {
        if((crb & CIA_CRB_INMODE_MASK) == CIA_CRB_INMODE_TA) {
            if(icr & CIA_ICR_TA) { // underflow, see above
                clk = 1;
            } else {
                goto skip;
            }
        }

        t = r.tb;

        if(clk > t) {
            t = w.tb - (clk -t);
            icr |= CIA_ICR_TB;
        }

        if((crb & CIA_CR_RUNMODE) == CIA_CR_RUNMODE_ONESHOT) {
            crb &= ~CIA_CR_START;
        } else {
            t -= clk;
        }

        r.tb = t;
    }

skip:

    if(icr & w.icr & CIA_ICR_IRQ_MASK) {
        icr |= CIA_ICR_IR;
    }

    r.icr = icr;
    r.cra = cra;
    r.crb = crb;
}

void CIA6526::checkRTCAlarm() {
    if((millis() - TOD) % DAY_IN_MILLISECONDS / 100 == TODalarm) {
        R[CIA_ICR] |= CIA_ICR_ALRM | (W[CIA_ICR] & CIA_ICR_ALRM ? CIA_ICR_IR : 0);
    }
}

void CIA6526::reset() {
    memset(R, 0, sizeof(R));

    r.ta = r.tb = 0xffff;
}