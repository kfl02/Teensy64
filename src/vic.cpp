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

/*
  TODOs:
  - Fix Bugs..
  - FLD  - (OK 08/17) test this more..
  - Sprite Stretching (requires "MOBcounter")
  - BA Signal -> CPU
  - xFLI
  - ...
  - DMA Delay (?) - needs partial rewrite (idle - > badline in middle of line. Is the 3.6 fast enough??)
  - optimize more
*/

#include "teensy64.h"
#include "vic.h"
#include "vic_palette.h"

#include "font_Play-Bold.h"

#define MAXCYCLESSPRITES0_2     3
#define MAXCYCLESSPRITES3_7     5
#define MAXCYCLESSPRITES        (MAXCYCLESSPRITES0_2 + MAXCYCLESSPRITES3_7)

DMAMEM uint16_t screen[ILI9341_TFTHEIGHT][ILI9341_TFTWIDTH];
uint16_t *const screenMem = &screen[0][0];

/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/

inline __attribute__((always_inline))
static uint8_t spriteBitFromLineData(uint16_t data) {
    return (uint8_t)(1 << (uint8_t)((data >> 8) & 0x07));
}

inline __attribute__((always_inline))
static uint8_t spritePixelFromLineData(uint16_t data) {
    return data & 0x0f;
}

inline __attribute__((always_inline))
static bool spritePriorityFromLineData(uint16_t data) {
    return data & 0x40;
}


#define CHARSETPTR() (cpu.vic.charsetPtr = cpu.vic.charsetPtrBase + cpu.vic.rc)
#define CYCLES(x) {if (cpu.vic.badline) {cia_clockt(x);} else {cpu_clock(x);} }

#define BADLINE(x) {if (cpu.vic.badline) { \
      cpu.vic.lineMemChr[x] = cpu.RAM[cpu.vic.videomatrix + vc + x]; \
      cpu.vic.lineMemCol[x] = cpu.vic.colorRAM[vc + x]; \
      cia1_clock(1); \
      cia2_clock(1); \
    } else { \
      cpu_clock(1); \
    } \
  }

#define SPRITEORFIXEDCOLOR() \
  sprite = *spl++; \
  if (sprite) { \
    *p++ = cpu.vic.palette[sprite & 0x0f]; \
  } else { \
    *p++ = col; \
  }

#if 0
#define PRINTOVERFLOW   \
  if (p>pe) { \
    Serial.print("VIC overflow Mode "); \
    Serial.println(mode); \
  }

#define PRINTOVERFLOWS  \
  if (p>pe) { \
    Serial.print("VIC overflow (Sprite) Mode ");  \
    Serial.println(mode); \
  }
#else
#define PRINTOVERFLOW
#define PRINTOVERFLOWS
#endif

/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/

inline __attribute__((always_inline))
static void fastFillLineNoSprites(tpixel *p, const tpixel *pe, const uint16_t col) {
    int i = 0;

    while(p < pe) {
        *p++ = col;
        i = (i + 1) & 0x07;
        if(!i) CYCLES(1);
    }
}

inline __attribute__((always_inline))
static void fastFillLine(tpixel *p, const tpixel *pe, const uint16_t col, uint16_t const *spl) {
    if(spl != nullptr && cpu.vic.lineHasSprites) {
        int i = 0;
        uint16_t sprite;
        while(p < pe) {
            SPRITEORFIXEDCOLOR();
            i = (i + 1) & 0x07;
            if(!i) CYCLES(1);
        }
    } else {

        fastFillLineNoSprites(p, pe, col);
    }
}

/*****************************************************************************************************/
static void mode0(tpixel *p, const tpixel *pe, uint16_t const *spl, const uint16_t vc) {
    /*
        3.7.3.1. Standard text mode (ECM/BMM/MCM=0/0/0)
        -----------------------------------------------

        In this mode (as in all text modes), the VIC reads 8 bit character pointers
        from the video matrix that specify the address of the dot matrix of the
        character in the character generator. A character set of 256 characters is
        available, each consisting of 8×8 pixels which are stored in 8 successive
        bytes in the character generator. Video matrix and character generator can
        be moved in memory with the bits VM10-VM13 and CB11-CB13 of register $d018.

        In standard text mode, every bit in the character generator directly
        corresponds to one pixel on the screen. The foreground color is given by
        the color nybble from the video matrix for each character, the background
        color is set globally with register $d021.

        c-access

         Addresses

         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         |VM13|VM12|VM11|VM10| VC9| VC8| VC7| VC6| VC5| VC4| VC3| VC2| VC1| VC0|
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

         Data

         +----+----+----+----+----+----+----+----+----+----+----+----+
         | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+
         |      Color of     | D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |
         |     "1" pixels    |    |    |    |    |    |    |    |    |
         +-------------------+----+----+----+----+----+----+----+----+

        g-access

         Addresses

         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         |CB13|CB12|CB11| D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 | RC2| RC1| RC0|
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

         Data

         +----+----+----+----+----+----+----+----+
         |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+
         |         8 pixels (1 bit/pixel)        |
         |                                       |
         | "0": Background color 0 ($d021)       |
         | "1": Color from bits 8-11 of c-data   |
         +---------------------------------------+
     */

    uint16_t x = 0;

    CHARSETPTR();

    if(cpu.vic.lineHasSprites) {
        do {
            BADLINE(x);

            auto chr = cpu.vic.charsetPtr[cpu.vic.lineMemChr[x] * 8];
            auto fgcol = cpu.vic.lineMemCol[x];
            uint8_t pixel;

            x++;

            unsigned m = min(8, pe - p);

            for(unsigned i = 0; i < m; i++) {
                uint16_t spriteLineData = *spl++;

                if(spriteLineData) {     // Sprite: Ja
                    uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                    uint8_t spritePixel = spritePixelFromLineData(spriteLineData);
                    bool spritePriority = spritePriorityFromLineData(spriteLineData);

                    if(spritePriority) {   // Sprite: Hinter Text  MxDP = 1
                        if(chr & 0x80) {
                            cpu.vic.fgcollision |= spriteBit;
                            pixel = fgcol;
                        } else {
                            pixel = spritePixel;
                        }
                    } else {            // Sprite: Vor Text //MxDP = 0
                        if(chr & 0x80) {
                            cpu.vic.fgcollision |= spriteBit;
                        }

                        pixel = spritePixel;
                    }
                } else {            // Kein Sprite
                    pixel = (chr & 0x80) ? fgcol : cpu.vic.r.B0C;
                }

                *p++ = cpu.vic.palette[pixel];
                chr = chr << 1;
            }
        } while(p < pe);
        PRINTOVERFLOWS
    } else { //Keine Sprites
        while(p < pe - 8) {
            BADLINE(x);

            auto chr = cpu.vic.charsetPtr[cpu.vic.lineMemChr[x] * 8];
            auto fgcol = cpu.vic.palette[cpu.vic.lineMemCol[x]];
            auto bgcol = cpu.vic.palette[cpu.vic.R[VIC_B0C]];
            x++;

            *p++ = (chr & 0x80) ? fgcol : bgcol;
            *p++ = (chr & 0x40) ? fgcol : bgcol;
            *p++ = (chr & 0x20) ? fgcol : bgcol;
            *p++ = (chr & 0x10) ? fgcol : bgcol;
            *p++ = (chr & 0x08) ? fgcol : bgcol;
            *p++ = (chr & 0x04) ? fgcol : bgcol;
            *p++ = (chr & 0x02) ? fgcol : bgcol;
            *p++ = (chr & 0x01) ? fgcol : bgcol;
        }

        while(p < pe) {
            BADLINE(x);

            auto chr = cpu.vic.charsetPtr[cpu.vic.lineMemChr[x] * 8];
            auto fgcol = cpu.vic.palette[cpu.vic.lineMemCol[x]];
            auto bgcol = cpu.vic.palette[cpu.vic.R[VIC_B0C]];
            x++;

            *p++ = (chr & 0x80) ? fgcol : bgcol;
            if(p >= pe) { break; }
            *p++ = (chr & 0x40) ? fgcol : bgcol;
            if(p >= pe) { break; }
            *p++ = (chr & 0x20) ? fgcol : bgcol;
            if(p >= pe) { break; }
            *p++ = (chr & 0x10) ? fgcol : bgcol;
            if(p >= pe) { break; }
            *p++ = (chr & 0x08) ? fgcol : bgcol;
            if(p >= pe) { break; }
            *p++ = (chr & 0x04) ? fgcol : bgcol;
            if(p >= pe) { break; }
            *p++ = (chr & 0x02) ? fgcol : bgcol;
            if(p >= pe) { break; }
            *p++ = (chr & 0x01) ? fgcol : bgcol;
        }
        PRINTOVERFLOW
    }

    while(x < 40) {
        BADLINE(x);
        x++;
    }
}

