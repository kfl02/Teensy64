/*
    Copyright Mike Chambers, Frank Bösing, 2017
    Parts of this file are based on "Fake6502 CPU emulator core v1.1" by Mike Chambers


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

/* Fake6502 CPU emulator core v1.1 *******************
 * (c)2011 Mike Chambers (miker00lz@gmail.com)       *
 *****************************************************
 * LICENSE: This source code is released into the    *
 * public domain, but if you use it please do give   *
 * credit. I put a lot of effort into writing this!  *
 *                                                   *
 *****************************************************
*/
#include <cstdint>
#include "teensy64.h"
#include "cpu.h"
#include "cia6526.h"

#define FLAG_CARRY     0x01
#define FLAG_ZERO      0x02
#define FLAG_INTERRUPT 0x04
#define FLAG_DECIMAL   0x08
#define FLAG_BREAK     0x10
#define FLAG_CONSTANT  0x20
#define FLAG_OVERFLOW  0x40
#define FLAG_SIGN      0x80

//flag modifier macros
#define setcarry()          cpu.cpustatus |= FLAG_CARRY
#define clearcarry()        cpu.cpustatus &= (~FLAG_CARRY)
#define setzero()           cpu.cpustatus |= FLAG_ZERO
#define clearzero()         cpu.cpustatus &= (~FLAG_ZERO)
#define setinterrupt()      cpu.cpustatus |= FLAG_INTERRUPT
#define clearinterrupt()    cpu.cpustatus &= (~FLAG_INTERRUPT)
#define setdecimal()        cpu.cpustatus |= FLAG_DECIMAL
#define cleardecimal()      cpu.cpustatus &= (~FLAG_DECIMAL)
#define setoverflow()       cpu.cpustatus |= FLAG_OVERFLOW
#define clearoverflow()     cpu.cpustatus &= (~FLAG_OVERFLOW)
#define setsign()           cpu.cpustatus |= FLAG_SIGN
#define clearsign()         cpu.cpustatus &= (~FLAG_SIGN)


//flag calculation macros
#define zerocalc(n)             { if ((n) & 0x00FF) clearzero(); else setzero(); }
//#define signcalc(n)           { if ((n) & 0x0080) setsign(); else clearsign(); }
#define signcalc(n)             { cpu.cpustatus =( cpu.cpustatus & 0x7f) | (n & 0x80); }
#define carrycalc(n)            { if ((n) & 0xFF00) setcarry(); else clearcarry(); }
//#define carrycalc(n)          { cpu.cpustatus =( cpu.cpustatus & 0xfe) | (n >> 8); }
#define overflowcalc(n, m, o)   { if (((n) ^ (uint16_t)(m)) & ((n) ^ (o)) & 0x0080) setoverflow(); else clearoverflow(); }

#define saveaccum(n)    cpu.a = (uint8_t)((n) & 0x00FF)

#define UNSUPPORTED { Serial.println("Unsupported static"); while(1){;} }

void logAddr(const uint32_t address, const uint8_t value, const uint8_t rw) {
    if(rw) { Serial.print("Write "); } else { Serial.print("Read "); }
    Serial.print("0x");
    Serial.print(address, HEX);
    Serial.print("=0x");
    Serial.println(value, HEX);
}

struct tcpu cpu;
struct tio io;

void reset6502();

static inline void cpu_irq();

static inline __attribute__((always_inline, flatten)) uint8_t read6502(uint32_t address) __attribute__ ((hot));

static inline __attribute__((always_inline, flatten)) uint8_t read6502(const uint32_t address) {
    return (*cpu.plamap_r)[address >> 8](address);
}

static inline __attribute__((always_inline, flatten)) uint8_t read6502ZP(uint32_t address) __attribute__ ((hot)); //Zeropage
static inline __attribute__((always_inline, flatten)) uint8_t read6502ZP(const uint32_t address) {
    return cpu.RAM[address & 0xff];
}

/* Ein Schreibzugriff auf einen ROM-Bereich speichert das Byte im „darunterliegenden” RAM. */
static inline __attribute__((always_inline, flatten)) void write6502(uint32_t address, uint8_t value) __attribute__ ((hot));

static inline __attribute__((always_inline, flatten)) void write6502(const uint32_t address, const uint8_t value) {
    (*cpu.plamap_w)[address >> 8](address, value);
}

//a few general functions used by various other functions
static inline __attribute__((always_inline, flatten)) void push16(const uint16_t pushval) {
    cpu.RAM[BASE_STACK + cpu.sp] = (pushval >> 8) & 0xFF;
    cpu.RAM[BASE_STACK + ((cpu.sp - 1) & 0xFF)] = pushval & 0xFF;
    cpu.sp -= 2;
}

static inline __attribute__((always_inline, flatten)) uint16_t pull16() {
    uint16_t temp16;

    temp16 = cpu.RAM[BASE_STACK + ((cpu.sp + 1) & 0xFF)] |
             ((uint16_t) cpu.RAM[BASE_STACK + ((cpu.sp + 2) & 0xFF)] << 8);
    cpu.sp += 2;

    return (temp16);
}

static inline __attribute__((always_inline, flatten)) void push8(uint8_t pushval) {
    cpu.RAM[BASE_STACK + (cpu.sp--)] = pushval;
}

static inline __attribute__((always_inline, flatten)) uint8_t pull8() {
    return cpu.RAM[BASE_STACK + (++cpu.sp)];
}

/********************************************************************************************************************/
/*addressing mode functions, calculates effective addresses                                                         */
/********************************************************************************************************************/

static inline __attribute__((always_inline, flatten)) void imp() { //implied
}

static inline __attribute__((always_inline, flatten)) void acc() { //accumulator
}

static inline __attribute__((always_inline, flatten)) void imm() { //immediate
    cpu.ea = cpu.pc++;
}

static inline __attribute__((always_inline, flatten)) void zp() { //zero-page
    cpu.ea = read6502(cpu.pc++) & 0xFF;
}

static inline __attribute__((always_inline, flatten)) void zpx() { //zero-page,X
    cpu.ea = (read6502(cpu.pc++) + cpu.x) & 0xFF; //zero-page wraparound
}

static inline __attribute__((always_inline, flatten)) void zpy() { //zero-page,Y
    cpu.ea = (read6502(cpu.pc++) + cpu.y) & 0xFF; //zero-page wraparound
}

static inline __attribute__((always_inline, flatten)) void rel() { //relative for branch ops (8-bit immediate value, sign-extended)
    cpu.reladdr = read6502(cpu.pc++);
    if(cpu.reladdr & 0x80) { cpu.reladdr |= 0xFF00; }
}

static inline __attribute__((always_inline, flatten)) void abso() { //absolute
    cpu.ea = read6502(cpu.pc) | (read6502(cpu.pc + 1) << 8);
    cpu.pc += 2;
}

static inline __attribute__((always_inline, flatten)) void absx() { //absolute,X
    cpu.ea = (read6502(cpu.pc) | (read6502(cpu.pc + 1) << 8)) + cpu.x;
    cpu.pc += 2;
}

static inline __attribute__((always_inline, flatten)) void absx_t() { //absolute,X with extra cycle
    uint16_t h = read6502(cpu.pc) + cpu.x;

    if(h & 0x100) { cpu.ticks += 1; }

    cpu.ea = h + (read6502(cpu.pc + 1) << 8);
    cpu.pc += 2;
}

static inline __attribute__((always_inline, flatten)) void absy() { //absolute,Y
    cpu.ea = (read6502(cpu.pc) + (read6502(cpu.pc + 1) << 8)) + cpu.y;
    cpu.pc += 2;
}

static inline __attribute__((always_inline, flatten)) void absy_t() { //absolute,Y with extra cycle
    uint16_t h = read6502(cpu.pc) + cpu.y;

    if(h & 0x100) { cpu.ticks += 1; }

    cpu.ea = h + (read6502(cpu.pc + 1) << 8);
    cpu.pc += 2;
}

static inline __attribute__((always_inline, flatten)) void ind() { //indirect
    uint16_t eahelp;
    uint16_t eahelp2;

    eahelp = read6502(cpu.pc) | (read6502(cpu.pc + 1) << 8);
    eahelp2 = (eahelp & 0xFF00) | ((eahelp + 1) & 0x00FF); //replicate 6502 page-boundary wraparound bug
    cpu.ea = read6502(eahelp) | (read6502(eahelp2) << 8);
    cpu.pc += 2;
}

static inline __attribute__((always_inline, flatten)) void indx() { // (indirect,X)
    uint32_t eahelp;

    eahelp = (read6502(cpu.pc++) + cpu.x) & 0xFF; //zero-page wraparound for table pointer
    cpu.ea = read6502ZP((uint8_t) eahelp) | (read6502ZP((uint8_t)(eahelp + 1)) << 8);
}

