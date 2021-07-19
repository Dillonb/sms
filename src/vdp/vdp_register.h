#ifndef SMS_VDP_REGISTER_H
#define SMS_VDP_REGISTER_H

#include <util/types.h>
#include <util/bitfield.h>

namespace Vdp {
    enum class Mode : u8 {
        M1 = 1 << 0,
        M2 = 1 << 1,
        M3 = 1 << 2,
        M4 = 1 << 3,
    };

    enum class VdpModeControl1 : u8 {
        VScrollLock         = 1 << 7,
        HScrollLock         = 1 << 6,
        MaskCol0            = 1 << 5,
        LineInterruptEnable = 1 << 4,
        ShiftSpritesLeft    = 1 << 3,
        M4                  = 1 << 2,
        M2                  = 1 << 1,
        IsMonochrome        = 1 << 0,

    };

    enum class VdpModeControl2 : u8 {
        EnableDisplay        = 1 << 6,
        FrameInterruptEnable = 1 << 5,
        M1                   = 1 << 4,
        M3                   = 1 << 3,
        TiledSprites         = 1 << 1,
        StretchedSprites     = 1 << 0,
    };

    extern Util::Bitfield<VdpModeControl1> vdpModeControl1;
    extern Util::Bitfield<VdpModeControl2> vdpModeControl2;

    extern Util::Bitfield<Mode> mode;

    extern u8 lc_reload;

    void register_write(u8 reg, u8 value);
}

#endif //SMS_VDP_REGISTER_H