/*****************************************************************************************************/
static void mode1(tpixel *p, const tpixel *pe, uint16_t const *spl, const uint16_t vc) {
    /*
        3.7.3.2. Multicolor text mode (ECM/BMM/MCM=0/0/1)
        -------------------------------------------------

        This mode allows for displaying four-colored characters at the cost of
        horizontal resolution. If bit 11 of the c-data is zero, the character is
        displayed as in standard text mode with only the colors 0-7 available for
        the foreground. If bit 11 is set, each two adjacent bits of the dot matrix
        form one pixel. By this means, the resolution of a character of reduced to
        4×8 (the pixels are twice as wide, so the total width of the characters
        doesn't change).

        It is interesting that not only the bit combination "00" but also "01" is
        regarded as "background" for the sprite priority and collision detection.

        c-access

         Addresses

         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         |VM13|VM12|VM11|VM10| VC9| VC8| VC7| VC6| VC5| VC4| VC3| VC2| VC1| VC0|
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

         Data

         +----+----+----+----+----+----+----+----+----+----+----+----+
         | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+
         | MC |   Color of   | D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |
         |flag|  "11" pixels |    |    |    |    |    |    |    |    |
         +----+--------------+----+----+----+----+----+----+----+----+

        g-access

         Addresses

         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         |CB13|CB12|CB11| D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 | RC2| RC1| RC0|
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

         Data

         +----+----+----+----+----+----+----+----+
         |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+
         |         8 pixels (1 bit/pixel)        |
         |                                       | MC flag = 0
         | "0": Background color 0 ($d021)       |
         | "1": Color from bits 8-10 of c-data   |
         +---------------------------------------+
         |         4 pixels (2 bits/pixel)       |
         |                                       |
         | "00": Background color 0 ($d021)      | MC flag = 1
         | "01": Background color 1 ($d022)      |
         | "10": Background color 2 ($d023)      |
         | "11": Color from bits 8-10 of c-data  |
         +---------------------------------------+
    */

    uint8_t pixel;
    uint16_t bgcol;
    uint16_t fgcol;
    uint8_t chr;
    uint8_t colors[4];
    uint8_t x = 0;

    CHARSETPTR();

    if(cpu.vic.lineHasSprites) {
        colors[0] = cpu.vic.r.B0C;

        do {
            if(cpu.vic.idle) {
                cpu_clock(1);

                fgcol = colors[1] = colors[2] = colors[3] = 0;
                chr = cpu.RAM[cpu.vic.bank + 0x3fff];
            } else {
                BADLINE(x);

                fgcol = cpu.vic.lineMemCol[x];
                colors[1] = cpu.vic.R[VIC_B1C];
                colors[2] = cpu.vic.R[VIC_B2C];
                colors[3] = fgcol & 0x07;
                chr = cpu.vic.charsetPtr[cpu.vic.lineMemChr[x] * 8];
            }

            x++;

            if((fgcol & 0x08) == 0) { //Zeichen ist HIRES
                unsigned m = min(8, pe - p);

                for(unsigned i = 0; i < m; i++) {
                    uint16_t spriteLineData = *spl++;

                    if(spriteLineData) {     // Sprite: Ja
                        /*
                          Sprite-Prioritäten (Anzeige)
                          MxDP = 1: Grafikhintergrund, Sprite, Vordergrund
                          MxDP = 0: Grafikhintergrund, Vordergrund, Sprite

                          Kollision:
                          Eine Kollision zwischen Sprites und anderen Grafikdaten wird erkannt,
                          sobald beim Bildaufbau ein nicht transparentes Spritepixel und ein Vordergrundpixel ausgegeben wird.

                        */
                        uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                        bool spritePriority = spritePriorityFromLineData(spriteLineData);
                        pixel = spritePixelFromLineData(spriteLineData);

                        if(spritePriority) {   // MxDP = 1
                            if(chr & 0x80) { //Vordergrundpixel ist gesetzt
                                cpu.vic.fgcollision |= spriteBit;
                                pixel = colors[3];
                            }
                        } else {            // MxDP = 0
                            if(chr & 0x80) {
                                cpu.vic.fgcollision |= spriteBit;
                            } //Vordergrundpixel ist gesetzt
                        }
                    } else {            // Kein Sprite
                        pixel = (chr >> 7) ? colors[3] : colors[0];
                    }

                    *p++ = cpu.vic.palette[pixel];

                    chr = chr << 1;
                }
            } else {//Zeichen ist MULTICOLOR
                for(unsigned i = 0; i < 4; i++) {
                    if(p >= pe) {
                        break;
                    }

                    int c = (chr >> 6) & 0x03;

                    chr = chr << 2;

                    uint16_t spriteLineData = *spl++;

                    if(spriteLineData) {    // Sprite: Ja
                        uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                        bool spritePriority = spritePriorityFromLineData(spriteLineData);

                        pixel = spritePixelFromLineData(spriteLineData);

                        if(spritePriority) {  // MxDP = 1
                            if(chr & 0x80) {  //Vordergrundpixel ist gesetzt
                                cpu.vic.fgcollision |= spriteBit;
                                pixel = colors[c];
                            }
                        } else {          // MxDP = 0
                            if(chr & 0x80) {
                                cpu.vic.fgcollision |= spriteBit;
                            } //Vordergrundpixel ist gesetzt
                        }
                    } else { // Kein Sprite
                        pixel = colors[c];
                    }

                    *p++ = cpu.vic.palette[pixel];

                    if(p >= pe) {
                        break;
                    }

                    spriteLineData = *spl++;

                    //Das gleiche nochmal für das nächste Pixel
                    if(spriteLineData) {    // Sprite: Ja
                        uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                        bool spritePriority = spritePriorityFromLineData(spriteLineData);

                        pixel = spritePixelFromLineData(spriteLineData);

                        if(spritePriority) {  // MxDP = 1

                            if(chr & 0x80) { //Vordergrundpixel ist gesetzt
                                cpu.vic.fgcollision |= spriteBit;
                                pixel = colors[c];
                            }
                        } else {          // MxDP = 0
                            if(chr & 0x80) {
                                cpu.vic.fgcollision |= spriteBit;
                            } //Vordergrundpixel ist gesetzt
                        }
                    } else { // Kein Sprite
                        pixel = colors[c];
                    }
                    *p++ = cpu.vic.palette[pixel];
                }
            }
        } while(p < pe);
        PRINTOVERFLOWS
    } else { //Keine Sprites
        while(p < pe - 8) {
            int c;

            bgcol = cpu.vic.palette[cpu.vic.R[VIC_B0C]];
            colors[0] = bgcol;

            if(cpu.vic.idle) {
                cpu_clock(1);
                c = colors[1] = colors[2] = colors[3] = 0;
                chr = cpu.RAM[cpu.vic.bank + 0x3fff];
            } else {
                BADLINE(x);

                colors[1] = cpu.vic.palette[cpu.vic.R[VIC_B1C]];
                colors[2] = cpu.vic.palette[cpu.vic.R[VIC_B2C]];

                chr = cpu.vic.charsetPtr[cpu.vic.lineMemChr[x] * 8];
                c = cpu.vic.lineMemCol[x];
            }

            x++;

            if((c & 0x08) == 0) { //Zeichen ist HIRES
                fgcol = cpu.vic.palette[c & 0x07];
                *p++ = (chr & 0x80) ? fgcol : bgcol;
                *p++ = (chr & 0x40) ? fgcol : bgcol;
                *p++ = (chr & 0x20) ? fgcol : bgcol;
                *p++ = (chr & 0x10) ? fgcol : bgcol;
                *p++ = (chr & 0x08) ? fgcol : bgcol;
                *p++ = (chr & 0x04) ? fgcol : bgcol;
                *p++ = (chr & 0x02) ? fgcol : bgcol;
                *p++ = (chr & 0x01) ? fgcol : bgcol;
            } else {//Zeichen ist MULTICOLOR
                colors[3] = cpu.vic.palette[c & 0x07];
                pixel = colors[(chr >> 6) & 0x03];
                *p++ = pixel;
                *p++ = pixel;
                pixel = colors[(chr >> 4) & 0x03];
                *p++ = pixel;
                *p++ = pixel;
                pixel = colors[(chr >> 2) & 0x03];
                *p++ = pixel;
                *p++ = pixel;
                pixel = colors[(chr) & 0x03];
                *p++ = pixel;
                *p++ = pixel;
            }
        }

        while(p < pe) {

            int c;

            bgcol = cpu.vic.palette[cpu.vic.R[VIC_B0C]];
            colors[0] = bgcol;

            if(cpu.vic.idle) {
                cpu_clock(1);
                c = colors[1] = colors[2] = colors[3] = 0;
                chr = cpu.RAM[cpu.vic.bank + 0x3fff];
            } else {
                BADLINE(x);

                colors[1] = cpu.vic.palette[cpu.vic.R[VIC_B1C]];
                colors[2] = cpu.vic.palette[cpu.vic.R[VIC_B2C]];

                chr = cpu.vic.charsetPtr[cpu.vic.lineMemChr[x] * 8];
                c = cpu.vic.lineMemCol[x];
            }

            x++;

            if((c & 0x08) == 0) { //Zeichen ist HIRES
                fgcol = cpu.vic.palette[c & 0x07];
                *p++ = (chr & 0x80) ? fgcol : bgcol;
                if(p >= pe) { break; }
                *p++ = (chr & 0x40) ? fgcol : bgcol;
                if(p >= pe) { break; }
                *p++ = (chr & 0x20) ? fgcol : bgcol;
                if(p >= pe) { break; }
                *p++ = (chr & 0x10) ? fgcol : bgcol;
                if(p >= pe) { break; }
                *p++ = (chr & 0x08) ? fgcol : bgcol;
                if(p >= pe) { break; }
                *p++ = (chr & 0x04) ? fgcol : bgcol;
                if(p >= pe) { break; }
                *p++ = (chr & 0x02) ? fgcol : bgcol;
                if(p >= pe) { break; }
                *p++ = (chr & 0x01) ? fgcol : bgcol;
            } else {//Zeichen ist MULTICOLOR
                colors[3] = cpu.vic.palette[c & 0x07];
                pixel = colors[(chr >> 6) & 0x03];
                *p++ = pixel;
                if(p >= pe) { break; }
                *p++ = pixel;
                if(p >= pe) { break; }
                pixel = colors[(chr >> 4) & 0x03];
                *p++ = pixel;
                if(p >= pe) { break; }
                *p++ = pixel;
                if(p >= pe) { break; }
                pixel = colors[(chr >> 2) & 0x03];
                *p++ = pixel;
                if(p >= pe) { break; }
                *p++ = pixel;
                if(p >= pe) { break; }
                pixel = colors[(chr) & 0x03];
                *p++ = pixel;
                if(p >= pe) { break; }
                *p++ = pixel;
            }
        }
        PRINTOVERFLOW
    }
    while(x < 40) {
        BADLINE(x);
        x++;
    }
}

