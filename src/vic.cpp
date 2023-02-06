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

#include "Teensy64.h"
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
    return 1 << ((data >> 8) & 0x07);
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
static void fastFillLine(tpixel *p, const tpixel *pe, const uint16_t col, uint16_t *spl) {
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
static void mode0(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc) {
    // Standard-Textmodus(ECM/BMM/MCM=0/0/0)
    /*
      Standard-Textmodus (ECM / BMM / MCM = 0/0/0)
      In diesem Modus (wie in allen Textmodi) liest der VIC aus der videomatrix 8-Bit-Zeichenzeiger,
      die die Adresse der Punktmatrix des Zeichens im Zeichengenerator angibt. Damit ist ein Zeichensatz
      von 256 Zeichen verfügbar, die jeweils aus 8×8 Pixeln bestehen, die in 8 aufeinanderfolgenden Bytes
      im Zeichengenerator abgelegt sind. Mit den Bits VM10-13 und CB11-13 aus Register $d018 lassen sich
      videomatrix und Zeichengenerator im Speicher verschieben. Im Standard-Textmodus entspricht jedes Bit
      im Zeichengenerator direkt einem Pixel auf dem Bildschirm. Die Vordergrundfarbe ist für jedes Zeichen
      im Farbnibble aus der videomatrix angegeben, die Hintergrundfarbe wird global durch Register $d021 festgelegt.

      +----+----+----+----+----+----+----+----+
      |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
      +----+----+----+----+----+----+----+----+
      |         8 Pixel (1 Bit/Pixel)         |
      |                                       |
      | "0": Hintergrundfarbe 0 ($d021)       |
      | "1": Farbe aus Bits 8-11 der c-Daten  |
      +---------------------------------------+

    */

    uint8_t chr, pixel;
    uint16_t fgcol;
    uint16_t bgcol;
    uint16_t x = 0;

    CHARSETPTR();

    if(cpu.vic.lineHasSprites) {
        do {
            BADLINE(x);

            chr = cpu.vic.charsetPtr[cpu.vic.lineMemChr[x] * 8];
            fgcol = cpu.vic.lineMemCol[x];

            x++;

            unsigned m = min(8, pe - p);

            for(unsigned i = 0; i < m; i++) {
                int spriteLineData = *spl++;

                if(spriteLineData) {     // Sprite: Ja
                    uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                    uint8_t spritePixel = spritePixelFromLineData(spriteLineData);
                    bool spritePriority = spritePriorityFromLineData(spriteLineData);

                    if(spritePriority) {   // Sprite: Hinter Text  MDP = 1
                        if(chr & 0x80) {
                            cpu.vic.fgcollision |= spriteBit;
                            pixel = fgcol;
                        } else {
                            pixel = spritePixel;
                        }
                    } else {            // Sprite: Vor Text //MDP = 0
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

            chr = cpu.vic.charsetPtr[cpu.vic.lineMemChr[x] * 8];
            fgcol = cpu.vic.palette[cpu.vic.lineMemCol[x]];
            bgcol = cpu.vic.palette[cpu.vic.R[VIC_B0C]];
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

            chr = cpu.vic.charsetPtr[cpu.vic.lineMemChr[x] * 8];
            fgcol = cpu.vic.palette[cpu.vic.lineMemCol[x]];
            bgcol = cpu.vic.palette[cpu.vic.R[VIC_B0C]];
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
static void mode1(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc) {
    /*
      Multicolor-Textmodus (ECM/BMM/MCM=0/0/1)
      Dieser Modus ermöglicht es, auf Kosten der horizontalen Auflösung vierfarbige Zeichen darzustellen.
      Ist Bit 11 der c-Daten Null, wird das Zeichen wie im Standard-Textmodus dargestellt, wobei aber nur die
      Farben 0-7 für den Vordergrund zur Verfügung stehen. Ist Bit 11 gesetzt, bilden jeweils zwei horizontal
      benachbarte Bits der Punktmatrix ein Pixel. Dadurch ist die Auflösung des Zeichens auf 4×8 reduziert
      (die Pixel sind doppelt so breit, die Gesamtbreite der Zeichen ändert sich also nicht).
      Interessant ist, daß nicht nur die Bitkombination „00”, sondern auch „01” für die Spritepriorität
      und -kollisionserkennung zum "Hintergrund" gezählt wird.

      +----+----+----+----+----+----+----+----+
      |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
      +----+----+----+----+----+----+----+----+
      |         8 Pixel (1 Bit/Pixel)         |
      |                                       | MC-Flag = 0
      | "0": Hintergrundfarbe 0 ($d021)       |
      | "1": Farbe aus Bits 8-10 der c-Daten  |
      +---------------------------------------+
      |         4 Pixel (2 Bit/Pixel)         |
      |                                       |
      | "00": Hintergrundfarbe 0 ($d021)      | MC-Flag = 1
      | "01": Hintergrundfarbe 1 ($d022)      |
      | "10": Hintergrundfarbe 2 ($d023)      |
      | "11": Farbe aus Bits 8-10 der c-Daten |
      +---------------------------------------+

    */

    // POKE 53270,PEEK(53270) OR 16
    // poke 53270,peek(53270) or 16

    uint8_t pixel;
    uint16_t bgcol, fgcol;
    uint16_t colors[4];
    uint8_t chr;
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
                    int spriteLineData = *spl++;

                    if(spriteLineData) {     // Sprite: Ja
                        /*
                          Sprite-Prioritäten (Anzeige)
                          MDP = 1: Grafikhintergrund, Sprite, Vordergrund
                          MDP = 0: Grafikhintergrund, Vordergrund, Sprite

                          Kollision:
                          Eine Kollision zwischen Sprites und anderen Grafikdaten wird erkannt,
                          sobald beim Bildaufbau ein nicht transparentes Spritepixel und ein Vordergrundpixel ausgegeben wird.

                        */
                        uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                        bool spritePriority = spritePriorityFromLineData(spriteLineData);
                        pixel = spritePixelFromLineData(spriteLineData);

                        if(spritePriority) {   // MDP = 1
                            if(chr & 0x80) { //Vordergrundpixel ist gesetzt
                                cpu.vic.fgcollision |= spriteBit;
                                pixel = colors[3];
                            }
                        } else {            // MDP = 0
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

                    int spriteLineData = *spl++;

                    if(spriteLineData) {    // Sprite: Ja
                        uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                        bool spritePriority = spritePriorityFromLineData(spriteLineData);

                        pixel = spritePixelFromLineData(spriteLineData);

                        if(spritePriority) {  // MDP = 1
                            if(chr & 0x80) {  //Vordergrundpixel ist gesetzt
                                cpu.vic.fgcollision |= spriteBit;
                                pixel = colors[c];
                            }
                        } else {          // MDP = 0
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

                        if(spritePriority) {  // MDP = 1

                            if(chr & 0x80) { //Vordergrundpixel ist gesetzt
                                cpu.vic.fgcollision |= spriteBit;
                                pixel = colors[c];
                            }
                        } else {          // MDP = 0
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
static void mode2(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc) {
    /*
       Standard-Bitmap-Modus (ECM / BMM / MCM = 0/1/0) ("HIRES")
       In diesem Modus (wie in allen Bitmap-Modi) liest der VIC die Grafikdaten aus einer 320×200-Bitmap,
       in der jedes Bit direkt einem Punkt auf dem Bildschirm entspricht. Die Daten aus der videomatrix
       werden für die Farbinformation benutzt. Da die videomatrix weiterhin nur eine 40×25-Matrix ist,
       können die Farben nur für Blöcke von 8×8 Pixeln individuell bestimmt werden (also eine Art YC-8:1-Format).
       Da die Entwickler des VIC-II den Bitmap-Modus mit sowenig zusätzlichem Schaltungsaufwand wie möglich realisieren wollten
       (der VIC-I hatte noch keinen Bitmap-Modus), ist die Bitmap etwas ungewöhnlich im Speicher abgelegt:
       Im Gegensatz zu modernen Videochips, die die Bitmap linear aus dem Speicher lesen, bilden beim VIC jeweils 8 aufeinanderfolgende Bytes einen 8×8-Pixelblock
       auf dem Bildschirm. Mit den Bits VM10-13 und CB13 aus Register $d018 lassen sich videomatrix und Bitmap im Speicher verschieben.
       Im Standard-Bitmap-Modus entspricht jedes Bit in der Bitmap direkt einem Pixel auf dem Bildschirm.
       Für jeden 8×8-Block können Vorder- und Hintergrundfarbe beliebig eingestellt werden.

       +----+----+----+----+----+----+----+----+
       |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
       +----+----+----+----+----+----+----+----+
       |         8 Pixel (1 Bit/Pixel)         |
       |                                       |
       | "0": Farbe aus Bits 0-3 der c-Daten   |
       | "1": Farbe aus Bits 4-7 der c-Daten   |
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
                int spriteLineData = *spl++;

                chr = chr << 1;

                if(spriteLineData) {     // Sprite: Ja
                    /*
                       Sprite-Prioritäten (Anzeige)
                       MDP = 1: Grafikhintergrund, Sprite, Vordergrund
                       MDP = 0: Grafikhintergrund, Vordergrund, Sprite

                       Kollision:
                       Eine Kollision zwischen Sprites und anderen Grafikdaten wird erkannt,
                       sobald beim Bildaufbau ein nicht transparentes Spritepixel und ein Vordergrundpixel ausgegeben wird.

                    */
                    uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                    bool spritePriority = spritePriorityFromLineData(spriteLineData);

                    pixel = spritePixelFromLineData(spriteLineData);

                    if(spritePriority) {   // MDP = 1
                        if(chr & 0x80) { //Vordergrundpixel ist gesetzt
                            cpu.vic.fgcollision |= spriteBit;
                            pixel = fgcol;
                        }
                    } else {            // MDP = 0
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
static void mode3(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc) {
    /*
      Multicolor-Bitmap-Modus (ECM/BMM/MCM=0/1/1)

      Ähnlich wie beim Multicolor-Textmodus bilden auch in diesem Modus jeweils
      zwei benachbarte Bits ein (doppelt so breites) Pixel. Die Auflösung
      reduziert sich damit auf 160×200 Pixel.

      Genau wie beim Multicolor-Textmodus wird die Bitkombination "01" für die
      Spritepriorität und -kollisionserkennung zum "Hintergrund" gezählt.

      +----+----+----+----+----+----+----+----+
      |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
      +----+----+----+----+----+----+----+----+
      |         4 Pixel (2 Bit/Pixel)         |
      |                                       |
      | "00": Hintergrundfarbe 0 ($d021)      |
      | "01": Farbe aus Bits 4-7 der c-Daten  |
      | "10": Farbe aus Bits 0-3 der c-Daten  |
      | "11": Farbe aus Bits 8-11 der c-Daten |
      +---------------------------------------+

      POKE 53265,PEEK(53625)OR 32: POKE 53270,PEEK(53270)OR 16
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

                int spriteLineData = *spl++;

                if(spriteLineData) {    // Sprite: Ja
                    uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                    bool spritePriority = spritePriorityFromLineData(spriteLineData);

                    pixel = spritePixelFromLineData(spriteLineData);

                    if(spritePriority) {  // MDP = 1
                        if(c & 0x02) {  //Vordergrundpixel ist gesetzt
                            cpu.vic.fgcollision |= spriteBit;
                            pixel = colors[c];
                        }
                    } else {          // MDP = 0
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

                    if(spritePriority) {  // MDP = 1
                        if(c & 0x02) {  //Vordergrundpixel ist gesetzt
                            cpu.vic.fgcollision |= spriteBit;
                            pixel = colors[c];
                        }
                    } else {          // MDP = 0
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
static void mode4(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc) {
    //ECM-Textmodus (ECM/BMM/MCM=1/0/0)
    /*
      Dieser Textmodus entspricht dem Standard-Textmodus, erlaubt es aber, für
      jedes einzelne Zeichen eine von vier Hintergrundfarben auszuwählen. Die
      Auswahl geschieht über die oberen beiden Bits des Zeichenzeigers. Dadurch
      reduziert sich der Zeichenvorrat allerdings von 256 auf 64 Zeichen.

      +----+----+----+----+----+----+----+----+
      |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
      +----+----+----+----+----+----+----+----+
      |         8 Pixel (1 Bit/Pixel)         |
      |                                       |
      | "0": Je nach Bits 6/7 der c-Daten     |
      |      00: Hintergrundfarbe 0 ($d021)   |
      |      01: Hintergrundfarbe 1 ($d022)   |
      |      10: Hintergrundfarbe 2 ($d023)   |
      |      11: Hintergrundfarbe 3 ($d024)   |
      | "1": Farbe aus Bits 8-11 der c-Daten  |
      +---------------------------------------+
    */
    // https://www.c64-wiki.de/wiki/Hintergrundfarbe
    // POKE 53265, PEEK(53265) OR 64:REM CURSOR BLINKT ROT abc

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
                int spriteLineData = *spl++;

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

static void mode5(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc) {
    /*
      Ungültiger Textmodus (ECM/BMM/MCM=1/0/1)

      Das gleichzeitige Setzen der ECM- und MCM-Bits wählt keinen der
      "offiziellen" Grafikmodi des VIC, sondern erzeugt nur schwarze Pixel.
      Nichtsdestotrotz erzeugt der Grafikdatensequenzer auch in diesem Modus
      intern gültige Grafikdaten, die die Spritekollisionserkennung triggern
      können. Über den Umweg der Spritekollisionen kann man die erzeugten Daten
      auch auslesen (sehen kann man nichts, das Bild ist schwarz). Man kann so
      allerdings nur Vordergrund- und Hintergrundpixel unterscheiden, die
      Farbinformation läßt sich aus den Spritekollisionen nicht gewinnen.

      Die erzeugte Grafik entspricht der des Multicolor-Textmodus, allerdings ist
      der Zeichenvorrat genau wie im ECM-Modus auf 64 Zeichen eingeschränkt.
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

                    int spriteLineData = *spl;
                    *spl++ = 0;

                    if(spriteLineData) {     // Sprite: Ja
                        /*
                          Sprite-Prioritäten (Anzeige)
                          MDP = 1: Grafikhintergrund, Sprite, Vordergrund
                          MDP = 0: Grafikhintergrund, Vordergrund, Sprite

                          Kollision:
                          Eine Kollision zwischen Sprites und anderen Grafikdaten wird erkannt,
                          sobald beim Bildaufbau ein nicht transparentes Spritepixel und ein Vordergrundpixel ausgegeben wird.

                        */
                        uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                        bool spritePriority = spritePriorityFromLineData(spriteLineData);

                        pixel = spritePixelFromLineData(spriteLineData);


                        if(spritePriority) {   // MDP = 1
                            if(chr & 0x80) { //Vordergrundpixel ist gesetzt
                                cpu.vic.fgcollision |= spriteBit;
                                pixel = 0;
                            }
                        } else {            // MDP = 0
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

                    int spriteLineData = *spl;

                    *spl++ = 0;

                    if(spriteLineData) {    // Sprite: Ja
                        uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                        bool spritePriority = spritePriorityFromLineData(spriteLineData);

                        pixel = spritePixelFromLineData(spriteLineData);

                        if(spritePriority) {  // MDP = 1
                            if(chr & 0x80) {  //Vordergrundpixel ist gesetzt
                                cpu.vic.fgcollision |= spriteBit;
                                pixel = 0;
                            }
                        } else {          // MDP = 0
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

                    spriteLineData = *spl;

                    *spl++ = 0;
                    //Das gleiche nochmal für das nächste Pixel
                    if(spriteLineData) {    // Sprite: Ja
                        uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                        bool spritePriority = spritePriorityFromLineData(spriteLineData);

                        pixel = spritePixelFromLineData(spriteLineData);

                        if(spritePriority) {  // MDP = 1
                            if(chr & 0x80) { //Vordergrundpixel ist gesetzt
                                cpu.vic.fgcollision |= spriteBit;
                                pixel = 0;
                            }
                        } else {          // MDP = 0
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
static void mode6(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc) {
    /*
      Ungültiger Bitmap-Modus 1 (ECM/BMM/MCM=1/1/0)

      Dieser Modus erzeugt nur ebenfalls nur ein schwarzes Bild, die Pixel lassen
      sich allerdings auch hier mit dem Spritekollisionstrick auslesen.

      Der Aufbau der Grafik ist im Prinzip wie im Standard-Bitmap-Modus, aber die
      Bits 9 und 10 der g-Adressen sind wegen dem gesetzten ECM-Bit immer Null,
      entsprechend besteht auch die Grafik - grob gesagt - aus vier
      "Abschnitten", die jeweils viermal wiederholt dargestellt werden.

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
                int spriteLineData = *spl;
                *spl++ = 0;

                chr = chr << 1;

                if(spriteLineData) {     // Sprite: Ja
                    /*
                       Sprite-Prioritäten (Anzeige)
                       MDP = 1: Grafikhintergrund, Sprite, Vordergrund
                       MDP = 0: Grafikhintergung, Vordergrund, Sprite

                       Kollision:
                       Eine Kollision zwischen Sprites und anderen Grafikdaten wird erkannt,
                       sobald beim Bildaufbau ein nicht transparentes Spritepixel und ein Vordergrundpixel ausgegeben wird.

                    */
                    uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                    bool spritePriority = spritePriorityFromLineData(spriteLineData);

                    pixel = spritePixelFromLineData(spriteLineData);

                    if(spritePriority) {   // MDP = 1
                        if(chr & 0x80) { //Vordergrundpixel ist gesetzt
                            cpu.vic.fgcollision |= spriteBit;
                            pixel = 0;
                        }
                    } else {            // MDP = 0
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
static void mode7(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc) {
    /*
      Ungültiger Bitmap-Modus 2 (ECM/BMM/MCM=1/1/1)

      Der letzte ungültige Modus liefert auch ein schwarzes Bild, das sich jedoch
      genauso mit Hilfe der Sprite-Grafik-Kollisionen "abtasten" läßt.

      Der Aufbau der Grafik ist im Prinzip wie im Multicolor-Bitmap-Modus, aber
      die Bits 9 und 10 der g-Adressen sind wegen dem gesetzten ECM-Bit immer
      Null, was sich in der Darstellung genauso wie beim ersten ungültigen
      Bitmap-Modus wiederspiegelt. Die Bitkombination "01" wird wie gewohnt zum
      Hintergrund gezählt.

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

                int spriteLineData = *spl;

                *spl++ = 0;

                if(spriteLineData) {    // Sprite: Ja
                    uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                    bool spritePriority = spritePriorityFromLineData(spriteLineData);

                    pixel = spritePixelFromLineData(spriteLineData);

                    if(spritePriority) {  // MDP = 1
                        if(chr & 0x80) {  //Vordergrundpixel ist gesetzt
                            cpu.vic.fgcollision |= spriteBit;
                            pixel = 0;
                        }
                    } else {          // MDP = 0
                        if(chr & 0x80) {
                            cpu.vic.fgcollision |= spriteBit;
                        } //Vordergundpixel ist gesetzt
                    }
                } else { // Kein Sprite
                    pixel = 0;
                }

                *p++ = cpu.vic.palette[pixel];

                if(p >= pe) { break; }

                spriteLineData = *spl;
                *spl++ = 0;

                if(spriteLineData) {    // Sprite: Ja
                    uint8_t spriteBit = spriteBitFromLineData(spriteLineData);
                    bool spritePriority = spritePriorityFromLineData(spriteLineData);

                    pixel = spritePixelFromLineData(spriteLineData);

                    if(spritePriority) {  // MDP = 1
                        if(chr & 0x80) {  //Vordergrundpixel ist gesetzt
                            cpu.vic.fgcollision |= spriteBit;
                            pixel = 0;
                        }
                    } else {          // MDP = 0
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

typedef void (*modes_t)(tpixel *p, const tpixel *pe, uint16_t *spl, const uint16_t vc); //Funktionspointer

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
    /*
      ?PEEK(678) NTSC =0
      ?PEEK(678) PAL = 1
    */

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
      Ein Bad-Line-Zustand liegt in einem beliebigen Taktzyklus vor, wenn an der
      negativen Flanke von ø0 zu Beginn des Zyklus RASTER >= $30 und RASTER <=
      $f7 und die unteren drei Bits von RASTER mit YSCROLL übereinstimmen und in
      einem beliebigen Zyklus von Rasterzeile $30 das DEN-Bit gesetzt war.

      (default 3)
      yscroll : POKE 53265, PEEK(53265) AND 248 OR 1:POKE 1024,0
      yscroll : poke 53265, peek(53265) and 248 or 1

      DEN : POKE 53265, PEEK(53265) AND 224 Bildschirm aus

      Die einzige Verwendung von YSCROLL ist der Vergleich mit rasterLine in der Badline

    */

    if(rasterLine == 0x30) { cpu.vic.denLatch |= cpu.vic.r.DEN; }

    /* 3.7.2
      2. In der ersten Phase von Zyklus 14 jeder Zeile wird VC mit VCBASE geladen
       (VCBASE->VC) und VMLI gelöscht. Wenn zu diesem Zeitpunkt ein
       Bad-Line-Zustand vorliegt, wird zusätzlich RC auf Null gesetzt.
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

    //cpu.vic.videomatrix =  cpu.vic.bank + (unsigned)(cpu.vic.R[VIC_VM_CB] & VIC_VM_CB_VM_MASK) * 64;

    /* Rand oben /unten **********************************************************************************/
    /*
      RSEL  Höhe des Anzeigefensters  Erste Zeile   Letzte Zeile
      0 24 Textzeilen/192 Pixel 55 ($37)  246 ($f6) = 192 sichtbare Zeilen, der Rest ist Rand oder unsichtbar
      1 25 Textzeilen/200 Pixel 51 ($33)  250 ($fa) = 200 sichtbare Zeilen, der Rest ist Rand oder unsichtbar
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


    //max_x =  (!cpu.vic.CSEL) ? 40:38;
    //X-Scrolling:

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
  3.7.3.9. Idle-Zustand
  ---------------------

  Im Idle-Zustand liest der VIC die Grafikdaten von Adresse $3fff (bzw. $39ff
  bei gesetztem ECM-Bit) und stellt sie im ausgewählten Grafikmodus dar,
  wobei aber die Videomatrix-Daten (normalerweise in den c-Zugriffen gelesen)
  nur aus "0"-Bits bestehen. Es wird also immer wiederholt das Byte an
  Adresse $3fff/$39ff ausgegeben.

  c-Zugriff

   Es werden keine c-Zugriffe ausgeführt.

   Daten

   +----+----+----+----+----+----+----+----+----+----+----+----+
   | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
   +----+----+----+----+----+----+----+----+----+----+----+----+
   |  0 |  0 |  0 |  0 |  0 |  0 |  0 |  0 |  0 |  0 |  0 |  0 |
   +----+----+----+----+----+----+----+----+----+----+----+----+

  g-Zugriff

   Adressen (ECM=0)

   +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
   | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
   +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
   |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |
   +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

   Adressen (ECM=1)

   +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
   | 13 | 12 | 11 | 10 |  9 |  8 |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
   +----+----+----+----+----+----+----+----+----+----+----+----+----+----+
   |  1 |  1 |  1 |  0 |  0 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |  1 |
   +----+----+----+----+----+----+----+----+----+----+----+----+----+----+

   Daten

   +----+----+----+----+----+----+----+----+
   |  7 |  6 |  5 |  4 |  3 |  2 |  1 |  0 |
   +----+----+----+----+----+----+----+----+
   |         8 Pixel (1 Bit/Pixel)         | Standard-Textmodus/
   |                                       | Multicolor-Textmodus/
   | "0": Hintergrundfarbe 0 ($d021)       | ECM-Textmodus
   | "1": Schwarz                          |
   +---------------------------------------+
   |         8 Pixel (1 Bit/Pixel)         | Standard-Bitmap-Modus/
   |                                       | Ungültiger Textmodus/
   | "0": Schwarz (Hintergrund)            | Ungültiger Bitmap-Modus 1
   | "1": Schwarz (Vordergrund)            |
   +---------------------------------------+
   |         4 Pixel (2 Bit/Pixel)         | Multicolor-Bitmap-Modus
   |                                       |
   | "00": Hintergrundfarbe 0 ($d021)      |
   | "01": Schwarz (Hintergrund)           |
   | "10": Schwarz (Vordergrund)           |
   | "11": Schwarz (Vordergrund)           |
   +---------------------------------------+
   |         4 Pixel (2 Bit/Pixel)         | Ungültiger Bitmap-Modus 2
   |                                       |
   | "00": Schwarz (Hintergrund)           |
   | "01": Schwarz (Hintergrund)           |
   | "10": Schwarz (Vordergrund)           |
   | "11": Schwarz (Vordergrund)           |
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
      Bei den MBC- und MMC-Interrupts löst jeweils nur die erste Kollision einen
      Interrupt aus (d.h. wenn die Kollisionsregister $d01e bzw. $d01f vor der
      Kollision den Inhalt Null hatten). Um nach einer Kollision weitere
      Interrupts auszulösen, muß das betreffende Register erst durch Auslesen
      gelöscht werden.
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
      5. In der ersten Phase von Zyklus 58 wird geprüft, ob RC=7 ist. Wenn ja,
       geht die Videologik in den Idle-Zustand und VCBASE wird mit VC geladen
       (VC->VCBASE). Ist die Videologik danach im Display-Zustand (liegt ein
       Bad-Line-Zustand vor, ist dies immer der Fall), wird RC erhöht.
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
                        /* Im Mehrfarbenmodus (Multicolor-Modus) bekommen alle Sprites zwei zusätzliche gemeinsame Farben.
                          Die horizontale Auflösung wird von 24 auf 12 halbiert, da bei der Sprite-Definition jeweils zwei Bits zusammengefasst werden.
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
      RSEL  Höhe des Anzeigefensters  Erste Zeile   Letzte Zeile
      0 24 Textzeilen/192 Pixel 55 ($37)  246 ($f6) = 192 sichtbare Zeilen, der Rest ist Rand oder unsichtbar
      1 25 Textzeilen/200 Pixel 51 ($33)  250 ($fa) = 200 sichtbare Zeilen, der Rest ist Rand oder unsichtbar
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

void tvic::adrchange() {
    uint8_t r18 = cpu.vic.R[VIC_VM_CB];
    cpu.vic.videomatrix = cpu.vic.bank + (unsigned) (r18 & VIC_VM_CB_VM_MASK) * 64;

    unsigned charsetAddr = r18 & VIC_VM_CB_CB_MASK;
    if((cpu.vic.bank & 0x4000) == 0) {
        if(charsetAddr == 0x04) { cpu.vic.charsetPtrBase = ((uint8_t *) &rom_characters); }
        else if(charsetAddr == 0x06) { cpu.vic.charsetPtrBase = ((uint8_t *) &rom_characters) + 0x800; }
        else {
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
            adrchange();

            break;

        case VIC_RASTER:
            cpu.vic.intRasterLine = (cpu.vic.intRasterLine & 0x100) | value;
            cpu.vic.R[VIC_RASTER] = value;
            break;

        case VIC_VM_CB:
            cpu.vic.R[VIC_VM_CB] = value;
            adrchange();
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

    adrchange();
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