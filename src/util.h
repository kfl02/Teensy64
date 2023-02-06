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

#ifndef TEENSY64_UTIL_H
#define TEENSY64_UTIL_H

inline void AudioNoInterrupts() __attribute__((always_inline));
inline void AudioNoInterrupts() {
    NVIC_DISABLE_IRQ(IRQ_SOFTWARE);
}

inline void AudioInterrupts() __attribute__((always_inline));
inline void AudioInterrupts() {
    NVIC_ENABLE_IRQ(IRQ_SOFTWARE);
}

void disableEventResponder();
void enableCycleCounter();

inline uint32_t cycleCountMillis() __attribute__((always_inline));
inline uint32_t cycleCountMillis() {
    return (uint32_t)(ARM_DWT_CYCCNT * (1000.0 / F_CPU));
}

inline uint32_t cycleCountMicros() __attribute__((always_inline));
inline uint32_t cycleCountMicros() {
    return (uint32_t)(ARM_DWT_CYCCNT * (1000000.0 / F_CPU));
}

inline uint32_t cycleCountNanos() __attribute__((always_inline));
inline uint32_t cycleCountNanos() {
    return (uint32_t)(ARM_DWT_CYCCNT * (1000000000.0 / F_CPU));
}

float setAudioSampleFreq(float freq);
void setAudioOff();
void setAudioOn();
void listInterrupts();

#endif // TEENSY64_UTIL_H