static inline __attribute__((always_inline, flatten)) void indy() { // (zeropage indirect),Y
    uint8_t zp = read6502(cpu.pc++);

    cpu.ea = read6502ZP((uint16_t) zp++);
    cpu.ea += (uint16_t) read6502ZP((uint16_t) zp) << 8;
    cpu.ea += cpu.y;
}

static inline __attribute__((always_inline, flatten)) void indy_t() { // (zeropage indirect),Y with extra cycle
    uint8_t zp = read6502(cpu.pc++);
    uint16_t h;

    h = read6502ZP((uint16_t) zp++);
    h += (uint16_t) read6502ZP((uint16_t) zp) << 8;

    if(((h + cpu.y) & 0xff) != (h & 0xff)) { cpu.ticks += 1; }

    cpu.ea = h + cpu.y;
}

//static inline __attribute__((always_inline, flatten, hot)) uint32_t getvalue() __attribute__ ((hot));

static inline __attribute__((always_inline, flatten, hot)) uint32_t getvalue() {
    return read6502(cpu.ea);
}

static inline __attribute__((always_inline, flatten)) uint32_t getvalueZP() __attribute__ ((hot));

static inline __attribute__((always_inline, flatten)) uint32_t getvalueZP() {
    return read6502ZP(cpu.ea);
}

static inline __attribute__((always_inline, flatten)) void putvalue(uint8_t saveval)  __attribute__ ((hot));

static inline __attribute__((always_inline, flatten)) void putvalue(const uint8_t saveval) {
    write6502(cpu.ea, saveval);
}


/********************************************************************************************************************/
/* instruction handler functions                                          											*/
/********************************************************************************************************************/

/*
Aliases used in other illegal static sources:

SLO = ASO
SRE = LSE
ISC = ISB
ALR = ASR
SHX = A11 (A11 was a result of only having tested this one on adress $1000)
SHY = A11
LAS = LAR
KIL = JAM, HLT
*/

#define SETFLAGS(data)                                              \
{                                                                   \
    if (!(data)) {                                                  \
        cpu.cpustatus = (cpu.cpustatus & ~FLAG_SIGN) | FLAG_ZERO;   \
    } else {                                                        \
        cpu.cpustatus = (cpu.cpustatus & ~(FLAG_SIGN|FLAG_ZERO)) |  \
                        ((data) & FLAG_SIGN);                       \
    }                                                               \
}


static inline __attribute__((always_inline, flatten)) void _adc(unsigned data) {
    unsigned tempval = data;
    unsigned temp;

    if(cpu.cpustatus & FLAG_DECIMAL) {
        temp = (cpu.a & 0x0f) + (tempval & 0x0f) + (cpu.cpustatus & FLAG_CARRY);

        if(temp > 9) { temp += 6; }
        if(temp <= 0x0f) {
            temp = (temp & 0xf) + (cpu.a & 0xf0) + (tempval & 0xf0);
        } else {
            temp = (temp & 0xf) + (cpu.a & 0xf0) + (tempval & 0xf0) + 0x10;
        }
        if(!((cpu.a + tempval + (cpu.cpustatus & FLAG_CARRY)) & 0xff)) {
            setzero();
        } else {
            clearzero();
        }

        signcalc(temp);

        if(((cpu.a ^ temp) & 0x80) && !((cpu.a ^ tempval) & 0x80)) {
            setoverflow();
        } else {
            clearoverflow();
        }
        if((temp & 0x1f0) > 0x90) { temp += 0x60; }
        if((temp & 0xff0) > 0xf0)
            setcarry();
        else
            clearcarry();
    } else {
        temp = tempval + cpu.a + (cpu.cpustatus & FLAG_CARRY);

        SETFLAGS(temp & 0xff);

        if(!((cpu.a ^ tempval) & 0x80) && ((cpu.a ^ temp) & 0x80)) {
            setoverflow();
        } else {
            clearoverflow();
        }
        if(temp > 0xff) {
            setcarry();
        } else {
            clearcarry();
        }
    }

    saveaccum(temp);
}

static inline __attribute__((always_inline, flatten)) void _sbc(unsigned data) {
    unsigned tempval = data;
    unsigned temp;

    temp = cpu.a - tempval - ((cpu.cpustatus & FLAG_CARRY) ^ FLAG_CARRY);

    if(cpu.cpustatus & FLAG_DECIMAL) {
        unsigned tempval2;

        tempval2 = (cpu.a & 0x0f) - (tempval & 0x0f) - ((cpu.cpustatus & FLAG_CARRY) ^ FLAG_CARRY);

        if(tempval2 & 0x10) {
            tempval2 = ((tempval2 - 6) & 0xf) | ((cpu.a & 0xf0) - (tempval & 0xf0) - 0x10);
        } else {
            tempval2 = (tempval2 & 0xf) | ((cpu.a & 0xf0) - (tempval & 0xf0));
        }
        if(tempval2 & 0x100) {
            tempval2 -= 0x60;
        }
        if(temp < 0x100) {
            setcarry();
        } else {
            clearcarry();
        }

        SETFLAGS(temp & 0xff);

        if(((cpu.a ^ temp) & 0x80) && ((cpu.a ^ tempval) & 0x80)) {
            setoverflow();
        }
        else {
            clearoverflow();
        }

        saveaccum(tempval2);
    } else {
        SETFLAGS(temp & 0xff);

        if(temp < 0x100) {
            setcarry();
        } else {
            clearcarry();
        }

        if(((cpu.a ^ temp) & 0x80) && ((cpu.a ^ tempval) & 0x80)) {
            setoverflow();
        } else {
            clearoverflow();
        }

        saveaccum(temp);
    }
}

static inline __attribute__((always_inline, flatten)) void adc() {
    unsigned data = getvalue();

    _adc(data);
}

static inline __attribute__((always_inline, flatten)) void adcZP() {
    unsigned data = getvalueZP();

    _adc(data);
}

static inline __attribute__((always_inline, flatten)) void op_and() {
    uint32_t result = cpu.a & getvalue();

    zerocalc(result);
    signcalc(result);
    saveaccum(result);
}

static inline __attribute__((always_inline, flatten)) void op_andZP() {
    uint32_t result = cpu.a & getvalueZP();

    zerocalc(result);
    signcalc(result);
    saveaccum(result);
}

static inline __attribute__((always_inline, flatten)) void asl() {
    uint32_t result = getvalue();

    result <<= 1;

    carrycalc(result);
    zerocalc(result);
    signcalc(result);
    putvalue(result);
}

static inline __attribute__((always_inline, flatten)) void aslZP() {
    uint32_t result = getvalueZP();

    result <<= 1;

    carrycalc(result);
    zerocalc(result);
    signcalc(result);
    putvalue(result);
}

static inline __attribute__((always_inline, flatten)) void asla() {
    uint32_t result = cpu.a << 1;

    carrycalc(result);
    zerocalc(result);
    signcalc(result);
    saveaccum(result);
}

static inline __attribute__((always_inline, flatten)) void bcc() {
    if((cpu.cpustatus & FLAG_CARRY) == 0) {
        uint32_t oldpc = cpu.pc;

        cpu.pc += cpu.reladdr;

        if((oldpc & 0xFF00) != (cpu.pc & 0xFF00)) {
            cpu.ticks += 2; //check if jump crossed a page boundary
        } else {
            cpu.ticks++;
        }
    }
}

static inline __attribute__((always_inline, flatten)) void bcs() {
    if((cpu.cpustatus & FLAG_CARRY) == FLAG_CARRY) {
        uint32_t oldpc = cpu.pc;

        cpu.pc += cpu.reladdr;

        if((oldpc & 0xFF00) != (cpu.pc & 0xFF00)) {
            cpu.ticks += 2; //check if jump crossed a page boundary
        } else {
            cpu.ticks++;
        }
    }
}

static inline __attribute__((always_inline, flatten)) void beq() {
    if((cpu.cpustatus & FLAG_ZERO) == FLAG_ZERO) {
        uint32_t oldpc = cpu.pc;

        cpu.pc += cpu.reladdr;

        if((oldpc & 0xFF00) != (cpu.pc & 0xFF00)) {
            cpu.ticks += 2; //check if jump crossed a page boundary
        } else {
            cpu.ticks++;
        }
    }
}

static inline __attribute__((always_inline, flatten)) void op_bit() {
    unsigned value = getvalue();

    cpu.cpustatus = (cpu.cpustatus & ~(FLAG_SIGN | FLAG_OVERFLOW)) | (value & (FLAG_SIGN | FLAG_OVERFLOW));

    if(!(value & cpu.a)) {
        setzero();
    } else {
        clearzero();
    }
}

