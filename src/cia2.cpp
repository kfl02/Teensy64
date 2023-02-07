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

#include "cpu.h"

#include "cia6526.h"
#include "cia2.h"

#define tod()       (cpu.cia2.TODfrozen ? cpu.cia2.TODfrozenMillis : (int)( (millis() - cpu.cia2.TOD) % 86400000l) )

void cia2_setAlarmTime() {
    cpu.cia2.TODAlarm = cpu.cia2.W[0x08] + cpu.cia2.W[0x09] * 10L + cpu.cia2.W[0x0A] * 600L +
                        cpu.cia2.W[0x0B] * 36000L;
}

void cia2_write(uint32_t address, uint8_t value) {

    address &= 0x0F;

    switch(address) {
        case CIA_PRA:
            if(~value & CIA2_PRA_IEC_OUT_MASK) {
                cpu_setExactTiming();
            }

            WRITE_ATN_CLK_DATA(value);

            cpu.vic.bank = ((~value) & CIA2_PRA_VIC_BANK_MASK) * 16384;
            tvic::applyAdressChange();
            cpu.cia2.R[CIA_PRA] = value;
            break;

        case CIA_PRB:
            break;

        case CIA_TALO:
            cpu.cia2.W[CIA_TALO] = value;
            break;

        case CIA_TAHI:
            cpu.cia2.W[CIA_TAHI] = value;

            if((cpu.cia2.R[CIA_CRA] & CIA_CR_START) == 0) {
                cpu.cia2.R[CIA_TAHI] = value;
            }
            break;

        case CIA_TBLO:
            cpu.cia2.W[CIA_TBLO] = value;
            break;

        case CIA_TBHI:
            cpu.cia2.W[CIA_TBHI] = value;

            if((cpu.cia2.R[CIA_CRB] & CIA_CR_START) == 0) {
                cpu.cia2.R[CIA_TBHI] = value;
            }
            break;

        case CIA_TOD10THS:
            if((cpu.cia2.R[CIA_CRB] & CIA_CRB_ALARM) > 0) {
                value &= CIA_TOD10THS_MASK;
                cpu.cia2.W[CIA_TOD10THS] = value;

                cia2_setAlarmTime();
            } else {
                value &= CIA_TOD10THS_MASK;
                cpu.cia2.TODstopped = 0;

                //Translate set Time to TOD:
                cpu.cia2.TOD = (int) (millis() % 86400000L) -
                               (value * 100 + cpu.cia2.R[CIA_TODSEC] * 1000L + cpu.cia2.R[CIA_TODMIN] * 60000L +
                                cpu.cia2.R[CIA_TODHR] * 3600000L
                               );
            }
            break;

        case CIA_TODSEC:
            if((cpu.cia2.R[CIA_CRB] & CIA_CRB_ALARM) > 0) {
                cpu.cia2.W[CIA_TODSEC] = bcdToDec(value);

                cia2_setAlarmTime();
            } else {
                cpu.cia2.R[CIA_TODSEC] = bcdToDec(value);
            }
            break; //TOD-Secs

        case CIA_TODMIN:
            if((cpu.cia2.R[CIA_CRB] & CIA_CRB_ALARM) > 0) {
                cpu.cia2.W[CIA_TODMIN] = bcdToDec(value);

                cia2_setAlarmTime();
            } else {
                cpu.cia2.R[CIA_TODMIN] = bcdToDec(value);
            }
            break; //TOD-Minutes

        case CIA_TODHR:
            if((cpu.cia2.R[CIA_CRB] & CIA_CRB_ALARM) > 0) {
                cpu.cia2.W[address] = bcdToDec(value & CIA_TODHR_HR_MASK) + (value & CIA_TODHR_PM ? 12: 0);
                cia2_setAlarmTime();
            } else {
                cpu.cia2.R[address] = bcdToDec(value & CIA_TODHR_HR_MASK) + (value & CIA_TODHR_PM ? 12: 0);
                cpu.cia2.TODstopped = 1;
            }
            break;

        case CIA_SDR:
            cpu.cia2.R[CIA_SDR] = value;
            cpu.cia2.R[CIA_ICR] |= CIA_ICR_SP | (cpu.cia2.W[CIA_ICR] & CIA_ICR_SP ? CIA_ICR_IR : 0);

            cpu_nmi();
            break;

        case CIA_ICR:
            if(value & CIA_ICR_SET) {
                cpu.cia2.W[CIA_ICR] |= value & CIA_ICR_IRQ_MASK;

                if(cpu.cia2.R[CIA_ICR] & cpu.cia2.W[CIA_ICR] & CIA_ICR_IRQ_MASK) {
                    cpu.cia2.R[CIA_ICR] |= CIA_ICR_IR;
                    cpu_nmi();
                }
            } else {
                cpu.cia2.W[CIA_ICR] &= ~value;
            }
            break;

        case CIA_CRA:
            cpu.cia2.R[CIA_CRA] = value & ~CIA_CR_LOAD;

            if(value & CIA_CR_LOAD) {
                cpu.cia2.R16[CIA_TALO / 2] = cpu.cia2.W16[CIA_TALO / 2];
            }
            break;

        case CIA_CRB:

            cpu.cia2.R[CIA_CRB] = value & ~CIA_CR_LOAD;

            if(value & CIA_CR_LOAD) {
                cpu.cia2.R16[CIA_TBLO / 2] = cpu.cia2.W16[CIA_TBLO / 2];
            }
            break;

        default:
            cpu.cia2.R[address] = value;
            break;
    }
}

