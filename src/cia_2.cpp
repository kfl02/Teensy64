//
// Created by kfl02 on 10.02.23.
//

#include "cia_2.h"
#include "cpu.h"

void cia_2::write(uint32_t address, uint8_t value) {
    address &= 0x0F;

    // TODO: move this away, the VIC part is easy
    switch(address) {
        case CIA_PRA:
            if(~value & CIA2_PRA_IEC_OUT_MASK) {
                cpu_setExactTiming();
            }

            WRITE_ATN_CLK_DATA(value);

            cpu.vic.bank = ((~value) & CIA2_PRA_VIC_BANK_MASK) * 16384;
            tvic::applyAdressChange();

            r.PRA = value;
            break;

        // TODO: Why was this in the original code? Leave it out for now.
        // Original comment ny FB: Fake IRQ
//        case CIA_SDR:
//            CIA6526::write(address, value);
//            cpu_nmi();
//            break;

        default:
            CIA6526::write(address, value);
            break;
    }
}

uint8_t cia_2::read(uint32_t address) {
    uint8_t ret;

    address &= 0x0F;

    switch(address) {
        case CIA_PRA:
            ret = (cpu.cia2.R[CIA_PRA] & ~CIA2_PRA_IEC_IN_MASK) | (uint8_t)READ_CLK_DATA();

            if(~ret & ~CIA2_PRA_IEC_IN_MASK) {
                cpu_setExactTiming();
            }
            break;

        default:
            ret = CIA6526::read(address);
            break;
    }

    return ret;
}
