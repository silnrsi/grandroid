/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _ARM_H_
#define _ARM_H_

#define ARM_INSTR_IS_2WORD(x)   (((*(x) & 0xE000) == 0xE000) && (*(x) & 0x1800))
#define ARM_INSTR_IS_JUMP(x)    (((*(x) & 0xF000) == 0xD000) || ((*(x) & 0xF800) == 0xE000) || \
                                 ((*(x) & 0xF8C0) == 0xF080) || ((*(x) & 0xF500) == 0xB100) || \
                                 (((*(x) & 0xFFF0) == 0xE8D0) && ((*((x)+1) & 0xFFE0) == 0xF000)))
#define ARM_INSTR_IS_RETURN(x)  (((((*(x) & 0xFFD0) == 0xE890) || ((*(x) & 0xFFD0) == 0xE910)) && ((*((x)+1) & 0x8000) == 0x8000)) || \
                                 ((((*(x) & 0xFF70) == 0xF850) && ((*((x)+1) & 0xF000) == 0xF000))) || \
                                 ((((*(x) & 0xFFEF) == 0xEA4F) && ((*((x)+1) & 0x7FFF) == 0x0F0E))))
#define ARM_INSTR_IS_CALL(x)    (((*(x) & 0xF800) == 0xF000) && ((*((x)+1) & 0xC000) == 0xC000))

#define ARM_INSTR_NEXT(x)       ((x) + 1 + (ARM_INSTR_IS_2WORD(x) != 0))
#define ARM_INSTR_PREV(x)       ((x) - 1 - (ARM_INSTR_IS_2WORD((x)-2) != 0))

inline bool arm_instr_is_jump(unsigned short *x)
{
    return  (((*x & 0xF000) == 0xD000) || ((*x & 0xF800) == 0xE000) ||
             ((*x & 0xF8C0) == 0xF080) || ((*x & 0xF500) == 0xB100) ||
             (((x[0] & 0xFFF0) == 0xE8D0) && ((x[1] & 0xFFE0) == 0xF000)));
}

inline Elf32_Addr get_calladdr_arm(unsigned short *p)
{
    unsigned v = (p[0] << 16) | p[1];
    unsigned int s = v & 0x04000000;     // could simplify this code assuming s is set
    bool x = (v & 0x1000);
    int l = ((v & 0x7FF) << 1) | ((v & 0x03FF0000) >> 4);
#if defined(NO_BRANCH)
    l = l       | ((~(((s >> 15) | (s >> 13)) ^ v) * 0xC00) & 0x00C00000)   // handle i1, i2
                | ((s >> 2) * 0xFF);                                        // handle sign extending
#else
    if (s)
        l = l | (((v & 0x2800) * 0xC00) & 0x00C00000) | 0xFF000000;
    else
        l = l | (~((v & 0x2800) * 0xC00) & 0x00C00000);
#endif
    if (x)
        return (reinterpret_cast<unsigned>(p) + 4 + l) & 0xFFFFFFFE;
    else
        return (reinterpret_cast<unsigned>(p) + 4 + l) & 0xFFFFFFFC;
}

#endif