uint8_t cia2_read(uint32_t address) {
    uint8_t ret;

    address &= 0x0F;

    switch(address) {
        case CIA_PRA:
            ret = (cpu.cia2.R[CIA_PRA] & ~CIA2_PRA_IEC_IN_MASK) | (uint8_t)READ_CLK_DATA();

            if(~ret & ~CIA2_PRA_IEC_IN_MASK) {
                cpu_setExactTiming();
            }
            break;

        case CIA_TOD10THS:
            ret = tod() % 1000 / 10;
            cpu.cia2.TODfrozen = 0;
            break;

        case CIA_TODSEC:
            ret = decToBcd(tod() / 1000 % 60);
            break;

        case CIA_TODMIN:
            ret = decToBcd(tod() / (1000 * 60) % 60);
            break;

        case CIA_TODHR:
            cpu.cia2.TODfrozen = 0;
            cpu.cia2.TODfrozenMillis = tod();
            cpu.cia2.TODfrozen = 1;

            ret = cpu.cia2.TODfrozenMillis / (1000 * 3600) % 24;

            if(ret >= 12) {
                ret = 128 | decToBcd(ret - 12);
            } else {
                ret = decToBcd(ret);
            }
            break;

        case CIA_ICR:
            ret = cpu.cia2.R[CIA_ICR] & CIA_ICR_READ_MASK;
            cpu.cia2.R[CIA_ICR] = 0;
            cpu_clearNmi();
            break;

        default:
            ret = cpu.cia2.R[address];
            break;
    }

    return ret;
}

#if 0
void cia2_clock(int clk) {

  uint32_t cnta, cntb, cra, crb;

  //Timer A
  cra = cpu.cia2.R[0x0e];
  crb = cpu.cia2.R[0x0f];

  if (( cra & 0x21) == 0x01) {
    cnta = cpu.cia2.R[CIA_TALO] | cpu.cia2.R[CIA_TAHI] << 8;
    cnta -= clk;
    if (cnta > 0xffff) { //Underflow
      cnta = cpu.cia2.W[CIA_TALO] | cpu.cia2.W[CIA_TAHI] << 8; // Reload Timer
      if (cra & 0x08) { // One Shot
        cpu.cia2.R[0x0e] &= 0xfe; //Stop timer
      }

      //Interrupt:
      cpu.cia2.R[CIA_ICR] |= 1 | /* (cpu.cia2.W[CIA_ICR] & 0x01) |*/ ((cpu.cia2.W[CIA_ICR] & 0x01) << 7);

      if ((crb & 0x61) == 0x41) { //Timer B counts underflows of Timer A
        cntb = cpu.cia2.R[CIA_TBLO] | cpu.cia2.R[CIA_TBHI] << 8;
        cntb--;
        if (cntb > 0xffff) { //underflow
          cpu.cia2.R[CIA_TALO] = cnta & 0x0f;
          cpu.cia2.R[CIA_TAHI] = cnta >> 8;
          goto underflow_b;
        }
      }
    }

    cpu.cia2.R[CIA_TALO] = cnta & 0x0f;
    cpu.cia2.R[CIA_TAHI] = cnta >> 8;

  }

  //Timer B
  if (( crb & 0x61) == 0x01) {
    cntb = cpu.cia2.R[CIA_TBLO] | cpu.cia2.R[CIA_TBHI] << 8;
    cntb -= clk;
    if (cntb > 0xffff) { //underflow
underflow_b:
      cntb = cpu.cia2.W[CIA_TBLO] | cpu.cia2.W[CIA_TBHI] << 8; // Reload Timer
      if (crb & 0x08) { // One Shot
        cpu.cia2.R[0x0f] &= 0xfe; //Stop timer
      }

      //Interrupt:
      cpu.cia2.R[CIA_ICR] |= 2 | /*(cpu.cia2.W[CIA_ICR] & 0x02) | */ ((cpu.cia2.W[CIA_ICR] & 0x02) << 6);
    }

    cpu.cia2.R[CIA_TBLO] = cntb & 0x0f;
    cpu.cia2.R[CIA_TBHI] = cntb >> 8;

  }
  if (cpu.cia2.R[CIA_ICR] & 0x80) cpu_nmi();
}

