//
//  Copyright (c) 2026 Cristian Dinca (icmd.tech)
//
//  Licensed Apache 2.0.
//
#include "pico/stdlib.h"

#include "hardware/pio.h"
#include "hardware/clocks.h"

#include "tusb.h"
#include "bsp/board_api.h"

#include "usb_descriptors.h"
#include "adb_txrx.pio.h"

static inline PIO adb_pio0(void) {
    return pio0;
}