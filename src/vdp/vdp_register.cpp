#include <util/log.h>
#include "vdp_register.h"

namespace Vdp {
    Util::Bitfield<VdpModeControl1> vdpModeControl1;
    Util::Bitfield<VdpModeControl2> vdpModeControl2;

    Util::Bitfield<Mode> mode;

    u8 overscan_bg_color;
    // Register 8
    u8 bg_x_scroll;
    // Register 9
    u8 bg_y_scroll;
    // Register A
    u8 lc_reload = 0xFF;

    void register_write(u8 reg, u8 value) {
        switch (reg) {
            case 0:
                vdpModeControl1.raw = value;
                mode(Mode::M2) = vdpModeControl1[VdpModeControl1::M2];
                mode(Mode::M4) = vdpModeControl1[VdpModeControl1::M4];
                break;
            case 1:
                vdpModeControl2.raw = value;
                mode(Mode::M1) = vdpModeControl1[VdpModeControl2::M1];
                mode(Mode::M3) = vdpModeControl1[VdpModeControl2::M3];
                break;
            case 2:
                if (value != 0xFF) {
                    logfatal("Wrote a non-0xFF value to VDP reg 2");
                }
                break;
            case 3:
                if (value != 0xFF) {
                    logfatal("Wrote a non-0xFF value to VDP reg 3");
                }
                break;
            case 4:
                if (value != 0xFF) {
                    logfatal("Wrote a non-0xFF value to VDP reg 4");
                }
                break;
            case 5:
                if (value != 0xFF) {
                    logfatal("Wrote a non-0xFF value to VDP reg 5");
                }
                break;
            case 6:
                logwarn("Sprite pattern generator table base address: %02X", value);
                break;
            case 7:
                overscan_bg_color = value & 0xF;
                break;
            case 8:
                bg_x_scroll = value;
                break;
            case 9:
                bg_y_scroll = value;
                break;
            case 0xA:
                lc_reload = value;
                break;
            default:
                logfatal("Write %02X to reg %X", value, reg);
        }
    }
}