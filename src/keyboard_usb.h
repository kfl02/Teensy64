
/* 
 * Copyright 2017 Paul Stoffregen (paul@pjrc.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//Modified for Teensy64 (c) F.Bösing

#pragma once

#ifndef TEENSY64_KEYBOARD_USB_H
#define TEENSY64_KEYBOARD_USB_H

#include <Arduino.h>
#include "USBHost_t36.h"  // Read this header first for key info


class c64USBKeyboard : public USBDriver { /* , public USBHIDInput */
public:
    using KBDLeds_t = union {
        struct {
            uint8_t numLock: 1;
            uint8_t capsLock: 1;
            uint8_t scrollLock: 1;
            uint8_t compose: 1;
            uint8_t kana: 1;
            uint8_t reserved: 3;
        };
        uint8_t byte;
    };
    c64USBKeyboard(USBHost const &) {
        init();
    }

    c64USBKeyboard(USBHost const *) {
        init();
    }

    void attachC64(void (*keyboardmatrix)(void *keys));//FB
    void LEDS(uint8_t leds);

    uint8_t LEDS() const {
        return leds_.byte;
    }

    void updateLEDS(void);

    bool numLock() const {
        return leds_.numLock;
    }

    bool capsLock() const {
        return leds_.capsLock;
    }

    bool scrollLock() const {
        return leds_.scrollLock;
    }

    void numLock(bool f);
    void capsLock(bool f);
    void scrollLock(bool f);

protected:
    bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len) final;
    void control(const Transfer_t *transfer) final;
    void disconnect() final;
    static void callback(const Transfer_t *transfer);
    void new_data(const Transfer_t *transfer);
    void init();

private:
    void update();

    Pipe_t *datapipe;
    setup_t setup;
    uint8_t report[8];
    KBDLeds_t leds_ = {0};
    bool update_leds_ = false;
    Pipe_t mypipes[2] __attribute__ ((aligned(32)));
    Transfer_t mytransfers[4] __attribute__ ((aligned(32)));

    void (*keyboardmatrixFunction)(void *keys);
};

#endif // TEENSY64_KEYBOARD_USB_H