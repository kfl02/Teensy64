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

#ifndef TEENSY64_VIC656X_H
#define TEENSY64_VIC656X_H

// http://www.oxyron.de/html/registers_vic2.html

#define VIC_M0X     0x00
#define VIC_M0Y     0x01
#define VIC_M1X     0x02
#define VIC_M1Y     0x03
#define VIC_M2X     0x04
#define VIC_M2Y     0x05
#define VIC_M3X     0x06
#define VIC_M3Y     0x07
#define VIC_M4X     0x08
#define VIC_M4Y     0x09
#define VIC_M5X     0x0A
#define VIC_M5Y     0x0B
#define VIC_M6X     0x0C
#define VIC_M6Y     0x0D
#define VIC_M7X     0x0E
#define VIC_M7Y     0x0F
#define VIC_MxX8    0x10
#define VIC_CR1     0x11
#define VIC_RASTER  0x12
#define VIC_RSTCMP  0x12
#define VIC_LPX     0x13
#define VIC_LPY     0x14
#define VIC_MxE     0x15
#define VIC_CR2     0x16
#define VIC_MxYE    0x17
#define VIC_VM_CB   0x18
#define VIC_IRQST   0x19
#define VIC_IRQEN   0x1A
#define VIC_MxDP    0x1B
#define VIC_MxMC    0x1C
#define VIC_MxXE    0x1D
#define VIC_MxM     0x1E
#define VIC_MxD     0x1F
#define VIC_EC      0x20
#define VIC_B0C     0x21
#define VIC_B1C     0x22
#define VIC_B2C     0x23
#define VIC_B3C     0x24
#define VIC_MM0     0x25
#define VIC_MM1     0x26
#define VIC_M0C     0x27
#define VIC_M1C     0x28
#define VIC_M2C     0x29
#define VIC_M3C     0x2A
#define VIC_M4C     0x2B
#define VIC_M5C     0x2C
#define VIC_M6C     0x2D
#define VIC_M7C     0x2E

#define VIC_CR1_YSCROLL_MASK    0x07
#define VIC_CR1_RSEL            0x08
#define VIC_CR1_DEN             0x10
#define VIC_CR1_BMM             0x20
#define VIC_CR1_ECM             0x40
#define VIC_CR1_RST8            0x80
#define VIC_CR1_xxM_MASK        0x60

#define VIC_CR2_XSCROLL_MASK    0x07
#define VIC_CR2_CSEL            0x08
#define VIC_CR2_MCM             0x10
#define VIC_CR2_RES             0x20
#define VIC_CR2_UNUSED_MASK     0xc0

#define VIC_VM_CB_VM_MASK       0xf0
#define VIC_VM_CB_CB_MASK       0x0e
#define VIC_VM_CB_UNUSED_MASK   0x01

#define VIC_IRQST_IRST          0x01
#define VIC_IRQST_IMBC          0x02
#define VIC_IRQST_IMMC          0x04
#define VIC_IRQST_ILP           0x08
#define VIC_IRQST_IRQ           0x80
#define VIC_IRQST_MASK          0x0f
#define VIC_IRQST_UNUSED_MASK   0x70

#define VIC_IRQEN_ERST          0x01
#define VIC_IRQEN_EMBC          0x02
#define VIC_IRQEN_EMMC          0x04
#define VIC_IRQEN_ELP           0x08
#define VIC_IRQEN_UNUSED_MASK   0xf0

#define VIC_EC_MASK  0x0f
#define VIC_B0C_MASK 0x0f
#define VIC_B1C_MASK 0x0f
#define VIC_B2C_MASK 0x0f
#define VIC_B3C_MASK 0x0f
#define VIC_MM0_MASK 0x0f
#define VIC_MM1_MASK 0x0f
#define VIC_M0C_MASK 0x0f
#define VIC_M1C_MASK 0x0f
#define VIC_M2C_MASK 0x0f
#define VIC_M3C_MASK 0x0f
#define VIC_M4C_MASK 0x0f
#define VIC_M5C_MASK 0x0f
#define VIC_M6C_MASK 0x0f
#define VIC_M7C_MASK 0x0f
#define VIC_xC_MASK  0x0f

#define VIC_EC_UNUSED_MASK  0xf0
#define VIC_B0C_UNUSED_MASK 0xf0
#define VIC_B1C_UNUSED_MASK 0xf0
#define VIC_B2C_UNUSED_MASK 0xf0
#define VIC_B3C_UNUSED_MASK 0xf0
#define VIC_MM0_UNUSED_MASK 0xf0
#define VIC_MM1_UNUSED_MASK 0xf0
#define VIC_M0C_UNUSED_MASK 0xf0
#define VIC_M1C_UNUSED_MASK 0xf0
#define VIC_M2C_UNUSED_MASK 0xf0
#define VIC_M3C_UNUSED_MASK 0xf0
#define VIC_M4C_UNUSED_MASK 0xf0
#define VIC_M5C_UNUSED_MASK 0xf0
#define VIC_M6C_UNUSED_MASK 0xf0
#define VIC_M7C_UNUSED_MASK 0xf0
#define VIC_xC_UNUSED_MASK  0xf0

