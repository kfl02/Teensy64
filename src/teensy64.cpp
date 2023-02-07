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

#include <Arduino.h>
#include "teensy64.h"
#include "vic_palette.h"

ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST, TFT_MOSI, TFT_SCLK, TFT_MISO);

extern uint16_t screen[ILI9341_TFTHEIGHT][ILI9341_TFTWIDTH];

AudioPlaySID playSID;
AudioOutputAnalog audioout;

AudioConnection patchCord1(playSID, 0, audioout, 0);


SdFat SD;
bool SDinitialized = false;


void resetMachine() {
    *(volatile uint32_t *) 0xE000ED0C = 0x5FA0004;
    while(true) {}
}

void resetExternal() {
    //perform a Reset for external devices on IEC-Bus,
    //   detachInterrupt(digitalPinToInterrupt(PIN_RESET));

    digitalWriteFast(PIN_RESET, 0);
    delay(50);
    digitalWriteFast(PIN_RESET, 1);
}


void oneRasterLine() {
    static unsigned short lc = 1;

    while(true) {
        cpu.lineStartTime = ARM_DWT_CYCCNT;
        cpu.lineCycles = cpu.lineCyclesAbs = 0;

        if(!cpu.exactTiming) {
            tvic::render();
        } else {
            tvic::renderSimple();
        }

        if(--lc == 0) {
            lc = (unsigned short)LINEFREQ / 10; // 10Hz
            cia1_checkRTCAlarm();
            cia2_checkRTCAlarm();
        }

        //Switch "ExactTiming" Mode off after a while:
        if(!cpu.exactTiming) { break; }
        if(ARM_DWT_CYCCNT - cpu.exactTimingStartTime >= EXACTTIMINGDURATION * (F_CPU / 1000)) {
            cpu_disableExactTiming();
            break;
        }
    }
}

DMAChannel dma_gpio(false);

void setupGPIO_DMA() {
    dma_gpio.begin(true);

    dma_gpio.TCD->CSR = 0;
    dma_gpio.TCD->SADDR = &GPIOA_PDIR;
    dma_gpio.TCD->SOFF = (uintptr_t) &GPIOB_PDIR - (uintptr_t) &GPIOA_PDIR;
    dma_gpio.TCD->ATTR = DMA_TCD_ATTR_SSIZE(DMA_TCD_ATTR_SIZE_32BIT) | DMA_TCD_ATTR_DSIZE(DMA_TCD_ATTR_SIZE_32BIT);
    dma_gpio.TCD->NBYTES = sizeof(uint32_t);
    dma_gpio.TCD->SLAST = -5 * ((uintptr_t) &GPIOB_PDIR - (uintptr_t) &GPIOA_PDIR);

    dma_gpio.TCD->DADDR = &io;
    dma_gpio.TCD->DOFF = sizeof(uint32_t);
    dma_gpio.TCD->DLASTSGA = -5 * sizeof(uint32_t);

    dma_gpio.TCD->CITER = 5;
    dma_gpio.TCD->BITER = 5;

    //Audio-DAC is triggered with PDB(NO-VGA) or FTM-Channel 7
    //Use The Audio-DMA a trigger for this DMA-Channel:
    dma_gpio.triggerAtCompletionOf(AudioOutputAnalog::dma);
    dma_gpio.enable();
}


