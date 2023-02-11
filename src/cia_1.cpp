//
// Created by kfl02 on 10.02.23.
//

#include "cia_1.h"
#include "keyboard.h"

uint8_t cia_1::read(uint32_t address) {
    uint8_t ret;

    address &= 0x0F;

    // TODO: move this away into keyboard code
    switch(address) {
        case CIA_PRA:
            ret = cia1PORTA();
            break;

        case CIA_PRB:
            ret = cia1PORTB();
            break;

        default:
            ret = CIA6526::read(address);
            break;
    }

    return ret;
}