/*****************************************************************************************************/
static void mode2(tpixel *p, const tpixel *pe, uint16_t const *spl, const uint16_t vc) {
    /*
        3.7.3.3. Standard bitmap mode (ECM/BMM/MCM=0/1/0)
        -------------------------------------------------

        In this mode (as in all bitmap modes), the VIC reads the graphics data from
        a 320×200 bitmap in which every bit corresponds to one pixel on the screen.
        The data from the video matrix is used for color information. As the video
        matrix is still only a 40×25 matrix, you can only specify the colors for
        blocks of 8×8 pixels individually (sort of a YC 8:1 format). As the
        designers of the VIC wanted to realize the bitmap mode with as little
        additional circuitry as possible (the VIC-I didn't have a bitmap mode), the
        arrangement of the bitmap in memory is somewhat weird: In contrast to
        modern video chips that read the bitmap in a linear fashion from memory,
        the VIC forms an 8×8 pixel block on the screen from 8 successive bytes of
        the bitmap. The video matrix and the bitmap can be moved in memory with the
        bits VM10-VM13 and CB13 of register $d018.

        In standard bitmap mode, every bit in the bitmap directly corresponds to
        one pixel on the screen. Foreground and background color can be arbitrarily
        set for every 8×8 block.

        c-access

         Addresses

         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         |VM13|VM12|VM11|VM10| VC9| VC8| VC7| VC6| VC5| VC4| VC3| VC2| VC1| VC0|
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

         Data

         +----+----+----+----+----+----+----+----+----+----+----+----+
         | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+
         |       unused      |     Color of      |     Color of      |
         |                   |    "1" pixels     |    "0" pixels     |
         +-------------------+-------------------+-------------------+

        g-access

         Addresses

         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         |CB13| VC9| VC8| VC7| VC6| VC5| VC4| VC3| VC2| VC1| VC0| RC2| RC1| RC0|
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

         Data

         +----+----+----+----+----+----+----+----+
         |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+
         |         8 pixels (1 bit/pixel)        |
         |                                       |
         | "0": Color from bits 0-3 of c-data    |
         | "1": Color from bits 4-7 of c-data    |
         +---------------------------------------+

       http://www.devili.iki.fi/Computers/Commodore/C64/Programmers_Reference/Chapter_3/page_127.html
    */

    uint8_t chr;
    uint8_t pixel;
    uint16_t fgcol;
    uint16_t bgcol;
    uint8_t x = 0;
    uint8_t *bP = cpu.vic.bitmapPtr + vc * 8 + cpu.vic.rc;

    if(cpu.vic.lineHasSprites) {
        do {
            BADLINE(x);

            uint8_t t = cpu.vic.lineMemChr[x];
            fgcol = t >> 4;
            chr = bP[x * 8];

            x++;

            unsigned m = min(8, pe - p);

            for(unsigned i = 0; i < m; i++) {
                uint16_t spriteLineData = *spl++;

                chr = chr << 1;

                if(spriteLineData) {     // Sprite: Ja
                    /*
                       Sprite-Prioritäten (Anzeige)
                       MxDP = 1: Grafikhintergrund, Sprite, Vordergrund
                       MxDP = 0: Grafikhintergrund, Vordergrund, Sprite

                       Kollision:
                       Eine Kollision zwischen Sprites und anderen Grafikdaten wird erkannt,
                       sobald beim Bildaufbau ein nicht transparentes Spritepixel und ein Vordergrundpixel ausgegeben wird.

                    */
                    uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                    bool spritePriority = spritePriorityFromLineData(spriteLineData);

                    pixel = spritePixelFromLineData(spriteLineData);

                    if(spritePriority) {   // MxDP = 1
                        if(chr & 0x80) { //Vordergrundpixel ist gesetzt
                            cpu.vic.fgcollision |= spriteBit;
                            pixel = fgcol;
                        }
                    } else {            // MxDP = 0
                        if(chr & 0x80) {
                            cpu.vic.fgcollision |= spriteBit;
                        } //Vordergrundpixel ist gesetzt
                    }
                } else {            // Kein Sprite
                    pixel = (chr & 0x80) ? fgcol : cpu.vic.r.B0C;
                }

                *p++ = cpu.vic.palette[pixel];
            }
        } while(p < pe);
        PRINTOVERFLOWS
    } else { //Keine Sprites

        while(p < pe - 8) {
            //color-ram not used!
            BADLINE(x);

            uint8_t t = cpu.vic.lineMemChr[x];
            fgcol = cpu.vic.palette[t >> 4];
            bgcol = cpu.vic.palette[t & 0x0f];
            chr = bP[x * 8];
            x++;

            *p++ = (chr & 0x80) ? fgcol : bgcol;
            *p++ = (chr & 0x40) ? fgcol : bgcol;
            *p++ = (chr & 0x20) ? fgcol : bgcol;
            *p++ = (chr & 0x10) ? fgcol : bgcol;
            *p++ = (chr & 0x08) ? fgcol : bgcol;
            *p++ = (chr & 0x04) ? fgcol : bgcol;
            *p++ = (chr & 0x02) ? fgcol : bgcol;
            *p++ = (chr & 0x01) ? fgcol : bgcol;
        }
        while(p < pe) {
            //color-ram not used!
            BADLINE(x);

            uint8_t t = cpu.vic.lineMemChr[x];
            fgcol = cpu.vic.palette[t >> 4];
            bgcol = cpu.vic.palette[t & 0x0f];
            chr = bP[x * 8];

            x++;

            *p++ = (chr & 0x80) ? fgcol : bgcol;
            if(p >= pe) { break; }
            *p++ = (chr & 0x40) ? fgcol : bgcol;
            if(p >= pe) { break; }
            *p++ = (chr & 0x20) ? fgcol : bgcol;
            if(p >= pe) { break; }
            *p++ = (chr & 0x10) ? fgcol : bgcol;
            if(p >= pe) { break; }
            *p++ = (chr & 0x08) ? fgcol : bgcol;
            if(p >= pe) { break; }
            *p++ = (chr & 0x04) ? fgcol : bgcol;
            if(p >= pe) { break; }
            *p++ = (chr & 0x02) ? fgcol : bgcol;
            if(p >= pe) { break; }
            *p++ = (chr & 0x01) ? fgcol : bgcol;
        }
        PRINTOVERFLOW
    }
    while(x < 40) {
        BADLINE(x);
        x++;
    }
}

/*****************************************************************************************************/
static void mode3(tpixel *p, const tpixel *pe, uint16_t const *spl, const uint16_t vc) {
    /*
        3.7.3.4. Multicolor bitmap mode (ECM/BMM/MCM=0/1/1)
        ---------------------------------------------------

        Similar to the multicolor text mode, this mode also forms (twice as wide)
        pixels by combining two adjacent bits. So the resolution is reduced to
        160×200 pixels.

        The bit combination "01" is also treated as "background" for the sprite
        priority and collision detection, as in multicolor text mode.

        c-access

         Addresses

         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         |VM13|VM12|VM11|VM10| VC9| VC8| VC7| VC6| VC5| VC4| VC3| VC2| VC1| VC0|
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

         Data

         +----+----+----+----+----+----+----+----+----+----+----+----+
         | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+
         |     Color of      |     Color of      |     Color of      |
         |    "11 pixels"    |    "01" pixels    |    "10" pixels    |
         +-------------------+-------------------+-------------------+

        g-access

         Addresses

         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         |CB13| VC9| VC8| VC7| VC6| VC5| VC4| VC3| VC2| VC1| VC0| RC2| RC1| RC0|
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

         Data

         +----+----+----+----+----+----+----+----+
         |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+
         |         4 pixels (2 bits/pixel)       |
         |                                       |
         | "00": Background color 0 ($d021)      |
         | "01": Color from bits 4-7 of c-data   |
         | "10": Color from bits 0-3 of c-data   |
         | "11": Color from bits 8-11 of c-data  |
         +---------------------------------------+
     */
    uint8_t *bP = cpu.vic.bitmapPtr + vc * 8 + cpu.vic.rc;
    uint16_t colors[4];
    uint8_t pixel;
    uint8_t chr, x;

    x = 0;

    if(cpu.vic.lineHasSprites) {
        colors[0] = cpu.vic.r.B0C;

        do {
            if(cpu.vic.idle) {
                cpu_clock(1);
                colors[1] = colors[2] = colors[3] = 0;
                chr = cpu.RAM[cpu.vic.bank + 0x3fff];
            } else {
                BADLINE(x);
                uint8_t t = cpu.vic.lineMemChr[x];
                colors[1] = t >> 4;//10
                colors[2] = t & 0x0f; //01
                colors[3] = cpu.vic.lineMemCol[x];
                chr = bP[x * 8];
            }

            x++;

            for(unsigned i = 0; i < 4; i++) {
                if(p >= pe) {
                    break;
                }

                uint32_t c = (chr >> 6) & 0x03;
                chr = chr << 2;

                uint16_t spriteLineData = *spl++;

                if(spriteLineData) {    // Sprite: Ja
                    uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                    bool spritePriority = spritePriorityFromLineData(spriteLineData);

                    pixel = spritePixelFromLineData(spriteLineData);

                    if(spritePriority) {  // MxDP = 1
                        if(c & 0x02) {  //Vordergrundpixel ist gesetzt
                            cpu.vic.fgcollision |= spriteBit;
                            pixel = colors[c];
                        }
                    } else {          // MxDP = 0
                        if(c & 0x02) {
                            cpu.vic.fgcollision |= spriteBit;
                        } //Vordergundpixel ist gesetzt
                    }
                } else { // Kein Sprite
                    pixel = colors[c];
                }

                *p++ = cpu.vic.palette[pixel];
                if(p >= pe) { break; }

                spriteLineData = *spl++;

                if(spriteLineData) {    // Sprite: Ja
                    uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                    bool spritePriority = spritePriorityFromLineData(spriteLineData);

                    pixel = spritePixelFromLineData(spriteLineData);

                    if(spritePriority) {  // MxDP = 1
                        if(c & 0x02) {  //Vordergrundpixel ist gesetzt
                            cpu.vic.fgcollision |= spriteBit;
                            pixel = colors[c];
                        }
                    } else {          // MxDP = 0
                        if(c & 0x02) {
                            cpu.vic.fgcollision |= spriteBit;
                        } //Vordergundpixel ist gesetzt
                    }
                } else { // Kein Sprite
                    pixel = colors[c];
                }

                *p++ = cpu.vic.palette[pixel];
            }
        } while(p < pe);
        PRINTOVERFLOWS
    } else { //Keine Sprites
        while(p < pe - 8) {
            colors[0] = cpu.vic.palette[cpu.vic.R[VIC_B0C]];

            if(cpu.vic.idle) {
                cpu_clock(1);
                colors[1] = colors[2] = colors[3] = 0;
                chr = cpu.RAM[cpu.vic.bank + 0x3fff];
            } else {
                BADLINE(x);

                uint8_t t = cpu.vic.lineMemChr[x];
                colors[1] = cpu.vic.palette[t >> 4];//10
                colors[2] = cpu.vic.palette[t & 0x0f]; //01
                colors[3] = cpu.vic.palette[cpu.vic.lineMemCol[x]];
                chr = bP[x * 8];
            }

            x++;
            pixel = colors[(chr >> 6) & 0x03];
            *p++ = pixel;
            *p++ = pixel;
            pixel = colors[(chr >> 4) & 0x03];
            *p++ = pixel;
            *p++ = pixel;
            pixel = colors[(chr >> 2) & 0x03];
            *p++ = pixel;
            *p++ = pixel;
            pixel = colors[chr & 0x03];
            *p++ = pixel;
            *p++ = pixel;
        }
        while(p < pe) {
            colors[0] = cpu.vic.palette[cpu.vic.R[VIC_B0C]];

            if(cpu.vic.idle) {
                cpu_clock(1);
                colors[1] = colors[2] = colors[3] = 0;
                chr = cpu.RAM[cpu.vic.bank + 0x3fff];
            } else {
                BADLINE(x);

                uint8_t t = cpu.vic.lineMemChr[x];
                colors[1] = cpu.vic.palette[t >> 4];//10
                colors[2] = cpu.vic.palette[t & 0x0f]; //01
                colors[3] = cpu.vic.palette[cpu.vic.lineMemCol[x]];
                chr = bP[x * 8];
            }

            x++;
            pixel = colors[(chr >> 6) & 0x03];
            *p++ = pixel;
            if(p >= pe) { break; }
            *p++ = pixel;
            if(p >= pe) { break; }
            pixel = colors[(chr >> 4) & 0x03];
            *p++ = pixel;
            if(p >= pe) { break; }
            *p++ = pixel;
            if(p >= pe) { break; }
            pixel = colors[(chr >> 2) & 0x03];
            *p++ = pixel;
            if(p >= pe) { break; }
            *p++ = pixel;
            if(p >= pe) { break; }
            pixel = colors[chr & 0x03];
            *p++ = pixel;
            if(p >= pe) { break; }
            *p++ = pixel;
        }
        PRINTOVERFLOW
    }
    while(x < 40) {
        BADLINE(x);
        x++;
    }
}

