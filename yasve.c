/* yasve.c (2016-10-21) */
/* Copyright (C) 2016-2022 RIKEN R-CCS */
/* SPDX-License-Identifier: LGPL-3.0-or-later */

/* Yet Another ARM SVE Emulator.  It is a trivial SIGILL trap handler
   on ARM SVE instructions.  The operations are direct translations of
   the pseudocode in the instruction-set manual.  The instruction
   table was taken from binutils (opcodes/aarch64-tbl.h), which is
   copyrighted by FSF.  The spinlock code was taken from Linux
   (arch/arm64/include/asm/spinlock.h), which is copyrighted by ARM.
   YASVE is licensed by the LGPL.  */

const char yasve_id[] = "$Id: yasve-v2.1 (2022-02-17) $";

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <ucontext.h>
#include <sys/syscall.h>
#include <arm_neon.h>

#include "yasve.h"
#include "mutex.h"

#define YASVE_MAX_THREADS (128)
#define YASVE_BAD_TID (0)

typedef __uint128_t u128;
typedef __int128_t s128;
typedef unsigned long u64;
typedef signed long s64;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned char u8;
typedef signed char s8;

/* This file includes files "action.c" and "insn.c" in the middle of
   the file.  They are included after sufficient
   declarations/definitions become visible. */

/* MEMO: This library avoids using standard libraries including
   pthread, stdio, malloc, assert, etc., for the use with "runstatic".
   See for the comments in "preloader.c". */

/* MEMO: This works only with little-endian.  The overlapping of
   elements in a vector forces the layout of the structure
   representation (See the zreg type definition). */

/* MEMO: The most interim values are held as s64/u64 in this
   implementation, while they are held with the exact width in the
   pseduocode definition.  It assumes s64/u64 is the largest type.
   Especially, it sign-extends values to 64 bits immediately after
   accessing from memory.  It is because holding values with the exact
   width is tedious in C. */

/* MEMO: PTRUE/PFALSE ignores the PL (predicate-length).  It uses the
   longest 256. */

/* MEMO: The behavior of CheckAlignment() is diffent.  It aborts at
   the call site. */

/* MEMO ON INSN SIMILARITY: (1) INDEX takes a scalar as a base, but
   ADR takes a vector.  (2) CPY takes a scalar, but DUP takes a
   vector.  (3) COMPACT selects and packs active elements and leaves
   zeros.  SPLICE selects consecutive elements and fills the space by
   the elements of another vector.  SEL selects the elements from two
   vectors by the predicates.  EXT extracts a vector from a
   concatenation of two vectors. */

/* Replacement of assert(). */

