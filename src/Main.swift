//
//  Copyright (c) 2026 Cristian Dinca (icmd.tech)
//
//  Licensed Apache 2.0.
//

func signed7(_ x: UInt8) -> Int {
    if x & 0b1000000 != 0 {
        return -Int(0b1111111 & ~(x-1))
    } else {
        return Int(x)
    }
}

func scaleMouse(_ d: Int) -> Int {
    let a = d < 0 ? -d : d

    if a <= 1 {
        return d * 3
    }

    if a <= 3 {
        return d * 5
    }

    return d * 7
}

@main
struct Main {
    static func main() {
        let ADB_TX_PIN: uint = 17
        let ADB_RX_PIN: uint = 16

        board_init()

        let rh_init = tusb_rhport_init_t(role: TUSB_ROLE_DEVICE, speed: TUSB_SPEED_FULL)
        let res = withUnsafePointer(to: rh_init) { ptr in
            tud_rhport_init(UInt8(BOARD_TUD_RHPORT), ptr)
        }

        guard res == true else {
            fatalError("tud_rhport_init")
        }

        board_init_after_tusb()

        let pio0 = adb_pio0()!

        let sm = uint(pio_claim_unused_sm(pio0, true));
        let offset = uint(withUnsafePointer(to: adb_txrx_program) { ptr in
            pio_add_program(pio0, ptr);
        })

        adb_txrx_program_init(
            pio0,
            sm,
            offset,
            ADB_TX_PIN,
            ADB_RX_PIN
        );

        var next_poll = get_absolute_time()

        while true {
            tud_task()

            if !time_reached(next_poll) {
                continue;
            }

            next_poll = make_timeout_time_ms(10)

            let poll_mouse_cmd: UInt8 = /* ADDR */ (3 << 4) |
                                        /* TALK */     0x0c |
                                        /* Reg0 */        0 ;

            let request: UInt32 = (UInt32(poll_mouse_cmd) << 24) | (15 << 16);

            pio_interrupt_clear(pio0, uint(0))
            pio_sm_put_blocking(pio0, sm, request)

            while !pio_interrupt_get(pio0, 0) {
                tud_task()
                tight_loop_contents()
            }

            if !pio_sm_is_rx_fifo_empty(pio0, sm) {
                let reg0 = ~UInt16(truncatingIfNeeded: pio_sm_get(pio0, sm))
                let dx = signed7(UInt8(reg0 & 0b1111111))
                let dy = signed7(UInt8((reg0 >> 8) & 0b1111111))

                var buttons: UInt8 = 0

                if (reg0 & 0x8000) == 0 {
                    buttons |= MOUSE_BUTTON_LEFT.rawValue;
                }

                let sx = scaleMouse(dx)
                let sy = scaleMouse(dy)

                if (tud_hid_ready()) {
                    tud_hid_mouse_report(
                        UInt8(REPORT_ID_MOUSE),
                        buttons,      // no buttons
                        Int8(sx),      // X
                        Int8(sy),      // Y
                        0,      // wheel
                        0       // pan
                    );
                }
            }
        }
    }
}