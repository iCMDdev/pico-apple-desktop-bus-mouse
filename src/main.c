//
//  Copyright (c) 2026 Cristian Dinca (icmd.tech)
//
//  Licensed Apache 2.0.
//
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"
// #include "pico/stdio_usb.h"
#include "tusb.h"
#include "bsp/board_api.h"
#include "usb_descriptors.h"

#include "adb_txrx.pio.h"

#define ADB_TX_PIN 17
#define ADB_RX_PIN 16

int signed7(uint8_t x) {
    if (x & 0b1000000) {
        // number is negative
        return -(int)(0b1111111 & ~(x-1));
    } else {
        return (int)x;
    }
}

int scale_mouse(int d) {
    int a = d < 0 ? -d : d;

    if (a <= 1)
        return d * 3;
    if (a <= 3)
        return d * 5;

    return d * 7;
}

int main() {
    // stdio_init_all();

    // while (!stdio_usb_connected()) {
    //     sleep_ms(50);
    // }
    board_init();

    const tusb_rhport_init_t rh_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUD_OPT_HIGH_SPEED ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL
    };

    TU_ASSERT(tud_rhport_init(BOARD_TUD_RHPORT, &rh_init));

    board_init_after_tusb();

    // Talk register 3.
    uint8_t command = (3 << 4) | 0x0c | 3;

    // Craft request containing command + the expected 16 bits.
    uint32_t request = ((uint32_t)command << 24) |
                       ((uint32_t)15   << 16);

    uint sm = pio_claim_unused_sm(pio0, true);
    uint offset = pio_add_program(pio0, &adb_txrx_program);

    adb_txrx_program_init(
        pio0,
        sm,
        offset,
        ADB_TX_PIN,
        ADB_RX_PIN
    );

    // // Read TALK register 3
    // pio_sm_put_blocking(pio0, sm, request);
    // uint16_t raw = (uint16_t)pio_sm_get_blocking(pio0, sm);
    // uint16_t reg3 = ~raw;

    // // printf("received: %x\n", reg3);

    // // addr bits are 11,10,9,8; mask them
    // uint16_t addr = (0b111100000000 & reg3) >> 8;
    // static long long x = 0, y = 0;

    absolute_time_t next_poll = get_absolute_time();

    while (1) {
        tud_task();

        if (!time_reached(next_poll)) {
            continue;
        }

        next_poll = make_timeout_time_ms(10);

        uint8_t poll_mouse_cmd = /* ADDR */ (3 << 4) |
                                 /* TALK */     0x0c |
                                 /* Reg0 */        0 ;
        // Craft request containing command + the expected 16 bits.
        request = ((uint32_t)poll_mouse_cmd << 24) |
                  ((uint32_t)15   << 16);

        // Read TALK register 3
        pio_interrupt_clear(pio0, 0);

        pio_sm_put_blocking(pio0, sm, request);

        while (!pio_interrupt_get(pio0, 0)) {
            tud_task();
            tight_loop_contents();
        }

        if (!pio_sm_is_rx_fifo_empty(pio0, sm)) {
            uint16_t raw = (uint16_t)pio_sm_get(pio0, sm);
            uint16_t reg0 = (uint16_t)~raw;
            int8_t dx = signed7(reg0 & ((uint16_t)0b1111111));
            int8_t dy = signed7((reg0 >> 8) & ((uint16_t)0b1111111));
            // printf("received: %04x\n(%lld, %lld) BTN=%d\n", reg0, x, y, !((reg0 & (1 << 15)) >> 15));

            uint8_t buttons = 0;

            if (!(reg0 & 0x8000)) {
                buttons |= MOUSE_BUTTON_LEFT;
            }
            
            int sx = scale_mouse(dx);
            int sy = scale_mouse(dy);

            if (tud_hid_ready()) {
                tud_hid_mouse_report(
                    REPORT_ID_MOUSE,
                    buttons,      // no buttons
                    sx,      // X
                    sy,      // Y
                    0,      // wheel
                    0       // pan
                );
            }
        }
    }

    return 0;
}

uint16_t tud_hid_get_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t *buffer,
    uint16_t reqlen
) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;
}

void tud_hid_set_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t const *buffer,
    uint16_t bufsize
) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}