/*****************************************************************************************************/
static void mode4(tpixel *p, const tpixel *pe, uint16_t const *spl, const uint16_t vc) {
    /*
        3.7.3.5. ECM text mode (ECM/BMM/MCM=1/0/0)
        ------------------------------------------

        This text mode is the same as the standard text mode, but it allows the
        selection of one of four background colors for every single character. The
        selection is done with the upper two bits of the character pointer. This,
        however, reduces the character set from 256 to 64 characters.

        c-access

         Addresses

         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         |VM13|VM12|VM11|VM10| VC9| VC8| VC7| VC6| VC5| VC4| VC3| VC2| VC1| VC0|
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

         Data

         +----+----+----+----+----+----+----+----+----+----+----+----+
         | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+
         |     Color of      |Back.col.| D5 | D4 | D3 | D2 | D1 | D0 |
         |    "1" pixels     |selection|    |    |    |    |    |    |
         +-------------------+---------+----+----+----+----+----+----+

        g-access

         Addresses

         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         |CB13|CB12|CB11|  0 |  0 | D5 | D4 | D3 | D2 | D1 | D0 | RC2| RC1| RC0|
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

         Data

         +----+----+----+----+----+----+----+----+
         |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+
         |         8 pixels (1 bit/pixel)        |
         |                                       |
         | "0": Depending on bits 6/7 of c-data  |
         |      00: Background color 0 ($d021)   |
         |      01: Background color 1 ($d022)   |
         |      10: Background color 2 ($d023)   |
         |      11: Background color 3 ($d024)   |
         | "1": Color from bits 8-11 of c-data   |
         +---------------------------------------+
    */

    uint8_t chr, pixel;
    uint16_t fgcol;
    uint16_t bgcol;
    uint8_t x = 0;

    CHARSETPTR();
    if(cpu.vic.lineHasSprites) {
        do {
            BADLINE(x);

            uint32_t td = cpu.vic.lineMemChr[x];
            bgcol = cpu.vic.R[VIC_B0C + ((td >> 6) & 0x03)];
            chr = cpu.vic.charsetPtr[(td & 0x3f) * 8];
            fgcol = cpu.vic.lineMemCol[x];

            x++;

            unsigned m = min(8, pe - p);

            for(unsigned i = 0; i < m; i++) {
                uint16_t spriteLineData = *spl++;

                if(spriteLineData) {     // Sprite: Ja
                    uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                    bool spritePriority = spritePriorityFromLineData(spriteLineData);

                    pixel = spritePixelFromLineData(spriteLineData);

                    if(spritePriority) {   // Sprite: Hinter Text
                        if(chr & 0x80) {
                            cpu.vic.fgcollision |= spriteBit;
                            pixel = fgcol;
                        } else { pixel = bgcol; }
                    } else {              // Sprite: Vor Text
                        if(chr & 0x80) {
                            cpu.vic.fgcollision |= spriteBit;
                        }
                    }
                } else {                // Kein Sprite
                    pixel = (chr & 0x80) ? fgcol : bgcol;
                }

                chr = chr << 1;
                *p++ = cpu.vic.palette[pixel];
            }
        } while(p < pe);
        PRINTOVERFLOWS
    } else { //Keine Sprites
        while(p < pe - 8) {
            BADLINE(x);

            uint32_t td = cpu.vic.lineMemChr[x];
            bgcol = cpu.vic.palette[cpu.vic.R[VIC_B0C + ((td >> 6) & 0x03)]];
            chr = cpu.vic.charsetPtr[(td & 0x3f) * 8];
            fgcol = cpu.vic.palette[cpu.vic.lineMemCol[x]];
            x++;

            *p++ = (chr & 0x80) ? fgcol : bgcol;
            *p++ = (chr & 0x40) ? fgcol : bgcol;
            *p++ = (chr & 0x20) ? fgcol : bgcol;
            *p++ = (chr & 0x10) ? fgcol : bgcol;
            *p++ = (chr & 0x08) ? fgcol : bgcol;
            *p++ = (chr & 0x04) ? fgcol : bgcol;
            *p++ = (chr & 0x02) ? fgcol : bgcol;
            *p++ = (chr & 0x01) ? fgcol : bgcol;
        }
    }

    while(p < pe) {
        BADLINE(x);

        uint32_t td = cpu.vic.lineMemChr[x];
        bgcol = cpu.vic.palette[cpu.vic.R[VIC_B0C + ((td >> 6) & 0x03)]];
        chr = cpu.vic.charsetPtr[(td & 0x3f) * 8];
        fgcol = cpu.vic.palette[cpu.vic.lineMemCol[x]];

        x++;

        *p++ = (chr & 0x80) ? fgcol : bgcol;
        if(p >= pe) { break; }
        *p++ = (chr & 0x40) ? fgcol : bgcol;
        if(p >= pe) { break; }
        *p++ = (chr & 0x20) ? fgcol : bgcol;
        if(p >= pe) { break; }
        *p++ = (chr & 0x10) ? fgcol : bgcol;
        if(p >= pe) { break; }
        *p++ = (chr & 0x08) ? fgcol : bgcol;
        if(p >= pe) { break; }
        *p++ = (chr & 0x04) ? fgcol : bgcol;
        if(p >= pe) { break; }
        *p++ = (chr & 0x02) ? fgcol : bgcol;
        if(p >= pe) { break; }
        *p++ = (chr & 0x01) ? fgcol : bgcol;
    }

    PRINTOVERFLOW

    while(x < 40) {
        BADLINE(x);
        x++;
    }
}

/*****************************************************************************************************/
/* Ungültige Modi ************************************************************************************/
/*****************************************************************************************************/

static void mode5(tpixel *p, const tpixel *pe, uint16_t const *spl, const uint16_t vc) {
    /*
        3.7.3.5. ECM text mode (ECM/BMM/MCM=1/0/0)
        ------------------------------------------

        This text mode is the same as the standard text mode, but it allows the
        selection of one of four background colors for every single character. The
        selection is done with the upper two bits of the character pointer. This,
        however, reduces the character set from 256 to 64 characters.

        c-access

         Addresses

         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         |VM13|VM12|VM11|VM10| VC9| VC8| VC7| VC6| VC5| VC4| VC3| VC2| VC1| VC0|
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

         Data

         +----+----+----+----+----+----+----+----+----+----+----+----+
         | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+
         |     Color of      |Back.col.| D5 | D4 | D3 | D2 | D1 | D0 |
         |    "1" pixels     |selection|    |    |    |    |    |    |
         +-------------------+---------+----+----+----+----+----+----+

        g-access

         Addresses

         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         |CB13|CB12|CB11|  0 |  0 | D5 | D4 | D3 | D2 | D1 | D0 | RC2| RC1| RC0|
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

         Data

         +----+----+----+----+----+----+----+----+
         |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+
         |         8 pixels (1 bit/pixel)        |
         |                                       |
         | "0": Depending on bits 6/7 of c-data  |
         |      00: Background color 0 ($d021)   |
         |      01: Background color 1 ($d022)   |
         |      10: Background color 2 ($d023)   |
         |      11: Background color 3 ($d024)   |
         | "1": Color from bits 8-11 of c-data   |
         +---------------------------------------+
     */
    CHARSETPTR();

    uint8_t chr, pixel;
    uint16_t fgcol;
    uint8_t x = 0;

    if(cpu.vic.lineHasSprites) {
        do {
            BADLINE(x);

            chr = cpu.vic.charsetPtr[(cpu.vic.lineMemChr[x] & 0x3F) * 8];
            fgcol = cpu.vic.lineMemCol[x];

            x++;

            if((fgcol & 0x08) == 0) { //Zeichen ist HIRES

                unsigned m = min(8, pe - p);
                for(unsigned i = 0; i < m; i++) {

                    uint16_t spriteLineData = *spl++;

                    if(spriteLineData) {     // Sprite: Ja
                        /*
                          Sprite-Prioritäten (Anzeige)
                          MxDP = 1: Grafikhintergrund, Sprite, Vordergrund
                          MxDP = 0: Grafikhintergrund, Vordergrund, Sprite

                          Kollision:
                          Eine Kollision zwischen Sprites und anderen Grafikdaten wird erkannt,
                          sobald beim Bildaufbau ein nicht transparentes Spritepixel und ein Vordergrundpixel ausgegeben wird.

                        */
                        uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                        bool spritePriority = spritePriorityFromLineData(spriteLineData);

                        pixel = spritePixelFromLineData(spriteLineData);


                        if(spritePriority) {   // MxDP = 1
                            if(chr & 0x80) { //Vordergrundpixel ist gesetzt
                                cpu.vic.fgcollision |= spriteBit;
                                pixel = 0;
                            }
                        } else {            // MxDP = 0
                            if(chr & 0x80) {
                                cpu.vic.fgcollision |= spriteBit;
                            } //Vordergrundpixel ist gesetzt
                        }
                    } else {            // Kein Sprite
                        pixel = 0;
                    }

                    *p++ = cpu.vic.palette[pixel];

                    chr = chr << 1;
                }
            } else {//Zeichen ist MULTICOLOR
                for(unsigned i = 0; i < 4; i++) {
                    if(p >= pe) {
                        break;
                    }

                    chr = chr << 2;

                    uint16_t spriteLineData = *spl++;

                    if(spriteLineData) {    // Sprite: Ja
                        uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                        bool spritePriority = spritePriorityFromLineData(spriteLineData);

                        pixel = spritePixelFromLineData(spriteLineData);

                        if(spritePriority) {  // MxDP = 1
                            if(chr & 0x80) {  //Vordergrundpixel ist gesetzt
                                cpu.vic.fgcollision |= spriteBit;
                                pixel = 0;
                            }
                        } else {          // MxDP = 0
                            if(chr & 0x80) {
                                cpu.vic.fgcollision |= spriteBit;
                            } //Vordergrundpixel ist gesetzt
                        }
                    } else { // Kein Sprite
                        pixel = 0;
                    }

                    *p++ = cpu.vic.palette[pixel];

                    if(p >= pe) {
                        break;
                    }

                    spriteLineData = *spl++;

                    //Das gleiche nochmal für das nächste Pixel
                    if(spriteLineData) {    // Sprite: Ja
                        uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                        bool spritePriority = spritePriorityFromLineData(spriteLineData);

                        pixel = spritePixelFromLineData(spriteLineData);

                        if(spritePriority) {  // MxDP = 1
                            if(chr & 0x80) { //Vordergrundpixel ist gesetzt
                                cpu.vic.fgcollision |= spriteBit;
                                pixel = 0;
                            }
                        } else {          // MxDP = 0
                            if(chr & 0x80) {
                                cpu.vic.fgcollision |= spriteBit;
                            } //Vordergrundpixel ist gesetzt
                        }
                    } else { // Kein Sprite
                        pixel = 0;
                    }
                    *p++ = cpu.vic.palette[pixel];
                }
            }
        } while(p < pe);

        PRINTOVERFLOWS
    } else { //Keine Sprites
        //Farbe immer schwarz
        const uint16_t bgcol = cpu.vic.palette[0];

        while(p < pe - 8) {
            BADLINE(x);

            x++;

            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
        }

        while(p < pe) {
            BADLINE(x);

            x++;

            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
        }
        PRINTOVERFLOW
    }

    while(x < 40) {
        BADLINE(x);
        x++;
    }
}