void initMachine() {

#if F_CPU < 240000000
#error Teensy64: Please select F_CPU=240MHz
#endif

#if F_BUS < 120000000
#error Teensy64: Please select F_BUS=120MHz
#endif

#if AUDIO_BLOCK_SAMPLES > 32
#error Teensy64: Set AUDIO_BLOCK_SAMPLES to 32
#endif

    unsigned long m = millis();

    //enable sd-card pullups early
    PORTE_PCR0 = PORT_PCR_MUX(1) | PORT_PCR_PE | PORT_PCR_PS;   /* PULLUP SDHC.D1  */
    PORTE_PCR1 = PORT_PCR_MUX(1) | PORT_PCR_PE | PORT_PCR_PS;   /* PULLUP SDHC.D0  */
    PORTE_PCR3 = PORT_PCR_MUX(1) | PORT_PCR_PE | PORT_PCR_PS;   /* PULLUP SDHC.CMD */
    PORTE_PCR4 = PORT_PCR_MUX(1) | PORT_PCR_PE | PORT_PCR_PS;   /* PULLUP SDHC.D3  */
    PORTE_PCR5 = PORT_PCR_MUX(1) | PORT_PCR_PE | PORT_PCR_PS;   /* PULLUP SDHC.D2  */


    // turn on USB host power for VGA Board
    PORTE_PCR6 = PORT_PCR_MUX(1);
    GPIOE_PDDR |= (1 << 6);
    GPIOE_PSOR = (1 << 6);

    pinMode(PIN_RESET, OUTPUT_OPENDRAIN);
    digitalWriteFast(PIN_RESET, 1);

    LED_INIT();
    LED_ON();
    disableEventResponder();

    Serial.begin(9600);
    Serial.println("Init");
    USBHost::begin();

    tft.begin(144000000);
    tft.setRotation(3);
    tft.setFrameBuffer(&screen[0][0]);
    tft.useFrameBuffer(true);
    tft.updateScreenAsync(true);

    SDinitialized = SD.begin(BUILTIN_SDCARD);

    float audioSampleFreq;

    audioSampleFreq = AUDIOSAMPLERATE;
    audioSampleFreq = setAudioSampleFreq(audioSampleFreq);

    playSID.setSampleParameters(CLOCKSPEED, audioSampleFreq);

    delay(250);

    while(!Serial && ((millis() - m) <= 1500));

    LED_OFF();

    Serial.println("=============================\n");
    Serial.println("Teensy64 v." VERSION " " __DATE__ " " __TIME__ "\n");

    Serial.print("SD Card ");
    Serial.println(SDinitialized ? "initialized." : "failed, or not present.");

    Serial.println();
    Serial.print("F_CPU (MHz): ");
    Serial.print((int) (F_CPU / 1e6));
    Serial.println();
    Serial.print("F_BUS (MHz): ");
    Serial.print((int) (F_BUS / 1e6));
    Serial.println();

    Serial.print("Emulated video: ");
    Serial.println(PAL ? "PAL" : "NTSC");
    Serial.print("Emulated video line frequency (Hz): ");
    Serial.println(LINEFREQ, 3);
    Serial.print("Emulated video refresh rate (Hz): ");
    Serial.println(REFRESHRATE, 3);

    Serial.println();
    Serial.print("Audioblock (Samples): ");
    Serial.print(AUDIO_BLOCK_SAMPLES);
    Serial.println();
    Serial.print("Audio Samplerate (Hz): ");
    Serial.println(audioSampleFreq);
    Serial.println();

    Serial.print("sizeof(tcpu) (Bytes): ");
    Serial.println(sizeof(tcpu));

    Serial.println();

    resetPLA();
    resetCia1();
    resetCia2();
    cpu.vic.reset();
    cpu_reset();

    resetExternal();

    while((millis() - m) <= 1500);

    setupGPIO_DMA();

    Serial.println("Starting.\n");

#if FASTBOOT
    cpu_clock(2e6);
    cpu.RAM[678] = (PAL == 1) ? 1 : 0; //PAL/NTSC switch, C64-Autodetection does not work with FASTBOOT
#endif

    cpu.vic.lineClock.begin(oneRasterLine, LINETIMER_DEFAULT_FREQ);
    cpu.vic.lineClock.priority(ISR_PRIORITY_RASTERLINE);

    attachInterrupt(digitalPinToInterrupt(PIN_RESET), resetMachine, RISING);

    listInterrupts();
}


// Switch off/replace Teensyduinos` yield and systick stuff

void yield(void) {
    static volatile uint8_t running = 0;

    if(running) { return; }

    running = 1;

    //Input via terminal to keyboardbuffer (for BASIC only)
    // kfl02: Temporarily disabled. My Teensy64 seems to get random garbage on the serial line.
//    if(Serial.available()) {
//        uint8_t r = Serial.read();
//        sendKey(r);
//        Serial.write(r);
//    }

    do_sendString();

    USBHost::Task();

    running = 0;
}

