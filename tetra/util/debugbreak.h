/* Copyright (c) 2011-2021, Scott Tsai
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Originally sourced at commit: #5dcbe41d2bd4712c8014aa7e843723ad7b40fd74
 * https://github.com/scottt/debugbreak/blob/5dcbe41d2bd4712c8014aa7e843723ad7b40fd74/debugbreak.h
 *
 * Modified to be purely macro based by Ian Hangartner on 2026-03-01
 */
#ifndef DEBUG_BREAK_H
#define DEBUG_BREAK_H

#ifdef _MSC_VER

#define debug_break __debugbreak

#else

#define DEBUG_BREAK_ASM(I)   \
    do                       \
    {                        \
        __asm__ volatile(I); \
    } while (0)

#if defined(__i386__) || defined(__x86_64__)
#define debug_break() DEBUG_BREAK_ASM("int $0x03")
#elif defined(__thumb__)
/* FIXME: handle __THUMB_INTERWORK__ */
#define debug_break() DEBUG_BREAK_ASM
/* See 'arm-linux-tdep.c' in GDB source.
 * Both instruction sequences below work. */
#if 1
/* 'eabi_linux_thumb_le_breakpoint' */
#define debug_break() DEBUG_BREAK_ASM(".inst 0xde01")
#else
/* 'eabi_linux_thumb2_le_breakpoint' */
#define debug_break() DEBUG_BREAK_ASM(".inst.w 0xf7f0a000")
#endif

/* Known problem:
 * After a breakpoint hit, can't 'stepi', 'step', or 'continue' in GDB.
 * 'step' would keep getting stuck on the same instruction.
 *
 * Workaround: use the new GDB commands 'debugbreak-step' and
 * 'debugbreak-continue' that become available
 * after you source the script from GDB:
 *
 * $ gdb -x debugbreak-gdb.py <... USUAL ARGUMENTS ...>
 *
 * 'debugbreak-step' would jump over the breakpoint instruction with
 * roughly equivalent of:
 * (gdb) set $instruction_len = 2
 * (gdb) tbreak *($pc + $instruction_len)
 * (gdb) jump   *($pc + $instruction_len)
 */
#elif defined(__arm__) && !defined(__thumb__)
#define debug_break() DEBUG_BREAK_ASM
/* See 'arm-linux-tdep.c' in GDB source,
 * 'eabi_linux_arm_le_breakpoint' */
#define debug_break() DEBUG_BREAK_ASM(".inst 0xe7f001f0")
/* Known problem:
 * Same problem and workaround as Thumb mode */
#elif defined(__aarch64__) && defined(__APPLE__)
#define debug_break __builtin_debugtrap
#elif defined(__aarch64__)
#define debug_break() DEBUG_BREAK_ASM
/* See 'aarch64-tdep.c' in GDB source,
 * 'aarch64_default_breakpoint' */
#define debug_break() DEBUG_BREAK_ASM(".inst 0xd4200000")
#elif defined(__powerpc__)
/* PPC 32 or 64-bit, big or little endian */
#define debug_break() DEBUG_BREAK_ASM
/* See 'rs6000-tdep.c' in GDB source,
 * 'rs6000_breakpoint' */
#define debug_break() DEBUG_BREAK_ASM(".4byte 0x7d821008")

/* Known problem:
 * After a breakpoint hit, can't 'stepi', 'step', or 'continue' in GDB.
 * 'step' stuck on the same instruction ("twge r2,r2").
 *
 * The workaround is the same as ARM Thumb mode: use debugbreak-gdb.py
 * or manually jump over the instruction. */
#elif defined(__riscv)
/* RISC-V 32 or 64-bit, whether the "C" extension
 * for compressed, 16-bit instructions are supported or not */
#define debug_break() DEBUG_BREAK_ASM
/* See 'riscv-tdep.c' in GDB source,
 * 'riscv_sw_breakpoint_from_kind' */
#define debug_break() DEBUG_BREAK_ASM(".4byte 0x00100073")
#else
#include <signal.h>
#define debug_break()  \
    do                 \
    {                  \
        raise(SIGTRAP) \
    } while (0)
#endif

#endif /* ifdef _MSC_VER */

#endif /* ifndef DEBUG_BREAK_H */