/*****************************************************************************************************/
static void mode6(tpixel *p, const tpixel *pe, uint16_t const *spl, const uint16_t vc) {
    /*
        3.7.3.7. Invalid bitmap mode 1 (ECM/BMM/MCM=1/1/0)
        --------------------------------------------------

        This mode also only displays a black screen, but the pixels can also be
        read out with the sprite collision trick.

        The structure of the graphics is basically as in standard bitmap mode, but
        the bits 9 and 10 of the g-addresses are always zero due to the set ECM bit
        and so the graphics is - roughly said - made up of four "sections" that are
        each repeated four times.

        c-access

         Addresses

         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         |VM13|VM12|VM11|VM10| VC9| VC8| VC7| VC6| VC5| VC4| VC3| VC2| VC1| VC0|
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

         Data

         +----+----+----+----+----+----+----+----+----+----+----+----+
         | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+
         |                           unused                          |
         +-----------------------------------------------------------+

        g-access

         Addresses

         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         |CB13| VC9| VC8|  0 |  0 | VC5| VC4| VC3| VC2| VC1| VC0| RC2| RC1| RC0|
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

         Data

         +----+----+----+----+----+----+----+----+
         |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+
         |         8 pixels (1 bit/pixel)        |
         |                                       |
         | "0": Black (background)               |
         | "1": Black (foreground)               |
         +---------------------------------------+
     */

    uint8_t chr, pixel;
    uint8_t x = 0;
    uint8_t *bP = cpu.vic.bitmapPtr + vc * 8 + cpu.vic.rc;

    if(cpu.vic.lineHasSprites) {

        do {
            BADLINE(x);

            chr = bP[x * 8];

            x++;

            unsigned m = min(8, pe - p);

            for(unsigned i = 0; i < m; i++) {
                uint16_t spriteLineData = *spl++;

                chr = chr << 1;

                if(spriteLineData) {     // Sprite: Ja
                    /*
                       Sprite-Prioritäten (Anzeige)
                       MxDP = 1: Grafikhintergrund, Sprite, Vordergrund
                       MxDP = 0: Grafikhintergung, Vordergrund, Sprite

                       Kollision:
                       Eine Kollision zwischen Sprites und anderen Grafikdaten wird erkannt,
                       sobald beim Bildaufbau ein nicht transparentes Spritepixel und ein Vordergrundpixel ausgegeben wird.

                    */
                    uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                    bool spritePriority = spritePriorityFromLineData(spriteLineData);

                    pixel = spritePixelFromLineData(spriteLineData);

                    if(spritePriority) {   // MxDP = 1
                        if(chr & 0x80) { //Vordergrundpixel ist gesetzt
                            cpu.vic.fgcollision |= spriteBit;
                            pixel = 0;
                        }
                    } else {            // MxDP = 0
                        if(chr & 0x80) {
                            cpu.vic.fgcollision |= spriteBit;
                        } //Vordergrundpixel ist gesetzt
                    }
                } else {            // Kein Sprite
                    pixel = 0;
                }

                *p++ = cpu.vic.palette[pixel];
            }
        } while(p < pe);
        PRINTOVERFLOWS
    } else { //Keine Sprites
        //Farbe immer schwarz
        const uint16_t bgcol = cpu.vic.palette[0];
        while(p < pe - 8) {
            BADLINE(x);

            x++;

            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
        }

        while(p < pe) {
            BADLINE(x);

            x++;

            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
        }

        PRINTOVERFLOW
    }

    while(x < 40) {
        BADLINE(x);

        x++;
    }
}

/*****************************************************************************************************/
static void mode7(tpixel *p, const tpixel *pe, uint16_t const *spl, const uint16_t vc) {
    /*
        3.7.3.8. Invalid bitmap mode 2 (ECM/BMM/MCM=1/1/1)
        --------------------------------------------------

        The last invalid mode also creates a black screen but it can also be
        "scanned" with sprite-graphics collisions.

        The structure of the graphics is basically as in multicolor bitmap mode,
        but the bits 9 and 10 of the g-addresses are always zero due to the set ECM
        bit, with the same results as in the first invalid bitmap mode. As usual,
        the bit combination "01" is part of the background.

        c-access

         Addresses

         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         |VM13|VM12|VM11|VM10| VC9| VC8| VC7| VC6| VC5| VC4| VC3| VC2| VC1| VC0|
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

         Data

         +----+----+----+----+----+----+----+----+----+----+----+----+
         | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+
         |                           unused                          |
         +-----------------------------------------------------------+

        g-access

         Addresses

         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
         |CB13| VC9| VC8|  0 |  0 | VC5| VC4| VC3| VC2| VC1| VC0| RC2| RC1| RC0|
         +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

         Data

         +----+----+----+----+----+----+----+----+
         |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
         +----+----+----+----+----+----+----+----+
         |         4 pixels (2 bits/pixel)       |
         |                                       |
         | "00": Black (background)              |
         | "01": Black (background)              |
         | "10": Black (foreground)              |
         | "11": Black (foreground)              |
         +---------------------------------------+
     */

    uint8_t chr;
    uint8_t x = 0;
    uint8_t pixel;
    uint8_t *bP = cpu.vic.bitmapPtr + vc * 8 + cpu.vic.rc;

    if(cpu.vic.lineHasSprites) {
        do {
            BADLINE(x);

            chr = bP[x * 8];
            x++;

            for(unsigned i = 0; i < 4; i++) {
                if(p >= pe) { break; }

                chr = chr << 2;

                uint16_t spriteLineData = *spl++;

                if(spriteLineData) {    // Sprite: Ja
                    uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                    bool spritePriority = spritePriorityFromLineData(spriteLineData);

                    pixel = spritePixelFromLineData(spriteLineData);

                    if(spritePriority) {  // MxDP = 1
                        if(chr & 0x80) {  //Vordergrundpixel ist gesetzt
                            cpu.vic.fgcollision |= spriteBit;
                            pixel = 0;
                        }
                    } else {          // MxDP = 0
                        if(chr & 0x80) {
                            cpu.vic.fgcollision |= spriteBit;
                        } //Vordergundpixel ist gesetzt
                    }
                } else { // Kein Sprite
                    pixel = 0;
                }

                *p++ = cpu.vic.palette[pixel];

                if(p >= pe) { break; }

                spriteLineData = *spl++;

                if(spriteLineData) {    // Sprite: Ja
                    uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                    bool spritePriority = spritePriorityFromLineData(spriteLineData);

                    pixel = spritePixelFromLineData(spriteLineData);

                    if(spritePriority) {  // MxDP = 1
                        if(chr & 0x80) {  //Vordergrundpixel ist gesetzt
                            cpu.vic.fgcollision |= spriteBit;
                            pixel = 0;
                        }
                    } else {          // MxDP = 0
                        if(chr & 0x80) {
                            cpu.vic.fgcollision |= spriteBit;
                        } //Vordergundpixel ist gesetzt
                    }
                } else { // Kein Sprite
                    pixel = 0;
                }

                *p++ = cpu.vic.palette[pixel];
            }
        } while(p < pe);

        PRINTOVERFLOWS
    } else { //Keine Sprites
        const uint16_t bgcol = cpu.vic.palette[0];

        while(p < pe - 8) {
            BADLINE(x);

            x++;

            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
            *p++ = bgcol;
        }

        while(p < pe) {
            BADLINE(x);

            x++;

            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
            if(p >= pe) { break; }
            *p++ = bgcol;
        }

        PRINTOVERFLOW
    }

    while(x < 40) {
        BADLINE(x);

        x++;
    }
}
/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/

using modes_t = void (*)(tpixel *p, const tpixel *pe, uint16_t const *spl, const uint16_t vc); //Funktionspointer

static const modes_t modes[8] = {mode0, mode1, mode2, mode3, mode4, mode5, mode6, mode7};