static inline __attribute__((always_inline, flatten)) void op_bitZP() {
    unsigned value = getvalueZP();

    cpu.cpustatus = (cpu.cpustatus & ~(FLAG_SIGN | FLAG_OVERFLOW)) | (value & (FLAG_SIGN | FLAG_OVERFLOW));

    if(!(value & cpu.a)) {
        setzero();
    } else {
        clearzero();
    }
}

static inline __attribute__((always_inline, flatten)) void bmi() {
    if((cpu.cpustatus & FLAG_SIGN) == FLAG_SIGN) {
        uint32_t oldpc = cpu.pc;

        cpu.pc += cpu.reladdr;

        if((oldpc & 0xFF00) != (cpu.pc & 0xFF00)) {
            cpu.ticks += 2; //check if jump crossed a page boundary
        } else {
            cpu.ticks++;
        }
    }
}

static inline __attribute__((always_inline, flatten)) void bne() {
    if((cpu.cpustatus & FLAG_ZERO) == 0) {
        uint32_t oldpc = cpu.pc;

        cpu.pc += cpu.reladdr;

        if((oldpc & 0xFF00) != (cpu.pc & 0xFF00)) {
            cpu.ticks += 2; //check if jump crossed a page boundary
        } else {
            cpu.ticks++;
        }
    }
}

static inline __attribute__((always_inline, flatten)) void bpl() {
    if((cpu.cpustatus & FLAG_SIGN) == 0) {
        uint32_t oldpc = cpu.pc;

        cpu.pc += cpu.reladdr;

        if((oldpc & 0xFF00) != (cpu.pc & 0xFF00)) {
            cpu.ticks += 2; //check if jump crossed a page boundary
        } else {
            cpu.ticks++;
        }
    }
}

static inline __attribute__((always_inline, flatten)) void brk() {
    cpu.pc++;

    push16(cpu.pc); //push next instruction address onto stack
    push8(cpu.cpustatus | FLAG_BREAK); //push CPU cpustatus to stack

    setinterrupt(); //set interrupt flag

    cpu.pc = read6502(0xFFFE) | (read6502(0xFFFF) << 8);
}

static inline __attribute__((always_inline, flatten)) void bvc() {
    if((cpu.cpustatus & FLAG_OVERFLOW) == 0) {
        uint32_t oldpc = cpu.pc;

        cpu.pc += cpu.reladdr;

        if((oldpc & 0xFF00) != (cpu.pc & 0xFF00)) {
            cpu.ticks += 2; //check if jump crossed a page boundary
        } else {
            cpu.ticks++;
        }
    }
}

static inline __attribute__((always_inline, flatten)) void bvs() {
    if((cpu.cpustatus & FLAG_OVERFLOW) == FLAG_OVERFLOW) {
        uint32_t oldpc = cpu.pc;

        cpu.pc += cpu.reladdr;

        if((oldpc & 0xFF00) != (cpu.pc & 0xFF00)) {
            cpu.ticks += 2; //check if jump crossed a page boundary
        } else {
            cpu.ticks++;
        }
    }
}

static inline __attribute__((always_inline, flatten)) void clc() {
    clearcarry();
}

static inline __attribute__((always_inline, flatten)) void cld() {
    cleardecimal();
}

static inline __attribute__((always_inline, flatten)) void cli_() {
    clearinterrupt();
}

static inline __attribute__((always_inline, flatten)) void clv() {
    clearoverflow();
}

static inline __attribute__((always_inline, flatten)) void cmp() {
    uint16_t value = getvalue();
    uint32_t result = (uint16_t) cpu.a - value;

    if(cpu.a >= (uint8_t)(value & 0x00FF)) {
        setcarry();
    } else {
        clearcarry();
    }
    if(cpu.a == (uint8_t)(value & 0x00FF)) {
        setzero();
    } else {
        clearzero();
    }

    signcalc(result);
}

static inline __attribute__((always_inline, flatten)) void cmpZP() {
    uint16_t value = getvalueZP();
    uint32_t result = (uint16_t) cpu.a - value;

    if(cpu.a >= (uint8_t)(value & 0x00FF)) {
        setcarry();
    } else {
        clearcarry();
    }
    if(cpu.a == (uint8_t)(value & 0x00FF)) {
        setzero();
    } else {
        clearzero();
    }
    signcalc(result);
}

static inline __attribute__((always_inline, flatten)) void cpx() {
    uint16_t value = getvalue();
    uint16_t result = (uint16_t) cpu.x - value;

    if(cpu.x >= (uint8_t)(value & 0x00FF)) {
        setcarry();
    } else {
        clearcarry();
    }
    if(cpu.x == (uint8_t)(value & 0x00FF)) {
        setzero();
    } else {
        clearzero();
    }

    signcalc(result);
}

static inline __attribute__((always_inline, flatten)) void cpxZP() {
    uint16_t value = getvalueZP();
    uint16_t result = (uint16_t) cpu.x - value;

    if(cpu.x >= (uint8_t)(value & 0x00FF)) {
        setcarry();
    } else {
        clearcarry();
    }
    if(cpu.x == (uint8_t)(value & 0x00FF)) {
        setzero();
    } else {
        clearzero();
    }

    signcalc(result);
}

static inline __attribute__((always_inline, flatten)) void cpy() {
    uint16_t value = getvalue();
    uint16_t result = (uint16_t) cpu.y - value;

    if(cpu.y >= (uint8_t)(value & 0x00FF)) {
        setcarry();
    } else {
        clearcarry();
    }
    if(cpu.y == (uint8_t)(value & 0x00FF)) {
        setzero();
    } else {
        clearzero();
    }

    signcalc(result);
}

static inline __attribute__((always_inline, flatten)) void cpyZP() {
    uint16_t value = getvalueZP();
    uint16_t result = (uint16_t) cpu.y - value;

    if(cpu.y >= (uint8_t)(value & 0x00FF)) {
        setcarry();
    } else {
        clearcarry();
    }
    if(cpu.y == (uint8_t)(value & 0x00FF)) {
        setzero();
    } else {
        clearzero();
    }

    signcalc(result);
}

static inline __attribute__((always_inline, flatten)) void dec() {
    uint32_t result = getvalue() - 1;

    zerocalc(result);
    signcalc(result);

    putvalue(result);
}

static inline __attribute__((always_inline, flatten)) void decZP() {
    uint32_t result = getvalueZP() - 1;

    zerocalc(result);
    signcalc(result);

    putvalue(result);
}

static inline __attribute__((always_inline, flatten)) void dex() {
    cpu.x--;

    zerocalc(cpu.x);
    signcalc(cpu.x);
}

static inline __attribute__((always_inline, flatten)) void dey() {
    cpu.y--;

    zerocalc(cpu.y);
    signcalc(cpu.y);
}

static inline __attribute__((always_inline, flatten)) void eor() {
    uint32_t result = cpu.a ^ getvalue();

    zerocalc(result);
    signcalc(result);

    saveaccum(result);
}

static inline __attribute__((always_inline, flatten)) void eorZP() {
    uint32_t result = cpu.a ^ getvalueZP();

    zerocalc(result);
    signcalc(result);

    saveaccum(result);
}

static inline __attribute__((always_inline, flatten)) void inc() {
    uint32_t result = getvalue() + 1;

    zerocalc(result);
    signcalc(result);

    putvalue(result);
}

static inline __attribute__((always_inline, flatten)) void incZP() {
    uint32_t result = getvalueZP() + 1;

    zerocalc(result);
    signcalc(result);

    putvalue(result);
}

static inline __attribute__((always_inline, flatten)) void inx() {
    cpu.x++;

    zerocalc(cpu.x);
    signcalc(cpu.x);
}

static inline __attribute__((always_inline, flatten)) void iny() {
    cpu.y++;

    zerocalc(cpu.y);
    signcalc(cpu.y);
}

static inline __attribute__((always_inline, flatten)) void jmp() {
    cpu.pc = cpu.ea;
}

static inline __attribute__((always_inline, flatten)) void jsr() {
    push16(cpu.pc - 1);

    cpu.pc = cpu.ea;
}

static inline __attribute__((always_inline, flatten)) void lda() {
    cpu.a = getvalue();

    zerocalc(cpu.a);
    signcalc(cpu.a);
}

static inline __attribute__((always_inline, flatten)) void ldaZP() {
    cpu.a = getvalueZP();

    zerocalc(cpu.a);
    signcalc(cpu.a);
}

static inline __attribute__((always_inline, flatten)) void ldx() {
    cpu.x = getvalue();

    zerocalc(cpu.x);
    signcalc(cpu.x);
}

static inline __attribute__((always_inline, flatten)) void ldxZP() {
    cpu.x = getvalue();

    zerocalc(cpu.x);
    signcalc(cpu.x);
}

static inline __attribute__((always_inline, flatten)) void ldy() {
    cpu.y = getvalue();

    zerocalc(cpu.y);
    signcalc(cpu.y);
}