class VIC656x {
    union {
        uint8_t R[0x40];
        struct {
            uint8_t M0X;    // $D000
            uint8_t M0Y;    // $D001
            uint8_t M1X;    // $D002
            uint8_t M1Y;    // $D003
            uint8_t M2X;    // $D004
            uint8_t M2Y;    // $D005
            uint8_t M3X;    // $D006
            uint8_t M3Y;    // $D007
            uint8_t M4X;    // $D008
            uint8_t M4Y;    // $D009
            uint8_t M5X;    // $D00a
            uint8_t M5Y;    // $D00b
            uint8_t M6X;    // $D00c
            uint8_t M6Y;    // $D00d
            uint8_t M7X;    // $D00e
            uint8_t M7Y;    // $D00f
            union {
                struct {
                    uint8_t MxX8;
                };
                uint8_t M0X8 : 1;
                uint8_t M1X8 : 1;
                uint8_t M2X8 : 1;
                uint8_t M3X8 : 1;
                uint8_t M4X8 : 1;
                uint8_t M5X8 : 1;
                uint8_t M6X8 : 1;
                uint8_t M7X8 : 1;
            };  // $D010
            union {
                struct {
                    uint8_t CR1;
                };
                uint8_t YSCROLL: 3;
                uint8_t RSEL: 1;
                uint8_t DEN: 1;
                uint8_t BMM: 1;
                uint8_t ECM: 1;
                uint8_t RST8: 1;
            };  // $D011
            uint8_t RASTER; // $D012
            uint8_t LPX;    // $D013
            uint8_t LPY;    // $D014
            union {
                struct {
                    uint8_t MxE;
                };
                uint8_t M0E : 1;
                uint8_t M1E : 1;
                uint8_t M2E : 1;
                uint8_t M3E : 1;
                uint8_t M4E : 1;
                uint8_t M5E : 1;
                uint8_t M6E : 1;
                uint8_t M7E : 1;
            };  // $$D015
            union {
                struct {
                    uint8_t CR2;
                };
                uint8_t XSCROLL: 3;
                uint8_t CSEL: 1;
                uint8_t MCM: 1;
                uint8_t RES: 1;
                uint8_t : 2;
            };  // $D016
            uint8_t MxYE;   // $D017
            union {
                struct {
                    uint8_t VM_CB;
                };
                uint8_t : 1;
                uint8_t CB: 3;
                uint8_t VM: 4;
            };  // $D018
            union {
                struct {
                    uint8_t IRQST;
                };
                uint8_t IRST : 1;
                uint8_t IMBC : 1;
                uint8_t IMMC : 1;
                uint8_t ILP : 1;
                uint8_t : 3;
                uint8_t IRQ: 1;
            };  // $D019
            union {
                struct {
                    uint8_t IRQEN;
                };
                uint8_t ERST : 1;
                uint8_t EMBC : 1;
                uint8_t EMMC : 1;
                uint8_t ELP : 1;
                uint8_t : 4;
            };  // $D01A
            union {
                struct {
                    uint8_t MxDP;
                };
                uint8_t M0DP : 1;
                uint8_t M1DP : 1;
                uint8_t M2DP : 1;
                uint8_t M3DP : 1;
                uint8_t M4DP : 1;
                uint8_t M5DP : 1;
                uint8_t M6DP : 1;
                uint8_t M7DP : 1;
            };  // $D01B
            union {
                struct {
                    uint8_t MxMC;
                };
                uint8_t M0MC : 1;
                uint8_t M1MC : 1;
                uint8_t M2MC : 1;
                uint8_t M3MC : 1;
                uint8_t M4MC : 1;
                uint8_t M5MC : 1;
                uint8_t M6MC : 1;
                uint8_t M7MC : 1;
            };  // $D01C
            union {
                struct {
                    uint8_t MxXE;
                };
                uint8_t M0XE : 1;
                uint8_t M1XE : 1;
                uint8_t M2XE : 1;
                uint8_t M3XE : 1;
                uint8_t M4XE : 1;
                uint8_t M5XE : 1;
                uint8_t M6XE : 1;
                uint8_t M7XE : 1;
            };  // $D01D
            union {
                struct {
                    uint8_t MxM;
                };
                uint8_t M0M : 1;
                uint8_t M1M : 1;
                uint8_t M2M : 1;
                uint8_t M3M : 1;
                uint8_t M4M : 1;
                uint8_t M5M : 1;
                uint8_t M6M : 1;
                uint8_t M7M : 1;
            };  // $D01E
            union {
                struct {
                    uint8_t MxD;
                };
                uint8_t M0D : 1;
                uint8_t M1D : 1;
                uint8_t M2D : 1;
                uint8_t M3D : 1;
                uint8_t M4D : 1;
                uint8_t M5D : 1;
                uint8_t M6D : 1;
                uint8_t M7D : 1;
            };  // $D01F
            struct {
                uint8_t EC: 4;
                uint8_t : 4;
            };  // $D020
            struct {
                uint8_t B0C: 4;
                uint8_t : 4;
            };  // $D021
            struct {
                uint8_t B1C: 4;
                uint8_t : 4;
            };  // $D022
            struct {
                uint8_t B2C: 4;
                uint8_t : 4;
            };  // $D023
            struct {
                uint8_t B3C: 4;
                uint8_t : 4;
            };  // $D024
            struct {
                uint8_t MM0: 4;
                uint8_t : 4;
            };  // $D025
            struct {
                uint8_t MM1: 4;
                uint8_t : 4;
            };  // $D026
            struct {
                uint8_t M0C: 4;
                uint8_t : 4;
            };  // $D027
            struct {
                uint8_t M1C: 4;
                uint8_t : 4;
            };  // $D028
            struct {
                uint8_t M2C: 4;
                uint8_t : 4;
            };  // $D029
            struct {
                uint8_t M3C: 4;
                uint8_t : 4;
            };  // $D02A
            struct {
                uint8_t M4C: 4;
                uint8_t : 4;
            };  // $D02B
            struct {
                uint8_t M5C: 4;
                uint8_t : 4;
            };  // $D02C
            struct {
                uint8_t M6C: 4;
                uint8_t : 4;
            };  // $D02D
            struct {
                uint8_t M7C: 4;
                uint8_t : 4;
            };  // $D02E
        } r;
    };
};

#endif //TEENSY64_VIC656X_H
