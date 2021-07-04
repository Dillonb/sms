#ifndef SMS_INSTRUCTIONS_H
#define SMS_INSTRUCTIONS_H

namespace Z80 {
    typedef int (*instruction)();
    extern const instruction instructions[0x100];
    extern const instruction ed_instructions[0xC0];
}

#endif //SMS_INSTRUCTIONS_H