static inline __attribute__((always_inline, flatten)) void ldyZP() {
    cpu.y = getvalueZP();

    zerocalc(cpu.y);
    signcalc(cpu.y);
}

static inline __attribute__((always_inline, flatten)) void lsr() {
    uint32_t value = getvalue();
    uint32_t result = value >> 1;

    if(value & 1) {
        setcarry();
    } else {
        clearcarry();
    }

    zerocalc(result);
    signcalc(result);
    putvalue(result);
}

static inline __attribute__((always_inline, flatten)) void lsrZP() {
    uint32_t value = getvalue();
    uint32_t result = value >> 1;

    if(value & 1) {
        setcarry();
    } else {
        clearcarry();
    }

    zerocalc(result);
    //clearsign();
    signcalc(result);
    putvalue(result);
}

static inline __attribute__((always_inline, flatten)) void lsra() {
    uint8_t value = cpu.a;
    uint8_t result = value >> 1;

    if(value & 1) {
        setcarry();
    } else {
        clearcarry();
    }

    zerocalc(result);
    //clearsign();
    signcalc(result);
    saveaccum(result);
}

static inline __attribute__((always_inline, flatten)) void ora() {
    uint32_t result = cpu.a | getvalue();

    zerocalc(result);
    signcalc(result);
    saveaccum(result);
}

static inline __attribute__((always_inline, flatten)) void oraZP() {
    uint32_t result = cpu.a | getvalueZP();

    zerocalc(result);
    signcalc(result);
    saveaccum(result);
}

static inline __attribute__((always_inline, flatten)) void pha() {
    push8(cpu.a);
}

static inline __attribute__((always_inline, flatten)) void php() {
    push8(cpu.cpustatus | FLAG_BREAK);
}

static inline __attribute__((always_inline, flatten)) void pla() {
    cpu.a = pull8();

    zerocalc(cpu.a);
    signcalc(cpu.a);
}

static inline __attribute__((always_inline, flatten)) void plp() {
    cpu.cpustatus = (pull8() & 0xef) | FLAG_CONSTANT;
}

static inline __attribute__((always_inline, flatten)) void rol() {
    uint16_t value = getvalue();
    uint16_t result = (value << 1) | (cpu.cpustatus & FLAG_CARRY);

    carrycalc(result);
    zerocalc(result);
    signcalc(result);
    putvalue(result);
}

static inline __attribute__((always_inline, flatten)) void rolZP() {
    uint16_t value = getvalueZP();
    uint16_t result = (value << 1) | (cpu.cpustatus & FLAG_CARRY);

    carrycalc(result);
    zerocalc(result);
    signcalc(result);
    putvalue(result);
}

static inline __attribute__((always_inline, flatten)) void rola() {
    uint16_t value = cpu.a;
    uint16_t result = (value << 1) | (cpu.cpustatus & FLAG_CARRY);

    carrycalc(result);
    zerocalc(result);
    signcalc(result);
    saveaccum(result);
}

static inline __attribute__((always_inline, flatten)) void ror() {
    uint32_t value = getvalue();
    uint16_t result = (value >> 1) | ((cpu.cpustatus & FLAG_CARRY) << 7);

    if(value & 1) {
        setcarry();
    } else {
        clearcarry();
    }

    zerocalc(result);
    signcalc(result);
    putvalue(result);
}

static inline __attribute__((always_inline, flatten)) void rorZP() {
    uint32_t value = getvalueZP();
    uint16_t result = (value >> 1) | ((cpu.cpustatus & FLAG_CARRY) << 7);

    if(value & 1) {
        setcarry();
    } else {
        clearcarry();
    }

    zerocalc(result);
    signcalc(result);
    putvalue(result);
}

static inline __attribute__((always_inline, flatten)) void rora() {
    uint32_t value = cpu.a;
    uint16_t result = (value >> 1) | ((cpu.cpustatus & FLAG_CARRY) << 7);

    if(value & 1) {
        setcarry();
    } else {
        clearcarry();
    }

    zerocalc(result);
    signcalc(result);
    saveaccum(result);
}


static inline __attribute__((always_inline, flatten)) void rti() {
    cpu.cpustatus = pull8();
    cpu.pc = pull16();
}

static inline __attribute__((always_inline, flatten)) void rts() {
    cpu.pc = pull16() + 1;
}

static inline __attribute__((always_inline, flatten)) void sbc() {
    unsigned data = getvalue();
    _sbc(data);
}

static inline __attribute__((always_inline, flatten)) void sbcZP() {
    unsigned data = getvalueZP();
    _sbc(data);
}


static inline __attribute__((always_inline, flatten)) void sec() {
    setcarry();
}

static inline __attribute__((always_inline, flatten)) void sed() {
    setdecimal();
}

static inline __attribute__((always_inline, flatten)) void sei_() {
    setinterrupt();
}

static inline __attribute__((always_inline, flatten)) void sta() {
    putvalue(cpu.a);
}

static inline __attribute__((always_inline, flatten)) void stx() {
    putvalue(cpu.x);
}

static inline __attribute__((always_inline, flatten)) void sty() {
    putvalue(cpu.y);
}

static inline __attribute__((always_inline, flatten)) void tax() {
    cpu.x = cpu.a;

    zerocalc(cpu.x);
    signcalc(cpu.x);
}

static inline __attribute__((always_inline, flatten)) void tay() {
    cpu.y = cpu.a;

    zerocalc(cpu.y);
    signcalc(cpu.y);
}

static inline __attribute__((always_inline, flatten)) void tsx() {
    cpu.x = cpu.sp;

    zerocalc(cpu.x);
    signcalc(cpu.x);
}

static inline __attribute__((always_inline, flatten)) void txa() {
    cpu.a = cpu.x;

    zerocalc(cpu.a);
    signcalc(cpu.a);
}

static inline __attribute__((always_inline, flatten)) void txs() {
    cpu.sp = cpu.x;
}

static inline __attribute__((always_inline, flatten)) void tya() {
    cpu.a = cpu.y;

    zerocalc(cpu.a);
    signcalc(cpu.a);
}


//undocumented instructions
static inline __attribute__((always_inline, flatten)) void lax() {
    lda();
    ldx();
}

static inline __attribute__((always_inline, flatten)) void sax() {
    sta();
    stx();
    putvalue(cpu.a & cpu.x);
}

static inline __attribute__((always_inline, flatten)) void dcp() {
    dec();
    cmp();
}

static inline __attribute__((always_inline, flatten)) void isb() {
    inc();
    sbc();
}

static inline __attribute__((always_inline, flatten)) void slo() {
    asl();
    ora();
}

static inline __attribute__((always_inline, flatten)) void rla() {
    rol();
    op_and();
}

static inline __attribute__((always_inline, flatten)) void sre() {
    lsr();
    eor();
}

static inline __attribute__((always_inline, flatten)) void rra() {
    ror();
    adc();
}

static inline __attribute__((always_inline, flatten)) void alr() { // (FB)
    uint32_t result = cpu.a & getvalue();

    if(result & 1) {
        setcarry();
    } else {
        clearcarry();
    }

    result = result / 2;

    clearsign();
    zerocalc(result);
    saveaccum(result);
}

static inline __attribute__((always_inline, flatten)) void arr() { //This one took me hours.. finally taken from VICE (FB)
    uint32_t result;

    result = cpu.a & getvalue();

    if(!(cpu.cpustatus & FLAG_DECIMAL)) {
        result >>= 1;
        result |= ((cpu.cpustatus & FLAG_CARRY) << 7);

        signcalc(result);
        zerocalc(result);

        if(result & 0x40) {
            setcarry();
        } else {
            clearcarry();
        }
        if((result & 0x40) ^ ((result & 0x20) << 1)) {
            setoverflow();
        } else {
            clearoverflow();
        }

        saveaccum(result);
    } else {
        uint32_t t2 = result;

        t2 >>= 1;
        t2 |= ((cpu.cpustatus & FLAG_CARRY) << 7);

        if(cpu.cpustatus & FLAG_CARRY) {
            setsign();
        } else {
            clearsign();
        }

        zerocalc(t2);

        if((t2 ^ result) & 0x40) {
            setoverflow();
        } else {
            clearoverflow();
        }
        if(((result & 0xf) + (result & 0x1)) > 0x5) {
            t2 = (t2 & 0xf0) | ((t2 + 0x6) & 0xf);
        }
        if(((result & 0xf0) + (result & 0x10)) > 0x50) {
            t2 = (t2 & 0x0f) | ((t2 + 0x60) & 0xf0);
            setcarry();
        } else {
            clearcarry();
        }

        saveaccum(t2);
    }
}