void tvic::render() {
    uint16_t vc;
    uint16_t xscroll;
    tpixel *pe;
    tpixel *p;
    uint16_t *spl;
    uint8_t mode;

    /*****************************************************************************************************/
    /* Linecounter ***************************************************************************************/
    /*****************************************************************************************************/

    if(cpu.vic.rasterLine >= LINECNT) {
        //reSID sound needs much time - too much to keep everything in sync and with stable refreshrate
        //but it is not called very often, so most of the time, we have more time than needed.
        //We can measure the time needed for a frame and calc a correction factor to speed things up.
        unsigned long m = cycleCountMicros();

        cpu.vic.neededTime = (m - cpu.vic.timeStart);
        cpu.vic.timeStart = m;
        cpu.vic.lineClock.update(LINETIMER_DEFAULT_FREQ -
                                 ((float) cpu.vic.neededTime / (float) LINECNT - LINETIMER_DEFAULT_FREQ));
        cpu.vic.rasterLine = 0;
        cpu.vic.vcbase = 0;
        cpu.vic.denLatch = 0;
    } else {
        cpu.vic.rasterLine++;
    }

    int rasterLine = cpu.vic.rasterLine;

    if(rasterLine == cpu.vic.intRasterLine) {//Set Rasterline-Interrupt
        cpu.vic.R[VIC_IRQST] |= VIC_IRQST_IRST | (cpu.vic.R[VIC_IRQEN] & VIC_IRQEN_ERST ? VIC_IRQST_IRQ : 0);
    }

    /*****************************************************************************************************/
    /* Badlines ******************************************************************************************/
    /*****************************************************************************************************/
    /*
        A Bad Line Condition is given at any arbitrary clock cycle, if at the
        negative edge of ø0 at the beginning of the cycle RASTER >= $30 and RASTER
        <= $f7 and the lower three bits of RASTER are equal to YSCROLL and if the
        DEN bit was set during an arbitrary cycle of raster line $30.
    */

    if(rasterLine == 0x30) { cpu.vic.denLatch |= cpu.vic.r.DEN; }

    /* 3.7.2
        2. In the first phase of cycle 14 of each line, VC is loaded from VCBASE
           (VCBASE->VC) and VMLI is cleared. If there is a Bad Line Condition in
           this phase, RC is also reset to zero.
    */

    vc = cpu.vic.vcbase;

    cpu.vic.badline = (cpu.vic.denLatch && (rasterLine >= 0x30) && (rasterLine <= 0xf7) && ((rasterLine & 0x07) == cpu.vic.r.YSCROLL));

    if(cpu.vic.badline) {
        cpu.vic.idle = 0;
        cpu.vic.rc = 0;
    }

    /*****************************************************************************************************/
    /*****************************************************************************************************/
#if 1
    {
        int t = MAXCYCLESSPRITES3_7 - cpu.vic.spriteCycles3_7;
        if(t > 0) { cpu_clock(t); }
        if(cpu.vic.spriteCycles3_7 > 0) { cia_clockt(cpu.vic.spriteCycles3_7); }
    }
#endif

    //HBlank:
    cpu_clock(10);

#ifdef ADDITIONALCYCLES
    cpu_clock(ADDITIONALCYCLES);
#endif

    /*
        RSEL|  Display window height   | First line  | Last line
         ----+--------------------------+-------------+----------
           0 | 24 text lines/192 pixels |   55 ($37)  | 246 ($f6)
           1 | 25 text lines/200 pixels |   51 ($33)  | 250 ($fa)
    */

    if(cpu.vic.borderFlag) {
        int firstLine = (cpu.vic.r.RSEL) ? 0x33 : 0x37;
        if((cpu.vic.r.DEN) && (rasterLine == firstLine)) { cpu.vic.borderFlag = false; }
    } else {
        int lastLine = (cpu.vic.r.RSEL) ? 0xfb : 0xf7;
        if(rasterLine == lastLine) { cpu.vic.borderFlag = true; }
    }

    if(rasterLine < FIRSTDISPLAYLINE || rasterLine > LASTDISPLAYLINE) {
        if(rasterLine == 0) {
            cpu_clock(CYCLESPERRASTERLINE - 10 - 2 - MAXCYCLESSPRITES - 1); // (minus hblank l + rasterLine)
        } else {
            cpu_clock(CYCLESPERRASTERLINE - 10 - 2 - MAXCYCLESSPRITES);
        }
        goto noDisplayIncRC;
    }

    //max_x =  (!cpu.vic.CSEL) ? 40:38;
    p = screenMem + (rasterLine - FIRSTDISPLAYLINE) * LINE_MEM_WIDTH;

    pe = p + SCREEN_WIDTH;
    //Left Screenborder: Cycle 10
    spl = &cpu.vic.spriteLine[24];
    cpu_clock(6);


    if(cpu.vic.borderFlag) {
        cpu_clock(5);
        fastFillLineNoSprites(p, pe + BORDER_RIGHT, cpu.vic.palette[cpu.vic.R[VIC_EC]]);
        goto noDisplayIncRC;
    }


    /*****************************************************************************************************/
    /* DISPLAY *******************************************************************************************/
    /*****************************************************************************************************/

    xscroll = cpu.vic.r.XSCROLL;

    if(xscroll > 0) {
        uint16_t col = cpu.vic.palette[cpu.vic.R[VIC_EC]];

        if(!cpu.vic.r.CSEL) {
            cpu_clock(1);
            uint16_t sprite;
            for(int i = 0; i < xscroll; i++) {
                SPRITEORFIXEDCOLOR();
            }
        } else {
            spl += xscroll;
            for(unsigned i = 0; i < xscroll; i++) {
                *p++ = col;
            }
        }
    }

    /*****************************************************************************************************/
    /*****************************************************************************************************/
    /*****************************************************************************************************/


    cpu.vic.fgcollision = 0;
    mode = (cpu.vic.r.ECM << 2) | (cpu.vic.r.BMM << 1) | cpu.vic.r.MCM;

    if(!cpu.vic.idle) {
        modes[mode](p, pe, spl, vc);
        vc = (vc + 40) & 0x3ff;
    } else {
        /*
            3.7.3.9. Idle state
            -------------------

            In idle state, the VIC reads the graphics data from address $3fff (resp.
            $39ff if the ECM bit is set) and displays it in the selected graphics mode,
            but with the video matrix data (normally read in the c-accesses) being all
            "0" bits. So the byte at address $3fff/$39ff is output repeatedly.

            c-access

             No c-accesses occur.

             Data

             +----+----+----+----+----+----+----+----+----+----+----+----+
             | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
             +----+----+----+----+----+----+----+----+----+----+----+----+
             |  0 |  0 |  0 |  0 |  0 |  0 |  0 |  0 |  0 |  0 |  0 |  0 |
             +----+----+----+----+----+----+----+----+----+----+----+----+

            g-access

             Addresses (ECM=0)

             +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
             | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
             +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
             |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |
             +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

             Addresses (ECM=1)

             +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
             | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
             +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
             |  1 |  1 |  1 |  0 |  0 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |
             +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

             Data

             +----+----+----+----+----+----+----+----+
             |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
             +----+----+----+----+----+----+----+----+
             |         8 pixels (1 bit/pixel)        | Standard text mode/
             |                                       | Multicolor text mode/
             | "0": Background color 0 ($d021)       | ECM text mode
             | "1": Black                            |
             +---------------------------------------+
             |         8 pixels (1 bit/pixel)        | Standard bitmap mode/
             |                                       | Invalid text mode/
             | "0": Black (background)               | Invalid bitmap mode 1
             | "1": Black (foreground)               |
             +---------------------------------------+
             |         4 pixels (2 bits/pixel)       | Multicolor bitmap mode
             |                                       |
             | "00": Background color 0 ($d021)      |
             | "01": Black (background)              |
             | "10": Black (foreground)              |
             | "11": Black (foreground)              |
             +---------------------------------------+
             |         4 pixels (2 bits/pixel)       | Invalid bitmap mode 2
             |                                       |
             | "00": Black (background)              |
             | "01": Black (background)              |
             | "10": Black (foreground)              |
             | "11": Black (foreground)              |
             +---------------------------------------+
  */
        //Modes 1 & 3
        if(mode == 1 || mode == 3) {
            modes[mode](p, pe, spl, vc);
        } else {
            fastFillLine(p, pe, cpu.vic.palette[0], spl);
        }
    }

    /*
        For the MBC and MMC interrupts, only the first collision will trigger an
        interrupt (i.e. if the collision registers $d01e resp. $d01f contained the
        value zero before the collision). To trigger further interrupts after a
        collision, the concerning register has to be cleared first by reading from
        it.
     */

    if(cpu.vic.fgcollision) {
        if(cpu.vic.r.MD == 0) {
            cpu.vic.R[VIC_IRQST] |= VIC_IRQST_IMBC | (cpu.vic.R[VIC_IRQEN] & VIC_IRQEN_EMBC ? VIC_IRQST_IRQ : 0);
        }

        cpu.vic.r.MD |= cpu.vic.fgcollision;
    }

    /*****************************************************************************************************/

    if(!cpu.vic.r.CSEL) {
        cpu_clock(1);
        uint16_t col = cpu.vic.palette[cpu.vic.R[VIC_EC]];
        //p = &screen[rasterLine - FIRSTDISPLAYLINE][0];
        p = screenMem + (rasterLine - FIRSTDISPLAYLINE) * LINE_MEM_WIDTH + BORDER_LEFT;
#if 0
        // Sprites im Rand
        uint16_t sprite;
        uint16_t * spl;
        spl = &cpu.vic.spriteLine[24 + xscroll];

        SPRITEORFIXEDCOLOR();
        SPRITEORFIXEDCOLOR();
        SPRITEORFIXEDCOLOR();
        SPRITEORFIXEDCOLOR();
        SPRITEORFIXEDCOLOR();
        SPRITEORFIXEDCOLOR();
        SPRITEORFIXEDCOLOR(); //7
#else
        //keine Sprites im Rand
        *p++ = col;
        *p++ = col;
        *p++ = col;
        *p++ = col;
        *p++ = col;
        *p++ = col;
        *p = col;
#endif

        //Rand rechts:
        //p = &screen[rasterLine - FIRSTDISPLAYLINE][SCREEN_WIDTH - 9];
        p = screenMem + (rasterLine - FIRSTDISPLAYLINE) * LINE_MEM_WIDTH + SCREEN_WIDTH - 9 + BORDER_LEFT;
        pe = p + 9;

#if 0
        // Sprites im Rand
        spl = &cpu.vic.spriteLine[24 + SCREEN_WIDTH - 9 + xscroll];
        while (p < pe) {
          SPRITEORFIXEDCOLOR();
        }
#else
        //keine Sprites im Rand
        while(p < pe) {
            *p++ = col;
        }
#endif
    }


    //Rechter Rand nach CSEL, im Textbereich

    cpu_clock(5);
    noDisplayIncRC:

    /* 3.7.2
        5. In the first phase of cycle 58, the VIC checks if RC=7. If so, the video
           logic goes to idle state and VCBASE is loaded from VC (VC->VCBASE). If
           the video logic is in display state afterwards (this is always the case
           if there is a Bad Line Condition), RC is incremented.
    */

    if(cpu.vic.rc == 7) {
        cpu.vic.idle = 1;
        cpu.vic.vcbase = vc;
    }

    // Ist dies richtig ??
    if((!cpu.vic.idle) || (cpu.vic.denLatch && (rasterLine >= 0x30) && (rasterLine <= 0xf7) && ((rasterLine & 0x07) == cpu.vic.r.YSCROLL))) {
        cpu.vic.rc = (cpu.vic.rc + 1) & 0x07;
    }


    /*****************************************************************************************************/
    /* Sprites *******************************************************************************************/
    /*****************************************************************************************************/

    cpu.vic.spriteCycles0_2 = 0;
    cpu.vic.spriteCycles3_7 = 0;

    if(cpu.vic.lineHasSprites) {
        cpu.vic.lineHasSprites = 0;

        memset(cpu.vic.spriteLine, 0, sizeof(cpu.vic.spriteLine));
    }

    uint32_t spritesEnabled = cpu.vic.R[VIC_MxE];

    if(spritesEnabled) {
        uint8_t spritesYExpanded = cpu.vic.R[VIC_MxYE];
        uint8_t collision = 0;
        uint8_t lastSpriteNum = 0;

        for(uint8_t spriteNum = 0; spriteNum < 8; spriteNum++) {
            if(!spritesEnabled) {
                break;
            }

            uint8_t spriteBit = 1 << spriteNum;

            if(spritesEnabled & spriteBit) {
                spritesEnabled &= ~spriteBit;

                uint8_t spriteYPos = cpu.vic.R[VIC_M0Y + spriteNum * 2];
                bool spriteYExpanded = spritesYExpanded & spriteBit;

                // does the rasterline cross the sprite?
                if(rasterLine >= spriteYPos &&
                   rasterLine < spriteYExpanded ? spriteYPos + 42 : spriteYPos + 21)
                {
                    // Sprite cycles
                    if(spriteNum < 3) {
                        if(!lastSpriteNum) {
                            cpu.vic.spriteCycles0_2 += 1;
                        }

                        cpu.vic.spriteCycles0_2 += 2;
                    } else {
                        if(!lastSpriteNum) {
                            cpu.vic.spriteCycles3_7 += 1;
                        }

                        cpu.vic.spriteCycles3_7 += 2;
                    }

                    lastSpriteNum = spriteNum;

                    if(rasterLine < FIRSTDISPLAYLINE || rasterLine > LASTDISPLAYLINE) {
                        continue;
                    }

                    uint16_t spriteXPos = ((cpu.vic.R[VIC_MxX8] & spriteBit) ? 0x100 : 0) |
                                            cpu.vic.R[VIC_M0X + spriteNum * 2];

                    if(spriteXPos >= SPRITE_MAX_X) {
                        continue;
                    }

                    uint8_t lineOfSprite = rasterLine - spriteYPos;

                    if(spriteYExpanded) {
                        lineOfSprite = lineOfSprite / 2;
                    }

                    // address of sprite line in memory
                    uint16_t spriteAdr = cpu.vic.bank |
                                         cpu.RAM[cpu.vic.videomatrix + (1024 - 8) + spriteNum] << 6 |
                                         (lineOfSprite * 3);

                    // 24 bits of sprite data for the line
                    uint32_t spriteData = ((unsigned) cpu.RAM[spriteAdr] << 16) |
                                          ((unsigned) cpu.RAM[spriteAdr + 1] << 8) |
                                          ((unsigned) cpu.RAM[spriteAdr + 2]);

                    if(!spriteData) {
                        continue;
                    }

                    cpu.vic.lineHasSprites = 1;

                    // "spriteLine" contains 16 bit per pixel for the current raster line.
                    // If bit 15 is set, a sprite is present for that pixel. Bit 14 then specifies whether the
                    // the sprite is to be displayed in front of the graphics (bit 14 = 0) or between foreground
                    // background graphics (bit 14 = 1). Bits 8 to 10 specify the sprite number.
                    // Bits 0 to 3 specify the actual color to be displayes.
                    // Sprites have implicit priorities with sprite 0's priority being the highest.
                    // Thus spriteLine data will only be written if it contains zero. Even for sprite number 0 and color
                    // number 0 will contain 0x8000.
                    uint16_t *slp = &cpu.vic.spriteLine[spriteXPos]; //Sprite-Line-Pointer
                    uint16_t spriteLineupperByte = (0x80 | ((cpu.vic.r.MDP & spriteBit) ? 0x40 : 0) | spriteNum) << 8;

                    //Sprite in Spritezeile schreiben:
                    if((cpu.vic.r.MMC & spriteBit) == 0) { // NO MULTICOLOR
                        // flag + priority + sprite number + color number
                        uint16_t spriteLineData = spriteLineupperByte | cpu.vic.R[VIC_M0C + spriteNum];

                        if((cpu.vic.r.MXE & spriteBit) == 0) { // NO MULTICOLOR, NO SPRITE-X-EXPANSION
                            for(unsigned cnt = 0; spriteData && cnt < 24; cnt++, spriteData <<= 1) {
                                bool pixel = (spriteData >> 23) & 0x01;

                                if(pixel) {
                                    if(*slp == 0) {
                                        *slp = spriteLineData;
                                    } else {
                                        collision |= spriteBit | (1 << ((*slp >> 8) & 0x07));
                                    }
                                }

                                slp++;
                            }
                        } else {    // NO MULTICOLOR, SPRITE-X-EXPANSION
                            for(unsigned cnt = 0; spriteData && cnt < 24; cnt++, spriteData <<= 1) {
                                bool pixel = (spriteData >> 23) & 0x01;

                                // expand by writing twice
                                if(pixel) {
                                    if(*slp == 0) {
                                        *slp = spriteLineData;
                                    } else {
                                        collision |= spriteBit | (1 << ((*slp >> 8) & 0x07));
                                    }

                                    slp++;

                                    if(*slp == 0) {
                                        *slp = spriteLineData;
                                    } else {
                                        collision |= spriteBit | (1 << ((*slp >> 8) & 0x07));
                                    }

                                    slp++;
                                } else {
                                    slp += 2;
                                }
                            }
                        }
                    } else { // MULTICOLOR
                        /*
                            In multicolor mode, two adjacent bits form one pixel, thus reducing the
                            resolution of the sprite to 12×21 (the pixels are twice as wide).
                         */

                        uint16_t colors[4];

                        // color 0 is transparent
                        colors[1] = spriteLineupperByte | cpu.vic.R[VIC_MM0];
                        colors[2] = spriteLineupperByte | cpu.vic.R[VIC_M0C + spriteNum];
                        colors[3] = spriteLineupperByte | cpu.vic.R[VIC_MM1];

                        if((cpu.vic.r.MXE & spriteBit) == 0) { // MULTICOLOR, NO SPRITE-X-EXPANSION
                            for(unsigned cnt = 0; spriteData && cnt < 24; cnt++, spriteData <<= 2) {
                                uint8_t pixel = (spriteData >> 22) & 0x03;

                                // write pixel two times
                                if(pixel) {
                                    if(*slp == 0) {
                                        *slp = colors[pixel];
                                    } else {
                                        collision |= spriteBit | (1 << ((*slp >> 8) & 0x07));
                                    }

                                    slp++;

                                    if(*slp == 0) {
                                        *slp = colors[pixel];
                                    } else {
                                        collision |= spriteBit | (1 << ((*slp >> 8) & 0x07));
                                    }

                                    slp++;
                                } else {
                                    slp += 2;
                                }
                            }
                        } else {    // MULTICOLOR, SPRITE-X-EXPANSION
                            for(unsigned cnt = 0; spriteData && cnt < 24; cnt++, spriteData <<= 2) {
                                uint8_t pixel = (spriteData >> 22) & 0x03;

                                // expand by writing pixel four times
                                if(pixel) {
                                    if(*slp == 0) {
                                        *slp = colors[pixel];
                                    } else {
                                        collision |= spriteBit | (1 << ((*slp >> 8) & 0x07));
                                    }

                                    slp++;

                                    if(*slp == 0) {
                                        *slp = colors[pixel];
                                    } else {
                                        collision |= spriteBit | (1 << ((*slp >> 8) & 0x07));
                                    }

                                    slp++;

                                    if(*slp == 0) {
                                        *slp = colors[pixel];
                                    } else {
                                        collision |= spriteBit | (1 << ((*slp >> 8) & 0x07));
                                    }

                                    slp++;

                                    if(*slp == 0) {
                                        *slp = colors[pixel];
                                    } else {
                                        collision |= spriteBit | (1 << ((*slp >> 8) & 0x07));
                                    }
                                    slp++;
                                } else {
                                    slp += 4;
                                }
                            }
                        }
                    }
                } else {
                    lastSpriteNum = 0;
                }
            }
        }

        if(collision) {
            if(cpu.vic.r.MM == 0) {
                cpu.vic.R[VIC_IRQST] |= VIC_IRQST_IMMC | (cpu.vic.R[VIC_IRQEN] & VIC_IRQEN_EMMC ? VIC_IRQST_IRQ : 0);
            }

            cpu.vic.r.MM |= collision;
        }
    }
    /*****************************************************************************************************/
#if 0
    {
      int t = MAXCYCLESSPRITES0_2 - cpu.vic.spriteCycles0_2;
      if (t > 0) cpu_clock(t);
      if (cpu.vic.spriteCycles0_2 > 0) cia_clockt(cpu.vic.spriteCycles0_2);
    }
#endif

    //HBlank:
#if PAL
    cpu_clock(2);
#else
    cpu_clock(3);
#endif
}

