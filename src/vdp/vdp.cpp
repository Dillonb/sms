#include <util/log.h>
#include "vdp.h"
#include "vdp_register.h"

namespace Vdp {
    bool ctrl_high = false;
    u8 code;
    u16 address;
    u8 read_buffer;

    u8 vram[0x4000];
    u8 cram[32];

    int cycle_counter = 0;
    int hcounter = 0;
    int vcounter = 0;
    u8 line_counter = 0;

    bool line_interrupt = false;
    bool frame_interrupt = false;

    constexpr int num_scanlines = 262;
    constexpr int fps = 60;
    constexpr int cycles_per_line = 3579545 / num_scanlines / fps;

    constexpr int COMMAND_VRAM_READ = 0;
    constexpr int COMMAND_VRAM_WRITE = 1;
    constexpr int COMMAND_REGISTER_WRITE = 2;
    constexpr int COMMAND_CRAM_WRITE = 3;

    void reset() {
        line_counter = 0xFF;
        cycle_counter = 0;
        hcounter = 0;
        vcounter = 0;
        for (int i = 0; i < 0x4000; i++) {
            vram[i] = 0;
        }
    }

    void process_command() {
        switch (code) {
            case COMMAND_VRAM_READ:
                read_buffer = vram[address];
                logalways("copy 1 byte from vram[address] to the \"read buffer\"");
                address++;
                break;
            case COMMAND_VRAM_WRITE: // Handled in the write_data() function below
                break;
            case COMMAND_REGISTER_WRITE:
                register_write((address >> 8) & 0xF, address & 0xFF);
                break;
            case COMMAND_CRAM_WRITE: // Handled in the write_data() function below.
                break;
            default:
                logfatal("Processing command %d", code);
        }
    }

    void write_control(u8 value) {
        if (ctrl_high) {
            address &= 0x00FF;
            address |= ((value << 8) & 0x3F00);
            code = (value >> 6) & 0x3;
            process_command();
        } else {
            address &= 0xFF00;
            address |= value;
        }
        ctrl_high = !ctrl_high;
    }

    void write_data(u8 value) {
        read_buffer = value;
        ctrl_high = false;
        switch (code) {
            case COMMAND_REGISTER_WRITE:
            case COMMAND_VRAM_WRITE:
                vram[address] = value;
                address = (address + 1) & 0x3FFF;
                break;
            case COMMAND_CRAM_WRITE:
                cram[address] = value & 0x3F;
                address = (address + 1) & 0x3FFF;
                break;
            default:
                logfatal("Write to VDP data port with code: %d address %04X", code, address);
        }
    }

    void scanline() {
        switch (mode.raw) {
            case 0b1010:
                printf("Mode 4.\n");
                break;
            case 0b1011:
                // TODO does this happen at 224 or 225?
                if (vcounter == 225 && vdpModeControl1[VdpModeControl2::FrameInterruptEnable]) {
                    frame_interrupt = true;
                }
                break;
            default:
                logfatal("Unknown mode: %d%d%d%d", mode[Mode::M4], mode[Mode::M3], mode[Mode::M2], mode[Mode::M1]);
        }
        if (vcounter <= 192) {
            if (--line_counter == 0xFF) {
                logfatal("Line counter underflow.");
            }
        } else {
            line_counter = lc_reload;
        }

        vcounter = (vcounter + 1) % num_scanlines;
    }

    void step(int cycles) {
        cycle_counter += cycles;
        if (cycle_counter >= cycles_per_line) {
            cycle_counter -= cycles_per_line;
            scanline();
        }
    }

    bool interrupt_pending() {
        return (frame_interrupt && vdpModeControl2[VdpModeControl2::FrameInterruptEnable]) || (line_interrupt && vdpModeControl1[VdpModeControl1::LineInterruptEnable]);
    }

    u8 get_status() {
        u8 val = 0;
        val |= (frame_interrupt << 7);
        val |= (0 << 6); // sprite overflow
        val |= (0 << 5); // sprite collision
        val |= 0b1111;


        frame_interrupt = false;
        line_interrupt = false;

        return val;
    }
}