static inline __attribute__((always_inline, flatten)) void xaa() { // AKA ANE
    const uint32_t val = 0xee; // VICE uses 0xff - but this results in an error in the testsuite (FB)

    uint32_t result = (cpu.a | val) & cpu.x & getvalue();

    signcalc(result);
    zerocalc(result);
    saveaccum(result);
}

static inline __attribute__((always_inline, flatten)) void lxa() {
    const uint32_t val = 0xee;

    uint32_t result = (cpu.a | val) & getvalue();

    signcalc(result);
    zerocalc(result);

    cpu.x = result;

    saveaccum(result);
}

static inline __attribute__((always_inline, flatten)) void axs() { //aka SBX
    uint32_t result = getvalue();

    result = (cpu.a & cpu.x) - result;
    cpu.x = result;

    if(result < 0x100) {
        setcarry();
    } else {
        clearcarry();
    }

    zerocalc(cpu.x);
    signcalc(cpu.x);
}

static inline __attribute__((always_inline, flatten)) void ahx() { //todo (is unstable)
    UNSUPPORTED
}

static inline __attribute__((always_inline, flatten)) void anc() {
    uint32_t result = cpu.a & getvalue();

    signcalc(result)
    zerocalc(result);

    if(cpu.cpustatus & FLAG_SIGN) {
        setcarry();
    } else {
        clearcarry();
    }

    saveaccum(result);
}

static inline __attribute__((always_inline, flatten)) void las() {
    uint32_t result = cpu.sp & getvalue();

    signcalc(result);
    zerocalc(result);

    cpu.sp = result;
    cpu.x = result;

    saveaccum(result);
}

/********************************************************************************************************************/
/* staticS																																																					*/
/********************************************************************************************************************/

static void opKIL(void) {
    Serial.print("CPU JAM @ $");
    Serial.println(cpu.pc);

    cpu_reset();
}

static void op0x0(void) {
    cpu.ticks = 7;

    imp();
    brk();
}

static void op0x1(void) {
    cpu.ticks = 6;

    indx();
    ora();
}

static void op0x3(void) { //undocumented
    cpu.ticks = 8;

    indx();
    slo();
}

static void op0x4(void) { //nop read zeropage
    cpu.ticks = 3;

    zp();
}

static void op0x5(void) {
    cpu.ticks = 3;

    zp();
    oraZP();
}

static void op0x6(void) {
    cpu.ticks = 5;

    zp();
    aslZP();
}

static void op0x7(void) { //undocumented SLO
    cpu.ticks = 5;

    zp();
    slo();
}

static void op0x8(void) {
    cpu.ticks = 3;

    imp();
    php();
}

static void op0x9(void) {
    cpu.ticks = 2;

    imm();
    ora();
}

static void op0xA(void) {
    cpu.ticks = 2;

    //acc();
    asla();
}

static void op0xB(void) { //undocumented
    cpu.ticks = 2;

    imm();
    anc();
}

static void op0xC(void) { //nop
    cpu.ticks = 4;

    abso();
}

static void op0xD(void) {
    cpu.ticks = 4;

    abso();
    ora();
}

static void op0xE(void) {
    cpu.ticks = 6;

    abso();
    asl();
}

static void op0xF(void) { //undocumented
    cpu.ticks = 6;

    abso();
    slo();
}

static void op0x10(void) {
    cpu.ticks = 2;

    rel();
    bpl();
}

static void op0x11(void) {
    cpu.ticks = 5;

    indy_t();
    ora();
}

static void op0x13(void) { //undocumented
    cpu.ticks = 8;

    indy();
    slo();
}

static void op0x14(void) { //nop
    cpu.ticks = 4;

    zpx();
}

static void op0x15(void) {
    cpu.ticks = 4;

    zpx();
    ora();
}

static void op0x16(void) {
    cpu.ticks = 6;

    zpx();
    asl();
}

static void op0x17(void) { //undocumented
    cpu.ticks = 6;

    //zpy(); bug
    zpx();
    slo();
}

static void op0x18(void) {
    cpu.ticks = 2;

    imp();
    clc();
}

static void op0x19(void) {
    cpu.ticks = 4;

    absy_t();
    ora();
}

static void op0x1A(void) { //nop
    cpu.ticks = 2;
}

static void op0x1B(void) { //undocumented
    cpu.ticks = 7;

    absy();
    slo();
}

static void op0x1C(void) { //nop
    cpu.ticks = 4;
    //();
}

static void op0x1D(void) {
    cpu.ticks = 4;

    absx_t();
    ora();
}

static void op0x1E(void) {
    cpu.ticks = 7;

    absx();
    asl();
}


static void op0x1F(void) { //undocumented
    cpu.ticks = 7;

    absx();
    slo();
}

static void op0x20(void) {
    cpu.ticks = 6;

    abso();
    jsr();
}

static void op0x21(void) {
    cpu.ticks = 6;

    indx();
    op_and();
}

static void op0x23(void) { //undocumented
    cpu.ticks = 8;

    indx();
    rla();
}

static void op0x24(void) {
    cpu.ticks = 3;

    zp();
    op_bitZP();
}

static void op0x25(void) {
    cpu.ticks = 3;

    zp();
    op_and();
}

static void op0x26(void) {
    cpu.ticks = 5;

    zp();
    rolZP();
}

static void op0x27(void) { //undocumented
    cpu.ticks = 5;

    zp();
    rla();
}

static void op0x28(void) {
    cpu.ticks = 4;

    imp();
    plp();
}

static void op0x29(void) {
    cpu.ticks = 2;

    imm();
    op_and();
}

static void op0x2A(void) {
    cpu.ticks = 2;

    //acc();
    rola();
}

static void op0x2B(void) { //undocumented
    cpu.ticks = 2;

    imm();
    anc();
}

static void op0x2C(void) {
    cpu.ticks = 4;

    abso();
    op_bit();
}

static void op0x2D(void) {
    cpu.ticks = 4;

    abso();
    op_and();
}

static void op0x2E(void) {
    cpu.ticks = 6;

    abso();
    rol();
}

static void op0x2F(void) { //undocumented
    cpu.ticks = 6;

    abso();
    rla();
}

static void op0x30(void) {
    cpu.ticks = 2;

    rel();
    bmi();
}

static void op0x31(void) {
    cpu.ticks = 5;

    indy_t();
    op_and();
}

static void op0x33(void) { //undocumented
    cpu.ticks = 8;

    indy();
    rla();
}

static void op0x34(void) { //nop
    cpu.ticks = 4;

    zpx();
}

static void op0x35(void) {
    cpu.ticks = 4;

    zpx();
    op_and();
}

static void op0x36(void) {
    cpu.ticks = 6;

    zpx();
    rol();
}

static void op0x37(void) { //undocumented
    cpu.ticks = 6;

    zpx();
    rla();
}

static void op0x38(void) {
    cpu.ticks = 2;

    imp();
    sec();
}

static void op0x39(void) {
    cpu.ticks = 4;

    absy_t();
    op_and();
}

static void op0x3A(void) { //nop
    cpu.ticks = 2;
}

static void op0x3B(void) { //undocumented
    cpu.ticks = 7;

    absy();
    rla();
}

static void op0x3C(void) { //nop
    cpu.ticks = 4;

    absx_t();
}

static void op0x3D(void) {
    cpu.ticks = 4;

    absx_t();
    op_and();
}

static void op0x3E(void) {
    cpu.ticks = 7;

    absx();
    rol();
}

static void op0x3F(void) { //undocumented
    cpu.ticks = 7;

    absx();
    rla();
}

static void op0x40(void) {
    cpu.ticks = 6;

    imp();
    rti();
}

static void op0x41(void) {
    cpu.ticks = 6;

    indx();
    eor();
}

static void op0x43(void) { //undocumented
    cpu.ticks = 8;

    indx();
    sre();
}

static void op0x44(void) { //nop
    cpu.ticks = 3;

    zp();
}

static void op0x45(void) {
    cpu.ticks = 3;

    zp();
    eorZP();
}

static void op0x46(void) {
    cpu.ticks = 5;

    zp();
    lsrZP();
}

static void op0x47(void) { //undocumented
    cpu.ticks = 5;

    zp();
    sre();
}

static void op0x48(void) {
    cpu.ticks = 3;

    imp();
    pha();
}

static void op0x49(void) {
    cpu.ticks = 2;

    imm();
    eor();
}

static void op0x4A(void) {
    cpu.ticks = 2;

//	acc();
    lsra();
}

static void op0x4B(void) { //undocumented
    cpu.ticks = 2;

    imm();
    alr();
}

static void op0x4C(void) {
    cpu.ticks = 3;

    abso();
    jmp();
}

static void op0x4D(void) {
    cpu.ticks = 4;

    abso();
    eor();
}

static void op0x4E(void) {
    cpu.ticks = 6;

    abso();
    lsr();
}

