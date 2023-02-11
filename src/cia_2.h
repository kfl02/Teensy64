//
// Created by kfl02 on 10.02.23.
//

#pragma once

#ifndef TEENSY64_CIA_2_H
#define TEENSY64_CIA_2_H

#include "cia6526.h"

#define CIA2_PRA_VIC_BANK_MASK  0x03
#define CIA2_PRA_TXD            0x04
#define CIA2_PRA_IEC_ATN_OUT    0x08
#define CIA2_PRA_IEC_CLK_OUT    0x10
#define CIA2_PRA_IEC_DATA_OUT   0x20
#define CIA2_PRA_IEC_CLK_IN     0x40
#define CIA2_PRA_IEC_DATA_IN    0x80
#define CIA2_PRA_IEC_OUT_MASK   (CIA2_PRA_IEC_ATN_OUT | CIA2_PRA_IEC_CLK_OUT | CIA2_PRA_IEC_DATA_OUT)
#define CIA2_PRA_IEC_IN_MASK    (CIA2_PRA_IEC_CLK_IN | CIA2_PRA_IEC_DATA_IN)

class cia_2 : public CIA6526 {
    void write(uint32_t address, uint8_t value) final;
    uint8_t read(uint32_t address) final;
};

#endif //TEENSY64_CIA_2_H