#else

void cia2_clock(uint16_t clk) {
    uint16_t t;
    uint32_t regFEDC = cpu.cia2.R32[CIA_SDR / 4];

    if(((regFEDC >> 16) & 0x21) == 0x1) {

        t = cpu.cia2.R16[CIA_TALO / 2];

        if(clk > t) { //underflow
            t = cpu.cia2.W16[CIA_TALO / 2] - (clk - t); //neu
            regFEDC |= 0x00000100;
            if(regFEDC & 0x00080000) { regFEDC &= 0xfffeffff; }
        } else {
            t -= clk;
        }

        cpu.cia2.R16[CIA_TALO / 2] = t;
    }


    // TIMER B
    //TODO: Prüfen ob das funktioniert
    if(regFEDC & 0x01000000) {
        if((regFEDC & 0x60000000) == 0x40000000) {

            if(regFEDC & 0x00000100) { //unterlauf TimerA?
                clk = 1;
            } else {
                goto tend;
            }
        }

        t = cpu.cia2.R16[CIA_TBLO / 2];

        if(clk > t) { //underflow
            t = cpu.cia2.W16[CIA_TBLO / 2] - (clk - t); //Neu
            regFEDC |= 0x00000200;
            if(regFEDC & 0x08000000) { regFEDC &= 0xfeffffff; }
        } else {
            t -= clk;
        }
        cpu.cia2.R16[CIA_TBLO / 2] = t;
    }

    tend:


    // INTERRUPT ?
    if(regFEDC & cpu.cia2.W32[CIA_SDR / 4] & 0x0f00) {
        regFEDC |= 0x8000;
        cpu.cia2.R32[CIA_SDR / 4] = regFEDC;
    }
    cpu.cia2.R32[CIA_SDR / 4] = regFEDC;
}

#endif

void cia2_checkRTCAlarm() { // call every 1/10 sec minimum
    if((millis() - cpu.cia2.TOD) % 86400000L / 100 == cpu.cia2.TODAlarm) {
        cpu.cia2.R[CIA_ICR] |= CIA_ICR_ALRM | (cpu.cia2.W[CIA_ICR] & CIA_ICR_ALRM ? CIA_ICR_IR : 0);
    }
}

void resetCia2() {
    memset((uint8_t * ) & cpu.cia2.R, 0, sizeof(cpu.cia2.R));

    cpu.cia2.W[CIA_TALO] = cpu.cia2.R[CIA_TALO] = 0xff;
    cpu.cia2.W[CIA_TAHI] = cpu.cia2.R[CIA_TAHI] = 0xff;
    cpu.cia2.W[CIA_TBLO] = cpu.cia2.R[CIA_TBLO] = 0xff;
    cpu.cia2.W[CIA_TBHI] = cpu.cia2.R[CIA_TBHI] = 0xff;

    pinMode(PIN_SERIAL_ATN, OUTPUT_OPENDRAIN);
    pinMode(PIN_SERIAL_CLK, OUTPUT_OPENDRAIN);
    pinMode(PIN_SERIAL_DATA, OUTPUT_OPENDRAIN);
    digitalWriteFast(PIN_SERIAL_ATN, 1);
    digitalWriteFast(PIN_SERIAL_CLK, 1);
    digitalWriteFast(PIN_SERIAL_DATA, 1);
}