static void op0x4F(void) { //undocumented
    cpu.ticks = 6;

    abso();
    sre();
}

static void op0x50(void) {
    cpu.ticks = 2;

    rel();
    bvc();
}

static void op0x51(void) {
    cpu.ticks = 5;

    indy_t();
    eor();
}

static void op0x53(void) { //undocumented
    cpu.ticks = 8;

    //zp(); BUG
    indy();
    sre();
}

static void op0x54(void) { //nop
    cpu.ticks = 4;

    zpx();
}

static void op0x55(void) {
    cpu.ticks = 4;

    zpx();
    eor();
}

static void op0x56(void) {
    cpu.ticks = 6;

    zpx();
    lsr();
}

static void op0x57(void) { //undocumented
    cpu.ticks = 6;

    zpx();
    sre();
}

static void op0x58(void) {
    cpu.ticks = 2;

    imp();
    cli_();
}

static void op0x59(void) {
    cpu.ticks = 4;

    absy_t();
    eor();
}

static void op0x5A(void) { //nop
    cpu.ticks = 2;
}

static void op0x5B(void) { //undocumented
    cpu.ticks = 7;

    absy();
    sre();
}

static void op0x5C(void) { //nop
    cpu.ticks = 4;

    absx_t();
}

static void op0x5D(void) {
    cpu.ticks = 4;

    absx_t();
    eor();
}

static void op0x5E(void) {
    cpu.ticks = 7;

    absx();
    lsr();
}

static void op0x5F(void) { //undocumented
    cpu.ticks = 7;

    absx();
    sre();
}

static void op0x60(void) {
    cpu.ticks = 6;

    imp();
    rts();
}

static void op0x61(void) {
    cpu.ticks = 6;

    indx();
    adc();
}

static void op0x63(void) { //undocumented
    cpu.ticks = 8;

    indx();
    rra();
}

static void op0x64(void) {
    cpu.ticks = 3;

    zp();
}

static void op0x65(void) {
    cpu.ticks = 3;

    zp();
    adcZP();
}

static void op0x66(void) {
    cpu.ticks = 5;

    zp();
    rorZP();
}

static void op0x67(void) { //undocumented
    cpu.ticks = 5;

    zp();
    rra();
}

static void op0x68(void) {
    cpu.ticks = 4;

    imp();
    pla();
}

static void op0x69(void) {
    cpu.ticks = 2;
    
    imm();
    adc();
}

static void op0x6A(void) {
    cpu.ticks = 2;

//	acc();
    rora();
}

static void op0x6B(void) { //undocumented
    cpu.ticks = 2;

    imm();
    arr();
}

static void op0x6C(void) {
    cpu.ticks = 5;

    ind();
    jmp();
}

static void op0x6D(void) {
    cpu.ticks = 4;

    abso();
    adc();
}

static void op0x6E(void) {
    cpu.ticks = 6;

    abso();
    ror();
}

static void op0x6F(void) { //undocumented
    cpu.ticks = 6;

    abso();
    rra();
}

static void op0x70(void) {
    cpu.ticks = 2;

    rel();
    bvs();
}

static void op0x71(void) {
    cpu.ticks = 5;

    indy_t();
    adc();
}

static void op0x73(void) { //undocumented
    cpu.ticks = 8;

    indy();
    rra();
}

static void op0x74(void) { //nop
    cpu.ticks = 4;

    zpx();
}

static void op0x75(void) {
    cpu.ticks = 4;

    zpx();
    adc();
}

static void op0x76(void) {
    cpu.ticks = 6;

    zpx();
    ror();
}

static void op0x77(void) { //undocumented
    cpu.ticks = 6;

    zpx();
    rra();
}

static void op0x78(void) {
    cpu.ticks = 2;

    imp();
    sei_();
}

static void op0x79(void) {
    cpu.ticks = 4;

    absy_t();
    adc();
}

static void op0x7A(void) { //nop
    cpu.ticks = 2;
}

static void op0x7B(void) { //undocumented
    cpu.ticks = 7;

    absy();
    rra();
}

static void op0x7C(void) { //nop
    cpu.ticks = 4;

    absx_t();
}

static void op0x7D(void) {
    cpu.ticks = 4;

    absx_t();
    adc();
}

static void op0x7E(void) {
    cpu.ticks = 7;

    absx();
    ror();
}

static void op0x7F(void) { //undocumented
    cpu.ticks = 7;

    absx();
    rra();
}

static void op0x80(void) { //nop
    cpu.ticks = 2;

    imm();
}

static void op0x81(void) {
    cpu.ticks = 6;

    indx();
    sta();
}

static void op0x82(void) { //nop
    cpu.ticks = 2;

    imm();
}

static void op0x83(void) { //undocumented
    cpu.ticks = 6;

    indx();
    sax();
}

static void op0x84(void) {
    cpu.ticks = 3;

    zp();
    sty();
}

static void op0x85(void) {
    cpu.ticks = 3;

    zp();
    sta();
}

static void op0x86(void) {
    cpu.ticks = 3;

    zp();
    stx();
}

static void op0x87(void) { //undocumented
    cpu.ticks = 3;

    zp();
    sax();
}

static void op0x88(void) {
    cpu.ticks = 2;

    imp();
    dey();
}

static void op0x89(void) { //nop
    cpu.ticks = 2;

    imm();
}

static void op0x8A(void) {
    cpu.ticks = 2;

    imp();
    txa();
}

static void op0x8B(void) { //undocumented
    cpu.ticks = 2;

    imm();
    xaa();
}

static void op0x8C(void) {
    cpu.ticks = 4;

    abso();
    sty();
}

static void op0x8D(void) {
    cpu.ticks = 4;

    abso();
    sta();
}

static void op0x8E(void) {
    cpu.ticks = 4;

    abso();
    stx();
}

static void op0x8F(void) { //undocumented
    cpu.ticks = 4;

    abso();
    sax();
}

static void op0x90(void) {
    cpu.ticks = 2;

    rel();
    bcc();
}

static void op0x91(void) {
    cpu.ticks = 6;

    indy();
    sta();
}

static void op0x93(void) { //undocumented
    cpu.ticks = 6;

    indy();
    ahx();
}

static void op0x94(void) {
    cpu.ticks = 4;

    zpx();
    sty();
}

static void op0x95(void) {
    cpu.ticks = 4;

    zpx();
    sta();
}

static void op0x96(void) {
    cpu.ticks = 4;

    zpy();
    stx();
}

static void op0x97(void) { //undocumented
    cpu.ticks = 4;

    zpy();
    sax();
}

static void op0x98(void) {
    cpu.ticks = 2;

    imp();
    tya();
}

static void op0x99(void) {
    cpu.ticks = 5;

    absy();
    sta();
}

static void op0x9A(void) {
    cpu.ticks = 2;

    imp();
    txs();
}

static void op0x9B(void) { //undocumented
    cpu.ticks = 5;

    absy();
    //tas();
    UNSUPPORTED;
}

static void op0x9C(void) { //undocumented
    cpu.ticks = 5;

    absy();
    //shy();
    UNSUPPORTED;
}

static void op0x9D(void) {
    cpu.ticks = 5;

    absx();
    sta();
}

static void op0x9E(void) { //undocumented
    cpu.ticks = 5;

    absx();
    //shx();
}

static void op0x9F(void) { //undocumented
    cpu.ticks = 5;

    absx();
    ahx();
}

static void op0xA0(void) {
    cpu.ticks = 2;

    imm();
    ldy();
}

static void op0xA1(void) {
    cpu.ticks = 6;

    indx();
    lda();
}

static void op0xA2(void) {
    cpu.ticks = 2;

    imm();
    ldx();
}

static void op0xA3(void) { //undocumented
    cpu.ticks = 6;

    indx();
    lax();
}

static void op0xA4(void) {
    cpu.ticks = 3;

    zp();
    ldyZP();
}

static void op0xA5(void) {
    cpu.ticks = 3;

    zp();
    ldaZP();
}

static void op0xA6(void) {
    cpu.ticks = 3;

    zp();
    ldxZP();
}

static void op0xA7(void) { //undocumented
    cpu.ticks = 3;

    zp();
    lax();
}

static void op0xA8(void) {
    cpu.ticks = 2;

    imp();
    tay();
}

static void op0xA9(void) {
    cpu.ticks = 2;

    imm();
    lda();
}

static void op0xAA(void) {
    cpu.ticks = 2;

    imp();
    tax();
}

static void op0xAB(void) { //undocumented
    cpu.ticks = 2;

    imm();
    lxa();
}

static void op0xAC(void) {
    cpu.ticks = 4;

    abso();
    ldy();
}

static void op0xAD(void) {
    cpu.ticks = 4;

    abso();
    lda();
}

static void op0xAE(void) {
    cpu.ticks = 4;

    abso();
    ldx();
}

