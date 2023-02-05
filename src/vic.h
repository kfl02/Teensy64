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

#ifndef TEENSY64_VIC_H
#define TEENSY64_VIC_H

#include <Arduino.h>
#include <IntervalTimer.h>
#include "Teensy64.h"

#define BORDER        20
#define SCREEN_WIDTH   320
#define LINE_MEM_WIDTH 320
#define FIRSTDISPLAYLINE (  51 - BORDER )
#define LASTDISPLAYLINE  ( 250 + BORDER )

#define BORDER_LEFT     0
#define BORDER_RIGHT    0

typedef uint16_t tpixel;

#define SPRITE_MAX_X (320 + 24)

/* for later use
struct tsprite {
  uint8_t MC; //Mob Data Counter
  uint8_t MCBase; //Mob Data Counter Base
  uint8_t MobYexpand; //Y-Epansion FlipFlop
};
*/

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
#define VIC_CR1_DEN             ßx10
#define VIC_CR1_BMM             0x20
#define VIC_CR1_ECM             0x40
#define VIC_CR1_RST8            0x80
#define VIC_CR1_xxM_MASK        0x60

#define VIC_CR2_XSCROLL_MASK    0x07
#define VIC_CR2_CSEL            0x08
#define VIC_CR2_MCM             ßx10
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

struct tvic {
    uint32_t timeStart, neededTime;
    int intRasterLine; //Interruptsetting
    int rasterLine;
    uint16_t bank;
    uint16_t vcbase;
    uint8_t rc;

    uint8_t borderFlag;  //Top-Bottom border flag
    uint8_t borderFlagH; //Left-Right border flag
    uint8_t idle;
    uint8_t denLatch;
    uint8_t badline;
    uint8_t BAsignal;
    uint8_t lineHasSprites;
    int8_t spriteCycles0_2;
    int8_t spriteCycles3_7;
    int fgcollision;

    uint8_t *charsetPtrBase;
    uint8_t *charsetPtr;
    uint8_t *bitmapPtr;
    uint16_t videomatrix;

    uint16_t palette[16];
    uint8_t paletteNo;

    IntervalTimer lineClock;

    union {
        uint8_t R[0x40];
        struct {
            uint8_t M0X, M0Y, M1X, M1Y, M2X, M2Y, M3X, M3Y, M4X, M4Y, M5X, M5Y, M6X, M6Y, M7X, M7Y;
            uint8_t MX8; // Sprite-X Bit 8 $D010
            uint8_t YSCROLL: 3, RSEL: 1, DEN: 1, BMM: 1, ECM: 1, RST8: 1; // $D011
            uint8_t RASTER; // Rasterline $D012
            uint8_t LPX; // Lightpen X $D013
            uint8_t LPY; // Lightpen Y $D014
            uint8_t ME;  // Sprite Enable $D015
            uint8_t XSCROLL: 3, CSEL: 1, MCM: 1, RES: 1, : 2; // $D016
            uint8_t MYE; // Sprite Y-Expansion $D017
            uint8_t : 1, CB: 3, VM: 4; // $D018
            uint8_t IRST: 1, IMBC: 1, IMMC: 1, ILP: 1, : 3, IRQ: 1; // $D019
            uint8_t ERST: 1, EMBC: 1, EMMC: 1, ELP: 1, : 4; // $D01A
            uint8_t MDP; // Sprite-Daten-Priority $D01B
            uint8_t MMC; // Sprite Multicolor $D01C
            uint8_t MXE; // Sprite X-Expansion $D01D
            uint8_t MM;  // Sprite-Sprite collision $D01E
            uint8_t MD;  // Sprite-Data collision $D01F
            uint8_t EC: 4, : 4; // Bordercolor $D020
            uint8_t B0C: 4, : 4; // Backgroundcolor 0 $D021
            uint8_t B1C: 4, : 4; // Backgroundcolor 1 $D022
            uint8_t B2C: 4, : 4; // Backgroundcolor 2 $D023
            uint8_t B3C: 4, : 4; // Backgroundcolor 3 $D024
            uint8_t MM0: 4, : 4; // Sprite Multicolor 0 $D025
            uint8_t MM1: 4, : 4; // Sprite Multicolor 1 $D026
            uint8_t M0C: 4, : 4; // Spritecolor 0 $D027
            uint8_t M1C: 4, : 4; // Spritecolor 1 $D028
            uint8_t M2C: 4, : 4; // Spritecolor 2 $D029
            uint8_t M3C: 4, : 4; // Spritecolor 3 $D02A
            uint8_t M4C: 4, : 4; // Spritecolor 4 $D02B
            uint8_t M5C: 4, : 4; // Spritecolor 5 $D02C
            uint8_t M6C: 4, : 4; // Spritecolor 6 $D02D
            uint8_t M7C: 4, : 4; // Spritecolor 7 $D02E
        } r;
    };

    //tsprite spriteInfo[8];//todo
    uint16_t spriteLine[SPRITE_MAX_X];

    uint8_t lineMemChr[40];
    uint8_t lineMemCol[40];
    uint8_t COLORRAM[1024];

    static void render(void);
    static void renderSimple(void);
    static void displaySimpleModeScreen(void);
    static void write(uint32_t address, uint8_t value);
    static uint8_t read(uint32_t address);
    static void adrchange(void);
    void reset(void);
    void updatePalette(int n);
    void nextPalette();
};

#endif // TEENSY64_VIC_H