#define assert(E) \
    ((E) ? (void)(0) : assert_fail(#E, __FILE__, __LINE__, __func__))

/* Replacement of assert(). */

static void
assert_fail(char *e, char *f, int l, const char *h)
{
    bool svr4 = true;
    if (svr4) {
	fprintf(stderr,
		"%s:%d: %s: Assertion '%s' failed.\n", f, l, h, e);
	fflush(0);
    } else {
	fprintf(stderr,
		"Assertion failed: %s, file %s, line %d, function %s\n",
		e, f, l, h);
	fflush(0);
    }
    abort();
}

/* ================================================================ */

/* State of SVE (registers). */

/* See "/usr/include/sys/ucontext.h" for UCONTEXT_T.  See
   "/usr/include/asm/sigcontext.h" for UC_MCONTEXT with "struct
   sigcontext".  UC_MCONTEXT includes: {fault_address; regs[31]; sp;
   pc; pstate; __reserved[4096];}.  Note no value is set in
   cx->UC_MCONTEXT.FAULT_ADDRESS. */

typedef struct {bool n, z, c, v;} bool4;

typedef struct {u64 v; bool f;} nf_value_t;

/* Registers (IT ASSUMES LITTLE-ENDIAN) Z0-Z31: vector registers.
   P0-P15, FFR: predicate registers.  ZCR_EL1, ZCR_EL2, ZCR_EL3:
   control registers. */

/* Z Register Values. (u128 is used to access as a V-register). */

typedef union {
    u128 g[16]; u64 x[32]; u32 w[64]; u16 h[128]; u8 b[256];
} zreg;

/* P Register Values. */

typedef union {
    bool k[256];
} preg;

typedef struct {
    pid_t tid;

    ucontext_t *cx;
    mcontext_t *ux;
    struct fpsimd_context *vx;

    int VL;
    int PL;
    int FPCR;

    zreg z[32];
    preg p[16];
    preg ffr;
    u32 zcr_el1, zcr_el2, zcr_el3;
} svecxt_t;

extern void yasve_perform(svecxt_t *zx, u32 opc);

/* ================================================================ */

/* Installs a SIGILL handler. */

static void __attribute__ ((constructor))
yasve_init()
{
    assert(sizeof(1ULL) == 8 && sizeof(0ULL) == 8);

    if (1) {
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = (SA_SIGINFO|SA_ONSTACK);
	sa.sa_sigaction = yasve_trap;
	int cc = sigaction(SIGILL, &sa, 0);
	if (cc == -1) {
	    fprintf(stderr, "sigaction(SIGILL): %s.\n", strerror(errno));
	    abort();
	}
    }
}

/* Gets task ID.  It is to avoid the pthread library. */

static pid_t
get_tid(void)
{
    long id = syscall(SYS_gettid);
    return (pid_t)id;
}

static int nthreads = 0;
static svecxt_t vcores[YASVE_MAX_THREADS];

static int proc_mem_fd = -1;

static void
init_contexts()
{
    int cc;

    mutex_enter(&mutex);
    mb();

    /*printf("sizeof(long)=%d\n", (int)sizeof(long)); fflush(0);*/

    union {int i; char check;} little_endian = {.i = 1};
    assert(little_endian.check != 0);

    /*pthread_t tid = pthread_self();*/
    pid_t tid = get_tid();
    assert(tid != YASVE_BAD_TID);

    if (nthreads == 0) {
	char *e = getenv("OMP_THREAD_LIMIT");
	if (e != 0) {
	    int count;
	    char gomi[4];
	    cc = sscanf(e, "%d%c", &count, gomi);
	    if (cc == 1 && count > 0) {
		nthreads = count;
	    }
	}
    }

#if 0 /*GOMI*/
    if (nthreads == 0) {
	cpu_set_t cpus;
	cc = pthread_getaffinity_np(tid, sizeof(cpus), &cpus);
	assert(cc == 0);
	u32 count = CPU_COUNT(&cpus);
	if (count > 0) {
	    nthreads = (int)count;
	}
    }
#endif

    nthreads = ((nthreads > 64) ? nthreads : 64);
    assert(0 < nthreads && nthreads <= YASVE_MAX_THREADS);
    memset(vcores, 0, (sizeof(svecxt_t) * (size_t)nthreads));
    for (int i = 0; i < nthreads; i++) {
	vcores[i].tid = YASVE_BAD_TID;
	vcores[i].VL = (8 * 64);
	vcores[i].PL = (8 * 8);
    }

    mb();
    mutex_leave(&mutex);
}

/* NEVER WAS/WILL BE USED. */

#if 0
static void
fin_contexts()
{
    mutex_enter(&mutex);
    mb();
    if (proc_mem_fd != -1) {
	int cc = close(proc_mem_fd);
	if (cc == -1) {
	    fprintf(stderr, "close(%s): %s\n", s, strerror(errno));
	    fflush(0);
	}
	proc_mem_fd = 1;
    }
    mb();
    mutex_leave(&mutex);

}
#endif

static svecxt_t *
get_context()
{
    assert(nthreads > 0);
    /*pthread_t tid = pthread_self();*/
    pid_t tid = get_tid();
    svecxt_t *zx;
    zx = 0;
    for (int i = 0; i < nthreads; i++) {
	if (vcores[i].tid == tid) {
	    zx = &vcores[i];
	    break;
	}
	if (vcores[i].tid == YASVE_BAD_TID) {
	    zx = &vcores[i];
	    zx->tid = tid;
	    break;
	}
    }
    assert(zx != 0);
    return zx;
}

#if 0

static void
dump_preg(svecxt_t *zx, int esize, preg *r, char *msg)
{
    int psize = (esize / 8);
    if (msg != 0) {
	printf("%s", msg);
    }
    printf("<");
    for (int i = 0; i < zx->PL; i += psize) {
	printf("%s%d", ((i == 0) ? "" : ","), r->k[i]);
    }
    printf(">\n");
    fflush(0);
}

static void
dump_zreg(int core, int r, int esize)
{
    assert(0 <= core && core < YASVE_MAX_THREADS);
    assert(0 <= r && r < 32);
    assert(8 <= esize && esize <= 64);
    svecxt_t *zx = &vcores[core];
    printf("<");
    for (int i = 0; i < (zx->VL / 64); i++) {
	printf("%s0x%lx", ((i == 0) ? "" : ","), zs->z[r].x[i]);
    }
    printf(">\n");
    fflush(0);
}

static void
dump_zreg_(svecxt_t *zx, int esize, zreg *r, char *msg)
{
    if (msg != 0) {
	printf("%s", msg);
    }
    printf("<");
    for (int i = 0; i < (zx->VL / 64); i++) {
	printf("%s0x%lx", ((i == 0) ? "" : ","), r->x[i]);
    }
    printf(">\n");
    fflush(0);
}

#endif

bool yasve_dispatch(svecxt_t *zx, u32 opc);

void
yasve_trap(int sig, siginfo_t *si, void *context)
{
    assert(si != 0 && si->si_signo == SIGILL);

    ucontext_t *cx = (ucontext_t *)context;
    mcontext_t *ux = &cx->uc_mcontext;
    struct fpsimd_context *vx = (void *)&(ux->__reserved);
    if (0) {
	printf("sp=0x%016lx pc=0x%016lx pstate=0x%016lx\n",
	       (u64)ux->sp, (u64)ux->pc, (u64)ux->pstate);
	fflush(0);
    }

    if (nthreads == 0) {
	init_contexts();
    }
    assert(nthreads > 0);
    svecxt_t *zx = get_context();
    assert(zx != 0);
    zx->cx = cx;
    zx->ux = ux;
    zx->vx = vx;

    u32 *ip = (u32 *)ux->pc;
    assert(ip == (u32 *)si->si_addr);
    assert(vx->head.magic == FPSIMD_MAGIC);

    if (0) {
	fprintf(stderr, "pc=%p op=0x%08x (tid=%ld)\n", (void *)ip, *ip,
		(zx - vcores));
	fflush(0);
    }

    u32 opc = *ip;
    _Bool sve = yasve_dispatch(zx, opc);

    if (!sve) {
	/* Reset SIGILL to default to cause a true SIGILL, if the
	   instruction is not handled. */

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = SIG_DFL;
	int cc = sigaction(SIGILL, &sa, 0);
	if (cc == -1) {
	    fprintf(stderr, "sigaction(SIGILL): %s.\n", strerror(errno));
	    abort();
	}
    }
}

/* It calls a selected enumlation code in "action.c" by cases in
   "insn.c". */

/* System Control Register, bit A. */

static bool SCTLR_strict_alignment = false;

static const zreg zreg_zeros = {{0}};
static const preg preg_zeros = {{0}};

static const preg preg_all_ones = {
    {1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
     1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
     1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
     1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
     1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
     1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
     1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
     1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1}};

/* Copies in/out the overlapped NEON registers. */

static void
sync_neon_regs(svecxt_t *zx, bool v_to_z)
{
    struct fpsimd_context *vx = zx->vx;
    if (v_to_z) {
	for (int i = 0; i < 32; i++) {
	    zx->z[i].g[0] = vx->vregs[i];
	}
    } else {
	for (int i = 0; i < 32; i++) {
	    vx->vregs[i] = zx->z[i].g[0];
	}
    }
}

/* ================================================================ */

/* Dispatch Table. */

/* OPCODE DECODER.  The operands are extacted as specified by the OPS
   slot of _SVE_INSN.  -- CLASS expands to a "size" argument. */

#define _SVE_INSN(NAME,OPCODE,MASK,CLASS,OP,OPS,QUALS,FLAGS,TIED) \
    else if ((FLAGS) == 0 && ((opc & (MASK)) == (OPCODE))) { \
	yasve_ ## NAME ## _ ## OPCODE \
	    (zx, #NAME, opc, opr_size22, opr_sz22, QUALS, OP, OPS); \
    }

#define _SVE_INSNC(NAME,OPCODE,MASK,CLASS,OP,OPS,QUALS,FLAGS,CONSTRAINT,TIED) \
    else if ((FLAGS) == 0 && ((opc & (MASK)) == (OPCODE))) { \
	yasve_ ## NAME ## _ ## OPCODE \
	    (zx, #NAME, opc, opr_size22, opr_sz22, QUALS, OP, OPS); \
    }

/* "CLASS"-SLOT.  A class-slot value is passed as the "size" argument
   (before operand arguments) to action functions.  Decoding the
   operand for "sve_limm" is done by SVE_LIMM in OPS.  Similally,
   "sve_shift_unpred" by SVE_SHLIMM_UNPRED, "sve_shift_pred" by
   SVE_SHRIMM_PRED, "sve_index" by SVE_Zn_INDEX.  Note that
   "sve_size_sd" refers to both sz22 or size22 operands, depending on
   the instructions. */

#define sve_cpy 0
#define sve_index 0
#define sve_limm 0
#define sve_misc 0
#define sve_movprfx opr_size22
#define sve_pred_zm 0
#define sve_shift_pred 0
#define sve_shift_unpred 0
#define sve_size_bhs opr_size22
#define sve_size_bhsd opr_size22
#define sve_size_hsd opr_size22
#define sve_size_sd opr_size22

/* "OP"-SLOT.  These are maybe used only in alias instructions. */

typedef enum {
    OP_NONE = 0,
    OP_MOV_P_P,
    OP_MOV_Z_P_Z,
    OP_MOV_Z_V,
    OP_MOV_Z_Z,
    OP_MOV_Z_Zi,
    OP_MOVM_P_P_P,
    OP_MOVS_P_P,
    OP_MOVZ_P_P_P,
    OP_MOVZS_P_P_P,
    OP_NOT_P_P_P_Z,
    OP_NOTS_P_P_P_Z
} sveop_t;

/* "QUALS"-SLOT.  QUALS is passed as a part of arguments but is not
   used. */

typedef enum {
    /*LDs*/
    OP_SVE_BZU,
    OP_SVE_HZU,
    OP_SVE_SZU,
    OP_SVE_DZU,
    OP_SVE_SZS,
    OP_SVE_DZD,

    /*STs*/
    OP_SVE_BUU,
    OP_SVE_DUD,
    OP_SVE_DUU,
    OP_SVE_HUU,
    OP_SVE_SUS,
    OP_SVE_SUU,

    OP_SVE_B,
    OP_SVE_BB,
    OP_SVE_BBBU,
    OP_SVE_BMB,
    OP_SVE_BPB,
    OP_SVE_BUB,
    OP_SVE_BUBB,
    OP_SVE_BZ,
    OP_SVE_BZB,
    OP_SVE_BZBB,
    OP_SVE_DD,
    OP_SVE_DDD,
    OP_SVE_DMD,
    OP_SVE_DMH,
    OP_SVE_DMS,
    OP_SVE_DU,
    OP_SVE_DUV_BHS,
    OP_SVE_DUV_BHSD,
    OP_SVE_HB,
    OP_SVE_HMD,
    OP_SVE_HMH,
    OP_SVE_HMS,
    OP_SVE_HU,
    OP_SVE_NIL,
    OP_SVE_RR,
    OP_SVE_RURV_BHSD,
    OP_SVE_RUV_BHSD,
    OP_SVE_SMD,
    OP_SVE_SMH,
    OP_SVE_SMS,
    OP_SVE_SU,
    OP_SVE_UB,
    OP_SVE_UUD,
    OP_SVE_UUS,
    OP_SVE_V_HSD,
    OP_SVE_VM_HSD,
    OP_SVE_VMR_BHSD,
    OP_SVE_VMU_HSD,
    OP_SVE_VMV_BHSD,
    OP_SVE_VMV_HSD,
    OP_SVE_VMV_SD,
    OP_SVE_VMVD_BHS,
    OP_SVE_VMVU_BHSD,
    OP_SVE_VMVU_HSD,
    OP_SVE_VMVV_BHSD,
    OP_SVE_VMVV_HSD,
    OP_SVE_VMVV_SD,
    OP_SVE_VMVVU_HSD,
    OP_SVE_VPU_BHSD,
    OP_SVE_VPV_BHSD,
    OP_SVE_VR_BHSD,
    OP_SVE_VRR_BHSD,
    OP_SVE_VRU_BHSD,
    OP_SVE_VU_BHSD,
    OP_SVE_VU_HSD,
    OP_SVE_VUR_BHSD,
    OP_SVE_VUU_BHSD,
    OP_SVE_VUV_BHSD,
    OP_SVE_VUV_HSD,
    OP_SVE_VUV_SD,
    OP_SVE_VUVV_BHSD,
    OP_SVE_VUVV_HSD,
    OP_SVE_VV_BHSD,
    OP_SVE_VV_BHSDQ,
    OP_SVE_VV_HSD,
    OP_SVE_VV_HSD_BHS,
    OP_SVE_VV_SD,
    OP_SVE_VVD_BHS,
    OP_SVE_VVU_BHSD,
    OP_SVE_VVV_BHSD,
    OP_SVE_VVV_D,
    OP_SVE_VVV_D_H,
    OP_SVE_VVV_H,
    OP_SVE_VVV_HSD,
    OP_SVE_VVV_S,
    OP_SVE_VVV_S_B,
    OP_SVE_VVV_SD_BH,
    OP_SVE_VVVU_H,
    OP_SVE_VVVU_HSD,
    OP_SVE_VVVU_S,
    OP_SVE_VWW_BHSD,
    OP_SVE_VXX_BHSD,
    OP_SVE_VZV_HSD,
    OP_SVE_VZVD_BHS,
    OP_SVE_VZVU_BHSD,
    OP_SVE_VZVV_BHSD,
    OP_SVE_VZVV_HSD,
    OP_SVE_WU,
    OP_SVE_WV_BHSD,
    OP_SVE_XU,
    OP_SVE_XUV_BHSD,
    OP_SVE_XV_BHSD,
    OP_SVE_XVW_BHSD,
    OP_SVE_XWU,
    OP_SVE_XXU
} svequ_t;

/* "FLAGS"-SLOT.  F_ALIAS=1 means to skip alias instructions in
   dispatching.  F_PSEUDO always appears with F_ALIAS.  */

/*static const int F_ALIAS = 1;*/
/*static const int F_MISC = 0;*/
static const int F_HAS_ALIAS = 0;
static const int F_OPD1_OPT = 0;
static const int F_OPD2_OPT = 0;
/*static const int F_PSEUDO = 0;*/
static const int F_SCAN = 0;
static int F_DEFAULT(int x) {return 0;}
static int F_OD(int x) {return 0;}

/* Indicators used in address arguments to action functions.  They
   indicate the width of the first operand register (for z-registers),
   and the second operand is either a register or an immediate.  ZSV
   means the width is defined as the operand size. */

typedef enum {
    OPR_Rn_IMM,
    OPR_Rn_Rm_LSL0,
    OPR_Rn_Rm_LSL1,
    OPR_Rn_Rm_LSL2,
    OPR_Rn_Rm_LSL3,
    OPR_Rn_Zm32_XS14_LSL0,
    OPR_Rn_Zm32_XS14_LSL1,
    OPR_Rn_Zm32_XS14_LSL2,
    OPR_Rn_Zm32_XS14_LSL3,
    OPR_Rn_Zm32_XS22_LSL0,
    OPR_Rn_Zm32_XS22_LSL1,
    OPR_Rn_Zm32_XS22_LSL2,
    OPR_Rn_Zm32_XS22_LSL3,
    OPR_Rn_Zm64_XS22_LSL0,
    OPR_Rn_Zm64_XS22_LSL1,
    OPR_Rn_Zm64_XS22_LSL2,
    OPR_Rn_Zm64_XS22_LSL3,
    OPR_ZnSS_IMM,
    OPR_ZnSS_Zm32_MSZ,
    OPR_ZnSS_ZmSS_MSZ_LSL
} svemo_t;

#define opr_sh13(opc) ((opc >> 13) & 0x1)
#define opr_m4(opc) ((opc >> 4) & 0x1)
#define opr_m14(opc) ((opc >> 14) & 0x1)
#define opr_m16(opc) ((opc >> 16) & 0x1)
#define opr_i2(opc) ((opc >> 19) & 0x3)
#define opr_i1(opc) ((opc >> 19) & 0x1)

#include "action.c"

/* "OPS"-SLOT.  OPS-slot defines the operands to the action.  Note
   that the number of operands is increased, when an argument extends
   to multiple entries.  Example cases are SVE_ADDR_XXX and
   SVE_PATTERN_SCALED. */

#define OP0() 0
#define OP1(Pd) Pd
#define OP2(A0, A1) A0, A1
#define OP3(A0, A1, A2) A0, A1, A2
#define OP4(A0, Pg3, A1, Zm_5) A0, Pg3, A1, Zm_5
#define OP5(A0, Pg3, A1, Zm_5, IMM_ROT1) A0, Pg3, A1, Zm_5, IMM_ROT1

/* Operand References in OPS-slot. */

#define Rn /*Rn*/ ((opc >> 5) & 0x1f)
#define Rm /*Rm*/ ((opc >> 16) & 0x1f)
#define Rd /*Rd*/ ((opc >> 0) & 0x1f)

#define FPIMM0 (0)
#define SIMM5 (opr_simm5_at16)
#define Rn_SP /*Rn*/ ((opc >> 5) & 0x1f)
#define Rd_SP /*Rd*/ ((opc >> 0) & 0x1f)

/* An address operand is 6-tuples.  It is:
   (reg_width_reg_or_imm_marker, Reg, Reg/Imm, sign-extension, LSL).
   reg_width indicates the width of the next register.
   reg_or_imm_marker indicates the next operand is Reg or Imm. */

#define SVE_ADDR_RI_S4x16   OPR_Rn_IMM, Rn, opr_simm4, 0, 0
#define SVE_ADDR_RI_S4x2xVL SVE_ADDR_RI_S4xVL /*ld2/st2*/
#define SVE_ADDR_RI_S4x3xVL SVE_ADDR_RI_S4xVL /*ld3/st3*/
#define SVE_ADDR_RI_S4x4xVL SVE_ADDR_RI_S4xVL /*ld4/st4*/
#define SVE_ADDR_RI_S4xVL   OPR_Rn_IMM, Rn, opr_simm4, 0, 0
#define SVE_ADDR_RI_S6xVL   OPR_Rn_IMM, Rn, opr_simm6_at16, 0, 0
#define SVE_ADDR_RI_S9xVL   OPR_Rn_IMM, Rn, opr_simm9, 0, 0
#define SVE_ADDR_RI_U6      OPR_Rn_IMM, Rn, opr_uimm6, 0, 0
#define SVE_ADDR_RI_U6x2    SVE_ADDR_RI_U6
#define SVE_ADDR_RI_U6x4    SVE_ADDR_RI_U6
#define SVE_ADDR_RI_U6x8    SVE_ADDR_RI_U6
#define SVE_ADDR_RR         OPR_Rn_Rm_LSL0, Rn, Rm, 0, 0
#define SVE_ADDR_RR_LSL1    OPR_Rn_Rm_LSL1, Rn, Rm, 0, 1
#define SVE_ADDR_RR_LSL2    OPR_Rn_Rm_LSL2, Rn, Rm, 0, 2
#define SVE_ADDR_RR_LSL3    OPR_Rn_Rm_LSL3, Rn, Rm, 0, 3
#define SVE_ADDR_RX         OPR_Rn_Rm_LSL0, Rn, Rm, 0, 0
#define SVE_ADDR_RX_LSL1    OPR_Rn_Rm_LSL1, Rn, Rm, 0, 1
#define SVE_ADDR_RX_LSL2    OPR_Rn_Rm_LSL2, Rn, Rm, 0, 2
#define SVE_ADDR_RX_LSL3    OPR_Rn_Rm_LSL3, Rn, Rm, 0, 3
#define SVE_ADDR_RZ         OPR_Rn_Zm64_XS22_LSL0, Rn, SVE_Zm_16, opr_xs22, 0
#define SVE_ADDR_RZ_LSL1    OPR_Rn_Zm64_XS22_LSL1, Rn, SVE_Zm_16, opr_xs22, 1
#define SVE_ADDR_RZ_LSL2    OPR_Rn_Zm64_XS22_LSL2, Rn, SVE_Zm_16, opr_xs22, 2
#define SVE_ADDR_RZ_LSL3    OPR_Rn_Zm64_XS22_LSL3, Rn, SVE_Zm_16, opr_xs22, 3
#define SVE_ADDR_RZ_XTW1_14 OPR_Rn_Zm32_XS14_LSL1, Rn, SVE_Zm_16, opr_xs14, 1
#define SVE_ADDR_RZ_XTW1_22 OPR_Rn_Zm32_XS22_LSL1, Rn, SVE_Zm_16, opr_xs22, 1
#define SVE_ADDR_RZ_XTW2_14 OPR_Rn_Zm32_XS14_LSL2, Rn, SVE_Zm_16, opr_xs14, 2
#define SVE_ADDR_RZ_XTW2_22 OPR_Rn_Zm32_XS22_LSL2, Rn, SVE_Zm_16, opr_xs22, 2
#define SVE_ADDR_RZ_XTW3_14 OPR_Rn_Zm32_XS14_LSL3, Rn, SVE_Zm_16, opr_xs14, 3
#define SVE_ADDR_RZ_XTW3_22 OPR_Rn_Zm32_XS22_LSL3, Rn, SVE_Zm_16, opr_xs22, 3
#define SVE_ADDR_RZ_XTW_14  OPR_Rn_Zm32_XS14_LSL0, Rn, SVE_Zm_16, opr_xs14, 0
#define SVE_ADDR_RZ_XTW_22  OPR_Rn_Zm32_XS22_LSL0, Rn, SVE_Zm_16, opr_xs22, 0
#define SVE_ADDR_ZI_U5      OPR_ZnSS_IMM, SVE_Zn, opr_uimm5, 0, 0
#define SVE_ADDR_ZI_U5x2    SVE_ADDR_ZI_U5
#define SVE_ADDR_ZI_U5x4    SVE_ADDR_ZI_U5
#define SVE_ADDR_ZI_U5x8    SVE_ADDR_ZI_U5
#define SVE_ADDR_ZZ_LSL     OPR_ZnSS_ZmSS_MSZ_LSL, SVE_Zn, SVE_Zm_16, opr_msz, opr_sz22
#define SVE_ADDR_ZZ_SXTW    OPR_ZnSS_Zm32_MSZ, SVE_Zn, SVE_Zm_16, opr_msz, 0
#define SVE_ADDR_ZZ_UXTW    OPR_ZnSS_Zm32_MSZ, SVE_Zn, SVE_Zm_16, opr_msz, 0

/* "SVE_ADDR_R" appears in entries with the same opcode as SVE_ADDR_RR
   and SVE_ADDR_RR_LSL1/2/3.  Thus, they are not used in switching the
   instructions. */

#define SVE_ADDR_R          SVE_ADDR_RR

#define SVE_IMM_ROT1        0
#define SVE_IMM_ROT2        0
#define SVE_Zm3_22_INDEX    SVE_Zm_16_3
#define SVE_Zm3_INDEX       SVE_Zm_16_3
#define SVE_Zm4_INDEX       SVE_Zm_16_4

/* YASVE assumes IMM_ROT2 is SVE_IMM_ROT2 which appears in "fcmla". */
#define IMM_ROT2 SVE_IMM_ROT2

#define SVE_AIMM opr_uimm8
#define SVE_ASIMM opr_simm8_lsh8
#define SVE_FPIMM8 opr_uimm8
#define SVE_I1_HALF_ONE ((opc >> 5) & 0x1)
#define SVE_I1_HALF_TWO ((opc >> 5) & 0x1)
#define SVE_I1_ZERO_ONE ((opc >> 5) & 0x1)
#define SVE_INV_LIMM ((opc >> 5) & 0x7ff)
#define SVE_LIMM opr_uimm13
#define SVE_LIMM_MOV opr_uimm13
#define SVE_PATTERN opr_pattern
#define SVE_PATTERN_SCALED opr_pattern, opr_uimm4
#define SVE_PRFOP /*prfop*/ ((opc >> 0) & 0xf)
#define SVE_Pd /*Pd*/ ((opc >> 0) & 0xf)
#define SVE_Pg3 /*Pg*/ ((opc >> 10) & 0x7)
#define SVE_Pg4_5 /*Pg*/ ((opc >> 5) & 0xf)
#define SVE_Pg4_10 /*Pg*/ ((opc >> 10) & 0xf)
#define SVE_Pg4_16 /*Pg*/ ((opc >> 16) & 0xf)
#define SVE_Pm /*Pm*/ ((opc >> 16) & 0xf)
#define SVE_Pn /*Pn*/ ((opc >> 5) & 0xf)
#define SVE_Pt /*Pt*/ ((opc >> 0) & 0xf)
#define SVE_Rm /*Rm*/ ((opc >> 5) & 0x1f)
#define SVE_Rn_SP /*Rn*/ ((opc >> 16) & 0x1f)

#define SVE_SHLIMM_PRED opr_uimm7_tszh22_tszl8_imm5
#define SVE_SHLIMM_UNPRED opr_uimm7_tszh22_tszl19_imm16
#define SVE_SHRIMM_PRED opr_uimm7_tszh22_tszl8_imm5
#define SVE_SHRIMM_UNPRED opr_uimm7_tszh22_tszl19_imm16
#define SVE_SIMM5 opr_simm5_at5
#define SVE_SIMM5B opr_simm5b_at16
#define SVE_SIMM6 opr_simm6_at5
#define SVE_SIMM8 /*SIMM8*/ opr_simm8
#define SVE_UIMM3 /*UIMM3*/ ((opc >> 16) & 0x7)
#define SVE_UIMM7 opr_uimm7
#define SVE_UIMM8 opr_uimm8
#define SVE_UIMM8_53 opr_uimm8_hilo

#define SVE_VZn /*Vn*/ ((opc >> 5) & 0x1f)
#define SVE_Vd /*Vd*/ ((opc >> 0) & 0x1f)
#define SVE_Vm /*Vm*/ ((opc >> 5) & 0x1f)
#define SVE_Vn /*Vn*/ ((opc >> 5) & 0x1f)
#define SVE_Za_5 /*Za*/ ((opc >> 5) & 0x1f)
#define SVE_Za_16 /*Za*/ ((opc >> 16) & 0x1f)
#define SVE_Zd /*Zd*/ ((opc >> 0) & 0x1f)
#define SVE_Zm_5 /*Zm*/ ((opc >> 5) & 0x1f)
#define SVE_Zm_16 /*Zm*/ ((opc >> 16) & 0x1f)
#define SVE_Zm_16_3 /*Zm*/ ((opc >> 16) & 0x7)
#define SVE_Zm_16_4 /*Zm*/ ((opc >> 16) & 0xf)
#define SVE_Zn /*Zn*/ ((opc >> 5) & 0x1f)
#define SVE_Zn_INDEX SVE_Zn, /*INDEX*/ opr_uimm7_index
#define SVE_ZnxN SVE_Zn
#define SVE_Zt /*Zt*/ ((opc >> 0) & 0x1f)
#define SVE_ZtxN SVE_Zt

/* Extra-Operand Accessors. */

#define opr_uimm3 ((opc >> 16) & 0x7)
#define opr_uimm4 ((opc >> 16) & 0xf)
#define opr_simm4 sign_extend_bits(((opc >> 16) & 0xf), 4)
#define opr_uimm5 ((opc >> 16) & 0x1f)
#define opr_simm5_at16 sign_extend_bits(((opc >> 16) & 0x1f), 5)
#define opr_simm5b_at16 sign_extend_bits(((opc >> 16) & 0x1f), 5)
#define opr_simm5_at5 sign_extend_bits(((opc >> 5) & 0x1f), 5)
#define opr_uimm6 ((opc >> 16) & 0x3f)
#define opr_simm6_at16 sign_extend_bits(((opc >> 16) & 0x3f), 6)
#define opr_simm6_at5 sign_extend_bits(((opc >> 5) & 0x3f), 6)
#define opr_uimm7 ((opc >> 14) & 0x7f)

#define opr_size22 ((opc >> 22) & 0x3)
#define opr_sz22 ((opc >> 22) & 0x1)
#define opr_xs22 ((opc >> 22) & 0x1)
#define opr_xs14 ((opc >> 14) & 0x1)

#define opr_bits_immh22_imml20_tsz16 \
    ((((opc >> 22) & 0x3) << 5) \
     | (((opc >> 20) & 0x1) << 4) \
     | ((opc >> 16) & 0xf))
#define opr_uimm7_index (opr_bits_immh22_imml20_tsz16)

#define opr_bits_tszh22_tszl19_imm16 \
    ((((opc >> 22) & 0x3) << 5) | ((opc >> 16) & 0x1f))
#define opr_bits_tszh22_tszl8_imm5 \
    ((((opc >> 22) & 0x3) << 5) | ((opc >> 5) & 0x1f))
#define opr_uimm7_tszh22_tszl19_imm16 opr_bits_tszh22_tszl19_imm16
#define opr_uimm7_tszh22_tszl8_imm5 opr_bits_tszh22_tszl8_imm5

#define opr_uimm8_hilo ((((opc >> 16) & 0x1f) << 3) | ((opc >> 10) & 0x7))
#define opr_uimm8 ((opc >> 5) & 0xff)
#define opr_simm8 sign_extend_bits(((opc >> 5) & 0xff), 8)
#define opr_simm8_lsh8 \
    (((opc >> 13) & 0x1) == 0 ? opr_simm8 : (opr_simm8 << 8))

#define opr_imm9 ((((opc >> 16) & 0x3f) << 3) | ((opc >> 10) & 0x7))
#define opr_simm9 sign_extend_bits(opr_imm9, 6)
#define opr_uimm13 ((opc >> 5) & 0x1fff)

/* (mbytes = 1 << msz_) */
#define opr_msz /*msz*/ ((opc >> 10) & 0x2)

#define opr_pattern /*pattern*/ ((opc >> 5) & 0x1f)

bool
yasve_dispatch(svecxt_t *zx, u32 opc)
{
    _Bool sve = (((opc >> 25) & 0xf) == 2);

    sync_neon_regs(zx, 1);
    if (!sve) {
	fprintf(stderr, "Non SVE insn.\n"); fflush(0);
	abort();
    }
#include "insn.c"
    else {
	fprintf(stderr, "Undefined SVE insn.\n"); fflush(0);
	abort();
    }
    sync_neon_regs(zx, 0);

    if (sve) {
	/* Skip the signaling insn for stepping next. */

	zx->ux->pc += 4;

	/* Do it again if the next insn is also SVE. */

	if (0) {
	    u32 *nip = (u32 *)zx->ux->pc;
	    u32 nopc = *nip;
	    if (sve_insn_p(nopc)) {
		return yasve_dispatch(zx, nopc);
	    }
	}
    }
    return sve;
}
