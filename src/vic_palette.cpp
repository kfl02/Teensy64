/*
	Copyright Frank Bösing, Karsten Fleischer, 2023

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

#include "vic_palette.h"

#define PALETTE(r,g,b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

uint16_t (*palette_fns[num_palette_fns])(uint16_t) = {
    // color display (default)
    [&] (uint16_t c) -> uint16_t {
         return c;
    },
    // B&W TV for real retro feeling. Looks great
    [&] (uint16_t c) -> uint16_t {
        auto r = (c & 0b1111100000000000) >> 8;
        auto g = (c & 0b0000011111100000) >> 3;
        auto b = (c & 0b0000000000011111) << 3;
        
        return ( ( ( ( (int)( 0.299f * r + 0.587f * g + 0.114f * b ) ) & 0xF8) << 8 ) |
            ( ( ( (int)( 0.299f * r + 0.587f * g + 0.114f * b ) ) & 0xFC) << 3 ) |
            ( ( ( (int)( 0.299f * r + 0.587f * g + 0.114f * b ) ) & 0xFF) >> 3 ) );
    },
    // green display
    [&] (uint16_t c) -> uint16_t {
        auto r = (c & 0b1111100000000000) >> 8;
        auto g = (c & 0b0000011111100000) >> 3;
        auto b = (c & 0b0000000000011111) << 3;
        
        return ( ( 0 ) |
            ( ( ( (int)( 0.299f * r + 0.587f * g + 0.114f * b ) ) & 0xFC) << 3 ) |
            ( 0 ) );
    },
    // amber display
    [&] (uint16_t c) -> uint16_t {
        auto r = (c & 0b1111100000000000) >> 8;
        auto g = (c & 0b0000011111100000) >> 3;
        auto b = (c & 0b0000000000011111) << 3;
        
        return ( ( ( ( (int)( 0.299f * r + 0.587f * g + 0.114f * b ) ) & 0xF8) << 8 ) |
            ( ( ( (int)( 0.69f * 0.299f * r + 0.69f * 0.587f * g + 0.69f * 0.114f * b ) ) & 0xFC) << 3 ) |
            ( 0 ) );
    }
};

const uint16_t palettes[num_palettes][16] = {
    {
        PALETTE(0x00, 0x00, 0x00), // black
        PALETTE(0xff, 0xff, 0xff), // white
        PALETTE(0x88, 0x20, 0x00), // red
        PALETTE(0x68, 0xd0, 0xa8), // cyan
        PALETTE(0xa8, 0x38, 0xa0), // purple
        PALETTE(0x50, 0xb8, 0x18), // green
        PALETTE(0x18, 0x10, 0x90), // blue
        PALETTE(0xf0, 0xe8, 0x58), // yellow
        PALETTE(0xa0, 0x48, 0x00), // orange
        PALETTE(0x47, 0x2b, 0x1b), // brown
        PALETTE(0xc8, 0x78, 0x70), // lightred
        PALETTE(0x48, 0x48, 0x48), // grey1
        PALETTE(0x80, 0x80, 0x80), // grey2
        PALETTE(0x98, 0xff, 0x98), // lightgreen
        PALETTE(0x50, 0x90, 0xd0), // lightblue
        PALETTE(0xb8, 0xb8, 0xb8)  // grey3
    },
    {   // vice
        PALETTE(0x00,0x00,0x00), 
        PALETTE(0xFD,0xFE,0xFC), 
        PALETTE(0xBE,0x1a,0x24), 
        PALETTE(0x30,0xe6,0xc6), 
        PALETTE(0xb4,0x1a,0xe2), 
        PALETTE(0x1f,0xd2,0x1e), 
        PALETTE(0x21,0x1b,0xae), 
        PALETTE(0xdf,0xf6,0x0a), 
        PALETTE(0xb8,0x41,0x04), 
        PALETTE(0x6a,0x33,0x04), 
        PALETTE(0xfe,0x4a,0x57), 
        PALETTE(0x42,0x45,0x40), 
        PALETTE(0x70,0x74,0x6f), 
        PALETTE(0x59,0xfe,0x59), 
        PALETTE(0x5f,0x53,0xfe), 
        PALETTE(0xa4,0xa7,0xa2)  
    },
    { // "PEPTO" http://www.pepto.de/projects/colorvic/2001/
		PALETTE(0x00, 0x00, 0x00),
		PALETTE(0xFF, 0xFF, 0xFF),
		PALETTE(0x68, 0x37, 0x2B),
		PALETTE(0x70, 0xA4, 0xB2),
		PALETTE(0x6F, 0x3D, 0x86),
		PALETTE(0x58, 0x8D, 0x43),
		PALETTE(0x35, 0x28, 0x79),
		PALETTE(0xB8, 0xC7, 0x6F),
		PALETTE(0x6F, 0x4F, 0x25),
		PALETTE(0x43, 0x39, 0x00),
		PALETTE(0x9A, 0x67, 0x59),
		PALETTE(0x44, 0x44, 0x44),
		PALETTE(0x6C, 0x6C, 0x6C),
		PALETTE(0x9A, 0xD2, 0x84),
		PALETTE(0x6C, 0x5E, 0xB5),
		PALETTE(0x95, 0x95, 0x95),
    },
    { // "GODOT" http://www.godot64.de/german/hpalet.htm
        PALETTE(0x00,0x00,0x00), 
        PALETTE(0xff,0xff,0xff), 
        PALETTE(0x88,0x00,0x00), 
        PALETTE(0xaa,0xff,0xee), 
        PALETTE(0xcc,0x44,0xcc), 
        PALETTE(0x00,0xcc,0x55), 
        PALETTE(0x00,0x00,0xaa), 
        PALETTE(0xee,0xee,0x77), 
        PALETTE(0xdd,0x88,0x55), 
        PALETTE(0x66,0x44,0x00), 
        PALETTE(0xff,0x77,0x77), 
        PALETTE(0x33,0x33,0x33), 
        PALETTE(0x77,0x77,0x77), 
        PALETTE(0xaa,0xff,0x66), 
        PALETTE(0x00,0x88,0xff), 
        PALETTE(0xbb,0xbb,0xbb) 
    },
    { // "FRODO"
        PALETTE(0x00,0x00,0x00), 
        PALETTE(0xff,0xff,0xff), 
        PALETTE(0xcc,0x00,0x00), 
        PALETTE(0x00,0xff,0xcc), 
        PALETTE(0xff,0x00,0xff), 
        PALETTE(0x00,0xcc,0x00), 
        PALETTE(0x00,0x00,0xcc), 
        PALETTE(0xff,0xff,0x00), 
        PALETTE(0xff,0x88,0x00), 
        PALETTE(0x88,0x44,0x00), 
        PALETTE(0xff,0x88,0x88), 
        PALETTE(0x44,0x44,0x44), 
        PALETTE(0x88,0x88,0x88), 
        PALETTE(0x88,0xff,0x88), 
        PALETTE(0x88,0x88,0xff), 
        PALETTE(0xcc,0xcc,0xcc) 
    },
    { //RGB (full saturated colors - only good for testing)
        PALETTE(0x00,0x00,0x00), 
        PALETTE(0xff,0xff,0xff), 
        PALETTE(0xff,0x00,0x00), 
        PALETTE(0x00,0xff,0xff), 
        PALETTE(0xff,0x00,0xff), 
        PALETTE(0x00,0xff,0x00), 
        PALETTE(0x00,0x00,0xff), 
        PALETTE(0xff,0xff,0x00), 
        PALETTE(0xff,0x80,0x00), 
        PALETTE(0x80,0x40,0x00), 
        PALETTE(0xff,0x80,0x80), 
        PALETTE(0x40,0x40,0x40), 
        PALETTE(0x80,0x80,0x80), 
        PALETTE(0x80,0xff,0x80), 
        PALETTE(0x80,0x80,0xff), 
        PALETTE(0xc0,0xc0,0xc0) 
    }
};