static void op0xAF(void) { //undocumented
    cpu.ticks = 4;

    abso();
    lax();
}

static void op0xB0(void) {
    cpu.ticks = 2;

    rel();
    bcs();
}

static void op0xB1(void) {
    cpu.ticks = 5;

    indy_t();
    lda();
}

static void op0xB3(void) { //undocumented
    cpu.ticks = 5;

    indy_t();
    lax();
}

static void op0xB4(void) {
    cpu.ticks = 4;

    zpx();
    ldy();
}

static void op0xB5(void) {
    cpu.ticks = 4;

    zpx();
    lda();
}

static void op0xB6(void) {
    cpu.ticks = 4;

    zpy();
    ldx();
}

static void op0xB7(void) { //undocumented
    cpu.ticks = 4;

    zpy();
    lax();
}

static void op0xB8(void) {
    cpu.ticks = 2;

    imp();
    clv();
}

static void op0xB9(void) {
    cpu.ticks = 4;

    absy_t();
    lda();
}

static void op0xBA(void) {
    cpu.ticks = 2;

    imp();
    tsx();
}

static void op0xBB(void) { //undocumented
    cpu.ticks = 4;

    absy_t();
    las();
}

static void op0xBC(void) {
    cpu.ticks = 4;

    absx_t();
    ldy();
}

static void op0xBD(void) {
    cpu.ticks = 4;

    absx_t();
    lda();
}

static void op0xBE(void) {
    cpu.ticks = 4;

    absy_t();
    ldx();
}

static void op0xBF(void) { //undocumented
    cpu.ticks = 4;

    absy_t();
    lax();
}

static void op0xC0(void) {
    cpu.ticks = 2;

    imm();
    cpy();
}

static void op0xC1(void) {
    cpu.ticks = 6;

    indx();
    cmp();
}

static void op0xC2(void) { //nop
    cpu.ticks = 2;

    imm();
}

static void op0xC3(void) { //undocumented
    cpu.ticks = 8;

    indx();
    dcp();
}

static void op0xC4(void) {
    cpu.ticks = 3;

    zp();
    cpyZP();
}

static void op0xC5(void) {
    cpu.ticks = 3;

    zp();
    cmpZP();
}

static void op0xC6(void) {
    cpu.ticks = 5;

    zp();
    decZP();
}

static void op0xC7(void) { //undocumented
    cpu.ticks = 5;

    zp();
    dcp();
}

static void op0xC8(void) {
    cpu.ticks = 2;

    imp();
    iny();
}

static void op0xC9(void) {
    cpu.ticks = 2;

    imm();
    cmp();
}

static void op0xCA(void) {
    cpu.ticks = 2;

    imp();
    dex();
}

static void op0xCB(void) { //undocumented
    cpu.ticks = 2;

    imm();
    axs();
}

static void op0xCC(void) {
    cpu.ticks = 4;

    abso();
    cpy();
}

static void op0xCD(void) {
    cpu.ticks = 4;

    abso();
    cmp();
}

static void op0xCE(void) {
    cpu.ticks = 6;

    abso();
    dec();
}

static void op0xCF(void) { //undocumented
    cpu.ticks = 6;

    abso();
    dcp();
}

static void op0xD0(void) {
    cpu.ticks = 2;

    rel();
    bne();
}

static void op0xD1(void) {
    cpu.ticks = 5;

    indy_t();
    cmp();
}

static void op0xD3(void) { //undocumented
    cpu.ticks = 8;

    indy();
    dcp();
}

static void op0xD4(void) { //nop
    cpu.ticks = 4;

    zpx();
}

static void op0xD5(void) {
    cpu.ticks = 4;

    zpx();
    cmp();
}

static void op0xD6(void) {
    cpu.ticks = 6;

    zpx();
    dec();
}

static void op0xD7(void) { //undocumented
    cpu.ticks = 6;

    zpx();
    dcp();
}

static void op0xD8(void) {
    cpu.ticks = 2;

    imp();
    cld();
}

static void op0xD9(void) {
    cpu.ticks = 4;

    absy_t();
    cmp();
}

static void op0xDA(void) { //nop
    cpu.ticks = 2;
}

static void op0xDB(void) { //undocumented
    cpu.ticks = 7;

    absy();
    dcp();
}

static void op0xDC(void) { //nop
    cpu.ticks = 4;

    absx_t();
}

static void op0xDD(void) {
    cpu.ticks = 4;

    absx_t();
    cmp();
}

static void op0xDE(void) {
    cpu.ticks = 7;

    absx();
    dec();
}

static void op0xDF(void) { //undocumented
    cpu.ticks = 7;

    absx();
    dcp();
}

static void op0xE0(void) {
    cpu.ticks = 2;

    imm();
    cpx();
}

static void op0xE1(void) {
    cpu.ticks = 5;

    indx();
    sbc();
}

static void op0xE2(void) { //NOP
    cpu.ticks = 2;

    imm();
}

static void op0xE3(void) { //undocumented
    cpu.ticks = 8;

    indx();
    isb();
}

static void op0xE4(void) {
    cpu.ticks = 3;

    zp();
    cpxZP();
}

static void op0xE5(void) {
    cpu.ticks = 3;

    zp();
    sbcZP();
}

static void op0xE6(void) {
    cpu.ticks = 5;

    zp();
    incZP();
}

static void op0xE7(void) { //undocumented
    cpu.ticks = 5;

    zp();
    isb();
}

static void op0xE8(void) {
    cpu.ticks = 2;

    imp();
    inx();
}

static void op0xE9(void) {
    cpu.ticks = 2;

    imm();
    sbc();
}

static void op0xEA(void) {
    cpu.ticks = 2;
}

static void op0xEB(void) {
    cpu.ticks = 2;
    imm();
    sbc();
}

static void op0xEC(void) {
    cpu.ticks = 4;
    abso();
    cpx();
}

static void op0xED(void) {
    cpu.ticks = 4;
    abso();
    sbc();
}

static void op0xEE(void) {
    cpu.ticks = 6;
    abso();
    inc();
}

static void op0xEF(void) { //undocumented
    cpu.ticks = 6;
    abso();
    isb();
}

static void op0xF0(void) {
    cpu.ticks = 2;
    rel();
    beq();
}

static void op0xF1(void) {
    cpu.ticks = 5;
    indy_t();
    sbc();
}

static void op0xF3(void) { //undocumented
    cpu.ticks = 8;
    indy();
    isb();
}

static void op0xF4(void) { //nop
    cpu.ticks = 4;
    zpx();
}

static void op0xF5(void) {
    cpu.ticks = 4;

    zpx();
    sbc();
}

static void op0xF6(void) {
    cpu.ticks = 6;

    zpx();
    inc();
}

static void op0xF7(void) { //undocumented
    cpu.ticks = 6;

    zpx();
    isb();
}

static void op0xF8(void) {
    cpu.ticks = 2;

    imp();
    sed();
}

static void op0xF9(void) {
    cpu.ticks = 4;

    absy_t();
    sbc();
}

static void op0xFA(void) { //nop
    cpu.ticks = 2;
}

static void op0xFB(void) { //undocumented
    cpu.ticks = 7;

    absy();
    isb();
}

static void op0xFC(void) { //nop
    cpu.ticks = 4;

    absx_t();
}

static void op0xFD(void) {
    cpu.ticks = 4;

    absx_t();
    sbc();
}

static void op0xFE(void) {
    cpu.ticks = 7;

    absx();
    inc();
}

static void op0xFF(void) { //undocumented
    cpu.ticks = 7;

    absx();
    isb();
}

static void opPATCHD2(void) {
#if APPLY_PATCHES
    patchLOAD();
#else
    opKIL();
#endif
}

static void opPATCHF2(void) {
#if APPLY_PATCHES
    patchSAVE();
#else
    opKIL();
#endif
}

using op_ptr_t = void (*)(void);

