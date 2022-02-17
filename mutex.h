/* mutex.h (2016-10-21) */
/* Copyright (C) 2016-2022 RIKEN R-CCS */
/* Copyright (C) 2012-2021 Free Software Foundation, Inc. */
/* Copyright (C) 2012 ARM Ltd. */
/* SPDX-License-Identifier: LGPL-3.0-or-later */

/* Spinlock code from Linux (arch/arm64/include/asm/spinlock.h).
   Spinlocks are used to avoid calls to the pthread library.
   Spinlocks are not appropriate because system calls are used inside
   the locks, but they are used for simplicity. */

/* Memory Barrier. */

#define mb() {asm volatile("dsb sy" ::: "memory");}

/* It assumes the "next" field is the upper half of a word (i.e.,
   little-endian). */

typedef union {
    struct {unsigned short owner; unsigned short next;} u;
    unsigned int ui;
} spinlock_t;

static spinlock_t mutex;

/* Define ARM81A if to use LSE (Large System Extension) instructions
   defined in ARMv8.1. */

#undef ARM81A

/* MEMO: asm-specifier "Q" is for a memory address in a register (ARM
   specific).  USED-OPCODES: LD-ADD-A(r0,r1,r3) is "load atomic add
   with aquire": r1:=[r3];[r3]:=r0+[r3] (in v8.1).  ST-ADD-L(r0,r1) is
   "store atomic add with release": [r1]:=r0 (in v8.1).  EOR(r0,r1,r2)
   is r0:=(r1&(r2<<shift)), where shift is one of {LSL, LSR, ASR, ROR}
   (ROR for rotate-right).  LD-AXR(r0,r1) is "load acquire exclusive
   register": r0:=[r1].  PRFM is prefetch.  PSTL1STRM means
   P-ST-L1-STRM; prefetch for store L1 streaming (non-temporal).
   LDAXR is "load acquire exclusive register".  The pair LDAXR-STXR is
   LL/SC. */

static void __attribute__ ((noinline))
mutex_enter(spinlock_t *lock)
#ifdef ARM81A
{
    /* next = lock->next; lock->next++; while (lock->owner != next); */
    asm volatile(
	"//(spin-lock)\n"
	"mov w5, %2\n"
	"ldadda w5, w3, %0\n"
	"eor w4, w3, w3, ror #16\n"
	"cbz w4, 3f\n"
	"sevl\n"
	"2:\n"
	"wfe\n"
	"ldaxrh w5, %1\n"
	"eor w4, w5, w3, lsr #16\n"
	"cbnz w4, 2b\n"
	"3:"

        : "+Q" (*lock)
        : "Q" (lock->u.owner), "I" (1 << 16)
        : "memory");
}
#else
{
    asm volatile(
	"//(spin-lock)\n"
	"prfm pstl1strm, %0\n"
	"1:\n"
	"ldaxr w3, %0\n"
	"add w4, w3, %2\n"
	"stxr w5, w4, %0\n"
	"cbnz w5, 1b\n"
	"eor w4, w3, w3, ror #16\n"
	"cbz w4, 3f\n"
	"sevl\n"
	"2:\n"
	"wfe\n"
	"ldaxrh w5, %1\n"
	"eor w4, w5, w3, lsr #16\n"
	"cbnz w4, 2b\n"
	"3:"

        : "+Q" (*lock)
        : "Q" (lock->u.owner), "I" (1 << 16)
        : "memory");
}
#endif /*ARM81A*/

static void __attribute__ ((noinline))
mutex_leave(spinlock_t *lock)
#ifdef ARM81A
{
    /* lock->owner++; */
    asm volatile(
	"//(spin-unlock)\n"
	"mov w3, #1\n"
	"staddlh w3, %0"

	: "=Q" (lock->u.owner)
	:
	: "memory");
}
#else
{
    asm volatile(
	"//(spin-unlock)\n"
	"ldrh w3, %0\n"
        "add w3, w3, #1\n"
	"stlrh w3, %0"

	: "=Q" (lock->u.owner)
	:
	: "memory");
}
#endif /*ARM81A*/
