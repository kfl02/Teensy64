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

#ifndef TEENSY64_VIC_H
#define TEENSY64_VIC_H

#include <Arduino.h>

#include "teensy64.h"
#include "vic656x.h"

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

struct tvic {
    uint32_t timeStart, neededTime;
    uint16_t intRasterLine;
    uint16_t rasterLine;
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
    uint8_t spriteCycles0_2;
    uint8_t spriteCycles3_7;
    uint8_t fgcollision;

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
            uint8_t M0X;
            uint8_t M0Y;
            uint8_t M1X;
            uint8_t M1Y;
            uint8_t M2X;
            uint8_t M2Y;
            uint8_t M3X;
            uint8_t M3Y;
            uint8_t M4X;
            uint8_t M4Y;
            uint8_t M5X;
            uint8_t M5Y;
            uint8_t M6X;
            uint8_t M6Y;
            uint8_t M7X;
            uint8_t M7Y;
            uint8_t MX8; // Sprite-X Bit 8 $D010
            uint8_t YSCROLL: 3;
            uint8_t RSEL: 1;
            uint8_t DEN: 1;
            uint8_t BMM: 1;
            uint8_t ECM: 1;
            uint8_t RST8: 1; // $D011
            uint8_t RASTER; // Rasterline $D012
            uint8_t LPX; // Lightpen X $D013
            uint8_t LPY; // Lightpen Y $D014
            uint8_t ME;  // Sprite Enable $D015
            uint8_t XSCROLL: 3;
            uint8_t CSEL: 1;
            uint8_t MCM: 1;
            uint8_t RES: 1;
            uint8_t : 2; // $D016
            uint8_t MYE; // Sprite Y-Expansion $D017
            uint8_t : 1;
            uint8_t CB: 3;
            uint8_t VM: 4; // $D018
            uint8_t IRST: 1;
            uint8_t IMBC: 1;
            uint8_t IMMC: 1;
            uint8_t ILP: 1;
            uint8_t : 3;
            uint8_t IRQ: 1; // $D019
            uint8_t ERST: 1;
            uint8_t EMBC: 1;
            uint8_t EMMC: 1;
            uint8_t ELP: 1;
            uint8_t : 4; // $D01A
            uint8_t MDP; // Sprite-Daten-Priority $D01B
            uint8_t MMC; // Sprite Multicolor $D01C
            uint8_t MXE; // Sprite X-Expansion $D01D
            uint8_t MM;  // Sprite-Sprite collision $D01E
            uint8_t MD;  // Sprite-Data collision $D01F
            uint8_t EC: 4;
            uint8_t : 4; // Bordercolor $D020
            uint8_t B0C: 4;
            uint8_t : 4; // Backgroundcolor 0 $D021
            uint8_t B1C: 4;
            uint8_t : 4; // Backgroundcolor 1 $D022
            uint8_t B2C: 4;
            uint8_t : 4; // Backgroundcolor 2 $D023
            uint8_t B3C: 4;
            uint8_t : 4; // Backgroundcolor 3 $D024
            uint8_t MM0: 4;
            uint8_t : 4; // Sprite Multicolor 0 $D025
            uint8_t MM1: 4;
            uint8_t : 4; // Sprite Multicolor 1 $D026
            uint8_t M0C: 4;
            uint8_t : 4; // Spritecolor 0 $D027
            uint8_t M1C: 4;
            uint8_t : 4; // Spritecolor 1 $D028
            uint8_t M2C: 4;
            uint8_t : 4; // Spritecolor 2 $D029
            uint8_t M3C: 4;
            uint8_t : 4; // Spritecolor 3 $D02A
            uint8_t M4C: 4;
            uint8_t : 4; // Spritecolor 4 $D02B
            uint8_t M5C: 4;
            uint8_t : 4; // Spritecolor 5 $D02C
            uint8_t M6C: 4;
            uint8_t : 4; // Spritecolor 6 $D02D
            uint8_t M7C: 4;
            uint8_t : 4; // Spritecolor 7 $D02E
        } r;
    };

    //tsprite spriteInfo[8];//todo
    uint16_t spriteLine[SPRITE_MAX_X];

    uint8_t lineMemChr[40];
    uint8_t lineMemCol[40];
    uint8_t colorRAM[1024];

    static void render(void);
    static void renderSimple(void);
    static void displaySimpleModeScreen(void);
    static void write(uint32_t address, uint8_t value);
    static uint8_t read(uint32_t address);
    static void applyAdressChange(void);
    void reset(void);
    void updatePalette(int n);
    void nextPalette();
};

#endif // TEENSY64_VIC_H