static const op_ptr_t statictable[256] = {
        /*        	0   	1   	2   	3   	4   	5   	6   	7   	8	   9   		A   	B   	C   	D   	E   	F */
        /* 0  */    op0x0, op0x1, opKIL, op0x3, op0x4, op0x5, op0x6, op0x7, op0x8, op0x9, op0xA, op0xB, op0xC, op0xD,
                    op0xE, op0xF,
        /* 1  */    op0x10, op0x11, opKIL, op0x13, op0x14, op0x15, op0x16, op0x17, op0x18, op0x19, op0x1A, op0x1B,
                    op0x1C, op0x1D, op0x1E, op0x1F,
        /* 2  */    op0x20, op0x21, opKIL, op0x23, op0x24, op0x25, op0x26, op0x27, op0x28, op0x29, op0x2A, op0x2B,
                    op0x2C, op0x2D, op0x2E, op0x2F,
        /* 3  */    op0x30, op0x31, opKIL, op0x33, op0x34, op0x35, op0x36, op0x37, op0x38, op0x39, op0x3A, op0x3B,
                    op0x3C, op0x3D, op0x3E, op0x3F,
        /* 4  */    op0x40, op0x41, opKIL, op0x43, op0x44, op0x45, op0x46, op0x47, op0x48, op0x49, op0x4A, op0x4B,
                    op0x4C, op0x4D, op0x4E, op0x4F,
        /* 5  */    op0x50, op0x51, opKIL, op0x53, op0x54, op0x55, op0x56, op0x57, op0x58, op0x59, op0x5A, op0x5B,
                    op0x5C, op0x5D, op0x5E, op0x5F,
        /* 6  */    op0x60, op0x61, opKIL, op0x63, op0x64, op0x65, op0x66, op0x67, op0x68, op0x69, op0x6A, op0x6B,
                    op0x6C, op0x6D, op0x6E, op0x6F,
        /* 7  */    op0x70, op0x71, opKIL, op0x73, op0x74, op0x75, op0x76, op0x77, op0x78, op0x79, op0x7A, op0x7B,
                    op0x7C, op0x7D, op0x7E, op0x7F,
        /* 8  */    op0x80, op0x81, op0x82, op0x83, op0x84, op0x85, op0x86, op0x87, op0x88, op0x89, op0x8A, op0x8B,
                    op0x8C, op0x8D, op0x8E, op0x8F,
        /* 9  */    op0x90, op0x91, opKIL, op0x93, op0x94, op0x95, op0x96, op0x97, op0x98, op0x99, op0x9A, op0x9B,
                    op0x9C, op0x9D, op0x9E, op0x9F,
        /* A  */    op0xA0, op0xA1, op0xA2, op0xA3, op0xA4, op0xA5, op0xA6, op0xA7, op0xA8, op0xA9, op0xAA, op0xAB,
                    op0xAC, op0xAD, op0xAE, op0xAF,
        /* B  */    op0xB0, op0xB1, opKIL, op0xB3, op0xB4, op0xB5, op0xB6, op0xB7, op0xB8, op0xB9, op0xBA, op0xBB,
                    op0xBC, op0xBD, op0xBE, op0xBF,
        /* C  */    op0xC0, op0xC1, op0xC2, op0xC3, op0xC4, op0xC5, op0xC6, op0xC7, op0xC8, op0xC9, op0xCA, op0xCB,
                    op0xCC, op0xCD, op0xCE, op0xCF,
        /* D  */    op0xD0, op0xD1, opPATCHD2, op0xD3, op0xD4, op0xD5, op0xD6, op0xD7, op0xD8, op0xD9, op0xDA, op0xDB,
                    op0xDC, op0xDD, op0xDE, op0xDF,
        /* E  */    op0xE0, op0xE1, op0xE2, op0xE3, op0xE4, op0xE5, op0xE6, op0xE7, op0xE8, op0xE9, op0xEA, op0xEB,
                    op0xEC, op0xED, op0xEE, op0xEF,
        /* F  */    op0xF0, op0xF1, opPATCHF2, op0xF3, op0xF4, op0xF5, op0xF6, op0xF7, op0xF8, op0xF9, op0xFA, op0xFB,
                    op0xFC, op0xFD, op0xFE, op0xFF
};

__attribute__((unused)) static const uint8_t cyclesTable[256] =
        {
                7, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6,  // $00
                2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 5, 5, 7, 7,  // $10
                6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6,  // $20
                2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 5, 5, 7, 7,  // $30
                6, 6, 2, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6,  // $40
                2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 5, 5, 7, 7,  // $50
                6, 6, 2, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6,  // $60
                2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 5, 5, 7, 7,  // $70
                2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,  // $80
                2, 6, 2, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5,  // $90
                2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4,  // $A0
                2, 5, 2, 5, 4, 4, 4, 4, 2, 4, 2, 5, 4, 4, 4, 4,  // $B0
                2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,  // $C0
                2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 5, 5, 7, 7,  // $D0
                2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6,  // $E0
                2, 5, 2, 8, 4, 4, 6, 6, 2, 4, 2, 7, 5, 5, 7, 7   // $F0
        };

__attribute__((unused)) static const uint8_t writeCycleTable[256] =
        {
                3, 0, 0, 2, 0, 0, 2, 2, 1, 0, 0, 0, 0, 0, 2, 2, // $00
                0, 0, 0, 2, 0, 0, 2, 2, 0, 0, 0, 2, 0, 0, 2, 2, // $10
                2, 0, 0, 2, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 2, 2, // $20
                0, 0, 0, 2, 0, 0, 2, 2, 0, 0, 0, 2, 0, 0, 2, 2, // $30
                0, 0, 0, 2, 0, 0, 2, 2, 1, 0, 0, 0, 0, 0, 2, 2, // $40
                0, 0, 0, 2, 0, 0, 2, 2, 0, 0, 0, 2, 0, 0, 2, 2, // $50
                0, 0, 0, 2, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 2, 2, // $60
                0, 0, 0, 2, 0, 0, 2, 2, 0, 0, 0, 2, 0, 0, 2, 2, // $70
                0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, // $80
                0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, // $90
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // $A0
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // $B0
                0, 0, 0, 2, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 2, 2, // $C0
                0, 0, 0, 2, 0, 0, 2, 2, 0, 0, 0, 2, 0, 0, 2, 2, // $D0
                0, 0, 0, 2, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 2, 2, // $E0
                0, 0, 0, 2, 0, 0, 2, 2, 0, 0, 0, 2, 0, 0, 2, 2  // $F0
        };


void cpu_nmi() {
    cpu.nmiLine = 1;
    Serial.println("nmiLine=1");
}

void cpu_clearNmi() {
    cpu.nmi = 0;
}

void cpu_nmi_do() {
    if(cpu.nmi) { return; }
    cpu.nmi = 1;
    cpu.nmiLine = 0;
    push16(cpu.pc);
    push8(cpu.cpustatus & ~FLAG_BREAK);
    cpu.cpustatus |= FLAG_INTERRUPT;
    cpu.pc = read6502(0xFFFA) | (read6502(0xFFFB) << 8);
    cpu.ticks = 7;
}

static inline void cpu_irq() {
    push16(cpu.pc);
    push8(cpu.cpustatus & ~FLAG_BREAK);
    cpu.cpustatus |= FLAG_INTERRUPT;
    cpu.pc = read6502(0xFFFE) | (read6502(0xFFFF) << 8);
    cpu.ticks = 7;
}

inline void cia_clock()  __attribute__((always_inline));

void cia_clock() {
    cia1_clock(1);
    cia2_clock(1);
}

void cia_clockt(int ticks) {
    cia1_clock(ticks);
    cia2_clock(ticks);
}

void cpu_clock(int cycles) {
    static int c = 0;

    cpu.lineCyclesAbs += cycles;
    c += cycles;

    while(c > 0) {
        uint8_t opcode;
        cpu.ticks = 0;

        //NMI

        if(!cpu.nmi && ((cpu.cia2.R[CIA_ICR] & CIA_ICR_IR) | cpu.nmiLine)) {
            cpu_nmi_do();
            goto nostatic;
        }

        if(!(cpu.cpustatus & FLAG_INTERRUPT)) {
            if(((cpu.vic.R[VIC_IRQST] | cpu.cia1.R[CIA_ICR]) & CIA_ICR_IR)) {
                cpu_irq();
                goto nostatic;
            }
        }

        cpu.cpustatus |= FLAG_CONSTANT;
        opcode = read6502(cpu.pc++);
        statictable[opcode]();
        nostatic:

        cia_clockt(cpu.ticks);
        c -= cpu.ticks;
        cpu.lineCycles += cpu.ticks;

        if(cpu.exactTiming) {
            uint32_t t = cpu.lineCycles * MCU_C64_RATIO;
            while(ARM_DWT_CYCCNT - cpu.lineStartTime < t) {}
        }
    }
}

//Enable "ExactTiming" Mode
void cpu_setExactTiming() {
    if(!cpu.exactTiming) {
        //enable exact timing
        LED_ON();
        setAudioOff();
        tvic::displaySimpleModeScreen();
    }
    cpu.exactTiming = 1;
    cpu.exactTimingStartTime = ARM_DWT_CYCCNT;
}

//Disable "ExactTiming" Mode
void cpu_disableExactTiming() {
    cpu.exactTiming = 0;
    setAudioOn();
    LED_OFF();
}

void cpu_reset() {
    enableCycleCounter();
    cpu.exactTiming = 0;
    cpu.nmi = 0;
    cpu.cpustatus = FLAG_CONSTANT;
    cpu.pc = read6502(0xFFFC) | (read6502(0xFFFD) << 8);
    cpu.sp = 0xFD;
}