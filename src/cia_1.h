//
// Created by kfl02 on 10.02.23.
//

#pragma once

#ifndef TEENSY64_CIA_1_H
#define TEENSY64_CIA_1_H

#include "cia6526.h"

class cia_1 : public CIA6526 {
    uint8_t read(uint32_t address) final;
};


#endif //TEENSY64_CIA_1_H