/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/

static void dim() {
    for(int i = 0; i < ILI9341_TFTWIDTH * ILI9341_TFTHEIGHT; i++) {
        auto p = screenMem[i];
        screenMem[i] = (p & 0b1110000000000000) >> 2 | (p & 0b0000011110000000) >> 2 | (p & 0b0000000000011100) >> 2;
    }
}

void tvic::displaySimpleModeScreen() {
    dim();

    tft.setFont(Play_60_Bold);
    tft.setTextColor(0xffff);
    tft.setCursor(25, 40);
    tft.print("IEC");
    tft.setCursor(25, 130);
    tft.print("Access");
}

void tvic::renderSimple() {
    uint16_t vc;
    int cycles = 0;

    if(cpu.vic.rasterLine >= LINECNT) {
        //reSID sound needs much time - too much to keep everything in sync and with stable refreshrate
        //but it is not called very often, so most of the time, we have more time than needed.
        //We can measure the time needed for a frame and calc a correction factor to speed things up.
        uint32_t m = cycleCountMicros();
        cpu.vic.neededTime = (m - cpu.vic.timeStart);
        cpu.vic.timeStart = m;

        cpu.vic.lineClock.update(LINETIMER_DEFAULT_FREQ -
                                 ((float)cpu.vic.neededTime / (float)LINECNT - LINETIMER_DEFAULT_FREQ));

        cpu.vic.rasterLine = 0;
        cpu.vic.vcbase = 0;
        cpu.vic.denLatch = 0;
    } else {
        cpu.vic.rasterLine++;
        cpu_clock(1);
        cycles += 1;
    }

    int r = cpu.vic.rasterLine;

    if(r == cpu.vic.intRasterLine) { // rasterline interrupt
        cpu.vic.R[VIC_IRQST] |= VIC_IRQST_IRST | (cpu.vic.R[VIC_IRQEN] & VIC_IRQEN_ERST ? VIC_IRQST_IRQ : 0);
    }

    cpu_clock(9);
    cycles += 9;

    if(r == 0x30) {
        cpu.vic.denLatch |= cpu.vic.r.DEN;
    }

    vc = cpu.vic.vcbase;

    cpu.vic.badline = (cpu.vic.denLatch && (r >= 0x30) && (r <= 0xf7) && ((r & 0x07) == cpu.vic.r.YSCROLL));

    if(cpu.vic.badline) {
        cpu.vic.idle = 0;
        cpu.vic.rc = 0;
    }


    /* Rand oben /unten **********************************************************************************/
    /*
        RSEL|  Display window height   | First line  | Last line
         ----+--------------------------+-------------+----------
           0 | 24 text lines/192 pixels |   55 ($37)  | 246 ($f6)
           1 | 25 text lines/200 pixels |   51 ($33)  | 250 ($fa)
     */

    if(cpu.vic.borderFlag) {
        int firstLine = (cpu.vic.r.RSEL) ? 0x33 : 0x37;

        if(cpu.vic.r.DEN && r == firstLine) {
            cpu.vic.borderFlag = false;
        }
    } else {
        int lastLine = (cpu.vic.r.RSEL) ? 0xfb : 0xf7;

        if(r == lastLine) {
            cpu.vic.borderFlag = true;
        }
    }

    //left screenborder
    cpu_clock(6);

    cycles += 6;
    CYCLES(40);
    cycles += 40;
    vc += 40;

    //right screenborder
    cpu_clock(4);
    cycles += 4;

    if(cpu.vic.rc == 7) {
        cpu.vic.idle = 1;
        cpu.vic.vcbase = vc;
    }

    //Ist dies richtig ??
    if((!cpu.vic.idle) || (cpu.vic.denLatch && (r >= 0x30) && (r <= 0xf7) && ((r & 0x07) == cpu.vic.r.YSCROLL))) {
        cpu.vic.rc = (cpu.vic.rc + 1) & 0x07;
    }

    cpu_clock(3); //1
    cycles += 3;

    int cyclesleft = CYCLESPERRASTERLINE - cycles;

    if(cyclesleft) {
        cpu_clock(cyclesleft);
    }
}

