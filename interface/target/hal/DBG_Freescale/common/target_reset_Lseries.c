/* CMSIS-DAP Interface Firmware
 * Copyright (c) 2009-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "target_reset.h"
#include "target_flash.h"
#include "swd_host.h"

#define MDM_STATUS  0x01000000
#define MDM_CTRL    0x01000004     //
#define MDM_IDR     0x010000fc     // read-only identification register

#define MCU_ID      0x001c0020

void target_before_init_debug(void) {
    swd_set_target_reset(1);
}

void board_init(void) {
}

uint8_t target_unlock_sequence(void) {
    uint32_t val;

    // read the device ID
    if (!swd_read_ap(MDM_IDR, &val)) {
        return 0;
    }
    // verify the result
    if (val != MCU_ID) {
        return 0;
    }

    if (!swd_read_ap(MDM_STATUS, &val)) {
        return 0;
    }

    // flash in secured mode
    if (val & (1 << 2)) {
        // hold the device in reset
        target_set_state(RESET_HOLD);
        // write the mass-erase enable bit
        if (!swd_write_ap(MDM_CTRL, 1)) {
            return 0;
        }
        while (1) {
            // wait until mass erase is started
            if (!swd_read_ap(MDM_STATUS, &val)) {
                return 0;
            }

            if (val & 1) {
                break;
            }
        }
        // mass erase in progress
        while (1) {            
            // keep reading until procedure is complete
            if (!swd_read_ap(MDM_CTRL, &val)) {
                return 0;
            }

            if (val == 0) {
                break;
            }
        }
    }

    return 1;
}

// Check Flash Configuration Field bytes at address 0x400-0x40f to ensure that flash security
// won't be enabled.
//
// FCF bytes:
// [0x0-0x7]=backdoor key
// [0x8-0xb]=flash protection bytes
// [0xc]=FSEC:
//      [7:6]=KEYEN (2'b10 is backdoor key enabled, all others backdoor key disabled)
//      [5:4]=MEEN (2'b10 mass erase disabled, all other mass erase enabled)
//      [3:2]=FSLACC (2'b00 and 2'b11 factory access enabled, 2'b01 and 2'b10 factory access disabled)
//      [1:0]=SEC (2'b10 flash security disabled, all other flash security enabled)
// [0xd]=FOPT
// [0xe]=EEPROM protection bytes (FlexNVM devices only)
// [0xf]=data flash protection bytes (FlexNVM devices only)
//
// This function checks that:
// - 0x0-0xb==0xff
// - 0xe-0xf==0xff
// - FSEC=0xfe
//
// FOPT can be set to any value.
uint8_t security_bits_set(uint32_t addr, uint8_t *data, uint32_t size)
{
    if (addr == 0x400) {
        // make sure we can unsecure the device or dont program at all
        if ((data[0xc] & 0x30) == 0x20) {
            // Dont allow programming mass-erase disabled state
            return 1;
        }
        // Security is OK long as we can mass-erase (comment the following out to enable target security)
        if ((data[0xc] & 0x03) != 0x02) {
            return 1;
        }
    }
    return 0;
}

uint8_t target_set_state(TARGET_RESET_STATE state) {
    return swd_set_target_state(state);
}