/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/

void tvic::applyAdressChange() {
    uint8_t r18 = cpu.vic.R[VIC_VM_CB];
    cpu.vic.videomatrix = cpu.vic.bank + (unsigned) (r18 & VIC_VM_CB_VM_MASK) * 64;

    unsigned charsetAddr = r18 & VIC_VM_CB_CB_MASK;
    if((cpu.vic.bank & 0x4000) == 0) {
        if(charsetAddr == 0x04) {
            cpu.vic.charsetPtrBase = ((uint8_t *) &rom_characters);
        } else if(charsetAddr == 0x06) {
            cpu.vic.charsetPtrBase = ((uint8_t *) &rom_characters) + 0x800;
        } else {
            cpu.vic.charsetPtrBase = &cpu.RAM[charsetAddr * 0x400 + cpu.vic.bank];
        }
    } else {
        cpu.vic.charsetPtrBase = &cpu.RAM[charsetAddr * 0x400 + cpu.vic.bank];
    }

    cpu.vic.bitmapPtr = (uint8_t *) &cpu.RAM[cpu.vic.bank | ((r18 & 0x08) * 0x400)];

    if((cpu.vic.R[VIC_CR1] & VIC_CR1_xxM_MASK) == VIC_CR1_xxM_MASK) {
        cpu.vic.bitmapPtr = (uint8_t *) ((uintptr_t) cpu.vic.bitmapPtr & 0xf9ff);
    }
}

/*****************************************************************************************************/
void tvic::write(uint32_t address, uint8_t value) {
    address &= 0x3F;

    switch(address) {
        case VIC_CR1:
            cpu.vic.R[VIC_CR1] = value;
            cpu.vic.intRasterLine = (cpu.vic.intRasterLine & 0xff) | (value & VIC_CR1_RST8 ? 0x100 : 0);

            if(cpu.vic.rasterLine == 0x30) {
                cpu.vic.denLatch |= value & VIC_CR1_DEN;
            }

            cpu.vic.badline = (cpu.vic.denLatch
                               && (cpu.vic.rasterLine >= 0x30)
                               && (cpu.vic.rasterLine <= 0xf7)
                               && ((cpu.vic.rasterLine & 0x07) == (value & VIC_CR1_YSCROLL_MASK)));

            if(cpu.vic.badline) {
                cpu.vic.idle = 0;
            }
            applyAdressChange();

            break;

        case VIC_RASTER:
            cpu.vic.intRasterLine = (cpu.vic.intRasterLine & 0x100) | value;
            cpu.vic.R[VIC_RASTER] = value;
            break;

        case VIC_VM_CB:
            cpu.vic.R[VIC_VM_CB] = value;
            applyAdressChange();
            break;

        case VIC_IRQST:
            cpu.vic.R[VIC_IRQST] &= (~value & VIC_IRQST_MASK);
            break;

        case VIC_IRQEN:
            cpu.vic.R[VIC_IRQEN] = value & ~VIC_IRQEN_UNUSED_MASK;
            break;

        case VIC_MxM:
        case VIC_MxD:
            cpu.vic.R[address] = 0;
            break;

        case VIC_EC ... VIC_M7C:
            cpu.vic.R[address] = value & ~VIC_xC_UNUSED_MASK;
            break;

        case 0x2F ... 0x3F:
            break;

        default:
            cpu.vic.R[address] = value;
            break;
    }
}

/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/

uint8_t tvic::read(uint32_t address) {
    uint8_t ret;

    address &= 0x3F;
    switch(address) {
        case VIC_CR1:
            ret = (cpu.vic.R[VIC_CR1] & 0x7F) | ((cpu.vic.rasterLine & 0x100) >> 1); // kfl02: ???
            break;

        case VIC_RASTER:
            ret = cpu.vic.rasterLine;
            break;

        case VIC_CR2:
            ret = cpu.vic.R[VIC_CR2] | VIC_CR2_UNUSED_MASK;
            break;

        case VIC_VM_CB:
            ret = cpu.vic.R[VIC_VM_CB] | VIC_VM_CB_UNUSED_MASK;
            break;

        case VIC_IRQST:
            ret = cpu.vic.R[VIC_IRQST] | VIC_IRQST_UNUSED_MASK;
            break;

        case VIC_IRQEN:
            ret = cpu.vic.R[VIC_IRQEN] | VIC_IRQEN_UNUSED_MASK;
            break;

        case VIC_MxM:
        case VIC_MxD:
            ret = cpu.vic.R[address];
            cpu.vic.R[address] = 0;
            break;

        case VIC_EC ... VIC_M7C:
            ret = cpu.vic.R[address] | VIC_xC_UNUSED_MASK;
            break;

        case 0x2F ... 0x3F:
            ret = 0xFF;
            break;

        default:
            ret = cpu.vic.R[address];
            break;
    }

    return ret;
}

/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/

void tvic::reset() {

    enableCycleCounter();

    cpu.vic.intRasterLine = 0;
    cpu.vic.rasterLine = 0;
    cpu.vic.lineHasSprites = 0;
    memset(&cpu.RAM[0x400], 0, 1000);
    memset(&cpu.vic.R, 0, sizeof(cpu.vic.R));
    updatePalette(0);

    //http://dustlayer.com/vic-ii/2013/4/22/when-visibility-matters
    cpu.vic.R[VIC_CR1] = 0x9B;
    cpu.vic.R[VIC_CR2] = 0x08;
    cpu.vic.R[VIC_VM_CB] = 0x14;
    cpu.vic.R[VIC_IRQST] = 0x0f;

    for(unsigned char & i : cpu.vic.colorRAM) {
        i = (rand() & 0x0F);
    }

    cpu.RAM[0x39FF] = 0x0;
    cpu.RAM[0x3FFF] = 0x0;
    cpu.RAM[0x39FF + 16384] = 0x0;
    cpu.RAM[0x3FFF + 16384] = 0x0;
    cpu.RAM[0x39FF + 32768] = 0x0;
    cpu.RAM[0x3FFF + 32768] = 0x0;
    cpu.RAM[0x39FF + 49152] = 0x0;
    cpu.RAM[0x3FFF + 49152] = 0x0;

    applyAdressChange();
}

void tvic::updatePalette(int n) {
    paletteNo = n % (numPalettes * numPaletteFns);

    int p = paletteNo % numPalettes;
    int fn = paletteNo / numPalettes;

    for(int i = 0; i < 16; i++) {
        palette[i] = paletteFns[fn](palettes[p][i]);
    }
}

void tvic::nextPalette() {
    updatePalette(paletteNo + 1);
}