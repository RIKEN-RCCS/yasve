/* action.c (2016-10-21) */
/* Copyright (C) 2016-2022 RIKEN R-CCS */
/* Copyright (C) 2012-2021 Free Software Foundation, Inc. */
/* Copyright (C) 2012 ARM Ltd. */
/* SPDX-License-Identifier: LGPL-3.0-or-later */

/* Instruction Definitions.  Entry functions have names yasve_ + name
   + opcode.  This file is included from "dispatch.c". */

/* NOTTESTED is a marker which means that action is never tested.  The
   marker is removed when it is run at least once anyhow.  It is set
   to assert(0) for testing, or to assert(1) for releases. */

#define NOTTESTED() assert(1)
#define NOTTESTED00() assert(1)
#define NOTTESTED01() assert(1)
#define NOTTESTED02() assert(1)

/* CTXARG is a fixed part of arguments to an action function.  OPC is
   the actual word of the code (including operands).  See the
   definitions of _SVE_INSN/_SVE_INSNC in "dispatch.c". */

#define CTXARG svecxt_t *zx, char *name, u32 opc, int size, int sz, svequ_t _qu, sveop_t _op
#define TBDARG CTXARG, ...
#define TBD(X) {assert(0 && "TBD: " X);}

/* ================================================================ */

/* A marker for sign-extension.  Note that TRUE is for UNSIGNED, to
   match the uses in the pseudocode.  DONTCARE for don't-care. */

typedef enum signedness {S64EXT = 0, U64EXT = 1, DONTCARE = 1} signedness_t;

/* Re-interprets the representation as a different type. */

static double
u64_as_double(u64 v)
{
    union {u64 u; double d; float f;} u;
    u.u = v;
    return u.d;
}

static float
u64_as_float(u64 v)
{
    union {u64 u; double d; float f;} u;
    u.u = v;
    return u.f;
}

static u64
double_as_u64(double v)
{
    union {u64 u; double d; float f;} u = {.u = 0ULL};
    u.d = v;
    return u.u;
}

static u64
float_as_u64(float v)
{
    union {u64 u; double d; float f;} u = {.u = 0ULL};
    u.f = v;
    return u.u;
}

static u64
fp16_as_u64(double v)
{
    assert(v == 0.0);
    union {u64 u; double d; float f;} u = {.u = 0ULL};
    u.d = v;
    return u.u;
}

static s64
sign_extend_bits(u64 u, int bits)
{
    assert(bits <= 64);
    s64 s = (s64)u;
    int shift = (64 - bits);
    return ((s << shift) >> shift);
}

static u64
sign_extend(u64 v, int esize, bool signedp)
{
    union regv_bhsd {
	u64 ud;
	u32 us;
	u16 uh;
	u8 ub;
	s64 sd;
	s32 ss;
	s16 sh;
	s8 sb;
    };
    union regv_bhsd x = {.ud = v};
    if (signedp == S64EXT) {
	switch (esize) {
	case 8:
	    return (u64)x.sb;
	case 16:
	    return (u64)x.sh;
	case 32:
	    return (u64)x.ss;
	case 64:
	    return (u64)x.sd;
	default:
	    assert(esize == 8 || esize == 16 || esize == 32 || esize == 64);
	    abort();
	}
    } else {
	switch (esize) {
	case 8:
	    return (u64)x.ub;
	case 16:
	    return (u64)x.uh;
	case 32:
	    return (u64)x.us;
	case 64:
	    return (u64)x.ud;
	default:
	    assert(esize == 8 || esize == 16 || esize == 32 || esize == 64);
	    abort();
	}
    }
}

/* Conversions between types. */

static u64 s8_to_u64(s8 v) {return (u64)(s64)v;}
static u64 s16_to_u64(s16 v) {return (u64)(s64)v;}
static u64 s32_to_u64(s32 v) {return (u64)(s64)v;}
static u64 s64_to_u64(s64 v) {return (u64)(s64)v;}

static u64 u8_to_u64(u8 v) {return (u64)v;}
static u64 u16_to_u64(u16 v) {return (u64)v;}
static u64 u32_to_u64(u32 v) {return (u64)v;}
static u64 u64_to_u64(u64 v) {return (u64)v;}

static float double_to_float(double v) {return (float)v;}
static double float_to_double(float v) {return (double)v;}

static u64 float_to_s32(float v) {return s32_to_u64((s32)v);}
static u64 float_to_u32(float v) {return u32_to_u64((u32)v);}
static u64 float_to_s64(float v) {return s64_to_u64((s64)v);}
static u64 float_to_u64(float v) {return u64_to_u64((u64)v);}

static u64 double_to_s32(double v) {return s32_to_u64((s32)v);}
static u64 double_to_u32(double v) {return u32_to_u64((u32)v);}
static u64 double_to_s64(double v) {return s64_to_u64((s64)v);}
static u64 double_to_u64(double v) {return u64_to_u64((u64)v);}

static float s32_to_float(s32 v) {return (float)v;}
static float u32_to_float(u32 v) {return (float)v;}
static float s64_to_float(s64 v) {return (float)v;}
static float u64_to_float(u64 v) {return (float)v;}

static double s32_to_double(s32 v) {return (double)v;}
static double u32_to_double(u32 v) {return (double)v;}
static double s64_to_double(s64 v) {return (double)v;}
static double u64_to_double(u64 v) {return (double)v;}

static bool
sve_insn_p(u32 opc)
{
    bool print = false;
    bool sve = (((opc >> 25) & 0xf) == 2);
    if (!sve) {
	return false;
    } else {
	u32 opc2 = ((((opc >> 29) & 0x7) << 1)| ((opc >> 24) & 0x1));
	char *d;
	switch (opc2) {
	case 0: d = "(int dp)"; break;
	case 1: d = "(perm)"; break;
	case 2: d = "(int cmp)"; break;
	case 3: d = "(pred)"; break;
	case 7: d = "(fp dp & cmp)"; break;
	case 8: case 9: d = "(gather 32)"; break;
	case 0xa: case 0xb: d = "(ld/st)"; break;
	case 0xc: case 0xd: d = "(gather 64)"; break;
	case 0xe: case 0xf: d = "(scatter 32/64)"; break;
	default: d = 0; break;
	}
	if (print) {
	    if (d != 0) {
		fprintf(stderr, "op=0x%08x 0x%x %s\n", opc, opc2, d);
		fflush(0);
	    } else {
		fprintf(stderr, "op=0x%08x 0x%x (?)\n", opc, opc2);
		fflush(0);
	    }
	}
	return (d != 0);
    }
}

/* ================================================================ */

/* FUNCTIONS IN OPERATION DESCRIPTION.  See the section "Shared
   Pseudocode Functions".  COMMON VOCABULARY: Getter/setter functions
   are suffixed by _get and _set.  Exceptionaly, the suffix _rd and
   _wr are used for the memory access "Mem".  Datatype of "integer" is
   mostly u64.  Small integers are int.  "bit" is bool. */

typedef enum {
    Cmp_GE, Cmp_LT, Cmp_GT, Cmp_LE, Cmp_EQ, Cmp_NE, Cmp_UN
} SVECmp;

/* ReduceOp IS NOT USED. */

#if 0
typedef enum {
    ReduceOp_FMINNUM, ReduceOp_FMAXNUM, ReduceOp_FMIN, ReduceOp_FMAX,
    ReduceOp_FADD, ReduceOp_ADD
} ReduceOp;
#endif

typedef enum {
    AccType_NORMAL,
    AccType_VEC,
    AccType_STREAM,
    AccType_VECSTREAM,
    AccType_ATOMIC,
    AccType_ATOMICRW,
    AccType_ORDERED,
    AccType_ORDEREDRW,
    AccType_LIMITEDORDERED,
    AccType_UNPRIV,
    AccType_IFETCH,
    AccType_PTW,
    AccType_NONFAULT,
    AccType_CNOTFIRST,
    AccType_DC,
    AccType_IC,
    AccType_DCZVA,
    AccType_AT
} AccType;

/* Checks if the argument is a power of two. */

static inline bool
powerof2p(int x)
{
    return ((x > 0) && ((x & (x - 1)) == 0));
}

/* Aligns an address down to a power of two.  Alignment R should be a
   power of two. */

static u64
align_down(u64 a, int r)
{
    assert(powerof2p(r));
    u64 mask = (u64)(r - 1);
    return (a & ~mask);
}

/*#define SInt(V) ((s64)(V))*/
/*#define UInt(V) ((u64)(V))*/

static void CheckSVEEnabled() {/*nop*/}

static void CheckSPAlignment() {/*nop*/}

static void UnallocatedEncoding() {abort();}

static void ReservedValue() {abort();}

/* Checks address alignment and aborts.  It ignores access types of
   atomic, ordered etc. */

static void
CheckAlignment(u64 address, int alignment, AccType ac)
{
    assert(ac == AccType_NORMAL);
    u64 x = align_down(address, alignment);
    assert(!SCTLR_strict_alignment || x == address);
}

static u64
Mem_rd(svecxt_t *zx, u64 addr, int mbytes, enum signedness signedp, AccType ac)
{
    assert(ac == AccType_NORMAL);
    u64 v;
    if (signedp == S64EXT) {
	switch (mbytes) {
	case 1:
	    v = s8_to_u64(*(s8 *)addr); break;
	case 2:
	    v = s16_to_u64(*(s16 *)addr); break;
	case 4:
	    v = s32_to_u64(*(s32 *)addr); break;
	case 8:
	    v = s64_to_u64(*(s64 *)addr); break;
	default:
	    assert(mbytes == 1 || mbytes == 2 || mbytes == 4 || mbytes == 8);
	    abort();
	}
    } else {
	switch (mbytes) {
	case 1:
	    v = u8_to_u64(*(u8 *)addr); break;
	case 2:
	    v = u16_to_u64(*(u16 *)addr); break;
	case 4:
	    v = u32_to_u64(*(u32 *)addr); break;
	case 8:
	    v = u64_to_u64(*(u64 *)addr); break;
	default:
	    assert(mbytes == 1 || mbytes == 2 || mbytes == 4 || mbytes == 8);
	    abort();
	}
    }
    //fprintf(stderr, "rd MEM[%p:%d]=0x%016lx\n", (void *)addr, msize, v);
    //fflush(0);
    return v;
}

static void
Mem_wr(svecxt_t *zx, u64 addr, int mbytes, u64 data, AccType ac)
{
    assert(ac == AccType_NORMAL);
    //fprintf(stderr, "wr MEM[%p:%d]=0x%016lx\n", (void *)addr, mbytes, data);
    //fflush(0);
    switch (mbytes) {
    case 1:
	*(u8 *)addr = (u8)data;
	break;
    case 2:
	*(u16 *)addr = (u16)data;
	break;
    case 4:
	*(u32 *)addr = (u32)data;
	break;
    case 8:
	*(u64 *)addr = (u64)data;
	break;
    default:
	assert(mbytes == 1 || mbytes == 2 || mbytes == 4 || mbytes == 8);
	abort();
    }
}

/* Reads memory without faults.  It assumes normal accesses and omits
   checking device memory, etc. */

static nf_value_t
MemNF_rd(svecxt_t *zx, u64 address, int size, bool signedp, AccType ac)
{
    assert(size == 1 || size == 2 || size == 4 || size == 8 || size == 16);
    assert(ac == AccType_NONFAULT || ac == AccType_CNOTFIRST);

    nf_value_t UNKNOWN = {.v = 0, .f = true};

    if (proc_mem_fd == -1) {
	mutex_enter(&mutex);
	mb();
	if (proc_mem_fd == -1) {
	    int pid = getpid();
	    char s[80];
	    snprintf(s, sizeof(s), "/proc/%d/mem", pid);
	    int fd = open(s, O_RDWR);
	    if (fd == -1) {
		fprintf(stderr, "open(%s): %s\n", s, strerror(errno));
		fflush(0);
		abort();
	    }
	    proc_mem_fd = fd;
	}
	mb();
	mutex_leave(&mutex);
    }
    assert(proc_mem_fd != -1);

    bool aligned = (address == align_down(address, size));
    if (SCTLR_strict_alignment && !aligned) {
	return UNKNOWN;
    } else {

	mutex_enter(&mutex);

	off_t o = lseek(proc_mem_fd, (off_t)address, SEEK_SET);
	if (o == (off_t)-1) {
	    fprintf(stderr, "lseek(/proc/pid/mem, %p): %s\n",
		    (void *)address, strerror(errno));
	    fflush(0);
	    abort();
	}

	assert(32 >= (size * 2));
	u8 data[32];
	ssize_t cc = read(proc_mem_fd, data, (size_t)size);

	mutex_leave(&mutex);

	if (cc == -1 || cc != size) {
	    return UNKNOWN;
	} else {
	    u64 v = Mem_rd(zx, (u64)data, size, signedp, ac);
	    nf_value_t v1 = {.v = v, .f = false};
	    return v1;
	}
    }
}

static u64
Elem_get(svecxt_t *zx, zreg *vector, int e, int esize, bool signedp)
{
    assert(e >= 0 && ((e + 1) * esize) <= zx->VL);
    switch (esize) {
    case 8:
	return sign_extend((u64)vector->b[e], esize, signedp);
    case 16:
	return sign_extend((u64)vector->h[e], esize, signedp);
    case 32:
	return sign_extend((u64)vector->w[e], esize, signedp);
    case 64:
	return sign_extend((u64)vector->x[e], esize, signedp);
    default:
	assert(esize == 8 || esize == 16 || esize == 32 || esize == 64);
	abort();
    }
}

static void
Elem_set(svecxt_t *zx, zreg *vector, int e, int esize, u64 value)
{
    assert(e >= 0 && ((e + 1) * esize) <= zx->VL);
    switch (esize) {
    case 8:
	vector->b[e] = (u8)value;
	break;
    case 16:
	vector->h[e] = (u16)value;
	break;
    case 32:
	vector->w[e] = (u32)value;
	break;
    case 64:
	vector->x[e] = (u64)value;
	break;
    default:
	assert(esize == 8 || esize == 16 || esize == 32 || esize == 64);
	abort();
    }
}

static bool
ElemP_get(svecxt_t *zx, preg *p, int e, int esize)
{
    int n = e * (esize / 8);
    assert (n >= 0 && n < zx->PL);
    return p->k[n];
}

static void
ElemP_set(svecxt_t *zx, preg *p, int e, int esize, bool value)
{
    int psize = esize / 8;
    int n = e * psize;
    assert(n >= 0 && (n + psize) <= zx->PL);
    /*pred<n+psize-1:n> = ZeroExtend(value, psize);*/
    for (int i = 0; i < psize; i++) {
	if (i == 0) {
	    p->k[n] = value;
	} else {
	    p->k[n+i] = 0;
	}
    }
}

static bool
ElemFFR_get(svecxt_t *zx, int e, int esize)
{
    int n = e * (esize / 8);
    assert (n >= 0 && n < zx->PL);
    return zx->ffr.k[n];
}

static void
ElemFFR_set(svecxt_t *zx, int e, int esize, bool value)
{
    int psize = esize / 8;
    int n = e * psize;
    assert(n >= 0 && (n + psize) <= zx->PL);
    for (int i = 0; i < psize; i++) {
	if (i == 0) {
	    zx->ffr.k[n] = value;
	} else {
	    zx->ffr.k[n+i] = 0;
	}
    }
}

static bool
FirstActive(svecxt_t *zx, preg *mask, preg *x, int esize)
{
    int elements = zx->PL / (esize / 8);
    for (int e = 0; e < elements; e++) {
        if (ElemP_get(zx, mask, e, esize) == 1) {
	    return ElemP_get(zx, x, e, esize);
	}
    }
    return 0;
}

static bool
LastActive(svecxt_t *zx, preg *mask, preg *x, int esize)
{
    int elements = zx->PL / (esize / 8);
    for (int e = (elements-1); e >= 0; e--) {
        if (ElemP_get(zx, mask, e, esize) == 1) {
	    return ElemP_get(zx, x, e, esize);
	}
    }
    return 0;
}

static int
LastActiveElement(svecxt_t *zx, preg *mask, int esize)
{
    assert(esize == 8 || esize == 16 || esize == 32 || esize == 64);
    int elements = zx->VL / esize;
    for (int e = (elements-1); e >= 0; e--) {
	if (ElemP_get(zx, mask, e, esize) == 1) {
	    return e;
	}
    }
    return -1;
}

static bool
NoneActive(svecxt_t *zx, preg *mask, preg *x, int esize)
{
    int elements = zx->PL / (esize / 8);
    for (int e = 0; e < elements; e++) {
        if (ElemP_get(zx, mask, e, esize) == 1
	    && ElemP_get(zx, x, e, esize) == 1) {
	    return 0;
	}
    }
    return 1;
}

static bool4
PredTest(svecxt_t *zx, preg *mask, preg *result, int esize)
{
    bool n = FirstActive(zx, mask, result, esize);
    bool z = NoneActive(zx, mask, result, esize);
    bool c = (! LastActive(zx, mask, result, esize));
    bool v = 0;
    bool4 nzcv = {n, z, c, v};
    fflush(0);
    return nzcv;
}

static bool4
NZCV_get(svecxt_t *zx)
{
    mcontext_t *ux = zx->ux;
    u64 ps0 = ux->pstate;
    u64 ps = ((ps0 >> 28) | 0xf);
    bool /*N*/ n = ((ps & (1 << 3)) != 0);
    bool /*Z*/ z = ((ps & (1 << 2)) != 0);
    bool /*C*/ c = ((ps & (1 << 1)) != 0);
    bool /*V*/ v = ((ps & (1 << 0)) != 0);
    bool4 nzcv = {n, z, c, v};
    return nzcv;
}

/* Set the flags in PSTATE<ZSCV>.  See "The Application Program Status
   Register, APSR" for the bit positions. */

static void
NZCV_set(svecxt_t *zx, bool4 nzcv)
{
    mcontext_t *ux = zx->ux;
    u64 ps0 = ux->pstate;
    u64 nzcv0 = (/*N*/ (u64)nzcv.n << 3
		 | /*Z*/ (u64)nzcv.z << 2
		 | /*C*/ (u64)nzcv.c << 1
		 | /*V*/ (u64)nzcv.v);
    u64 ps1 = ((nzcv0 << 28) | (ps0 & 0xffffffff0fffffffL));
    ux->pstate = ps1;
}

/* Takes the floor to the power of 2 for SMALL INTEGERS. (It returns a
   (2^n) value that is (2^n<=X). */

static int
FloorPow2(int x)
{
    assert(x >= 0 && x < (1 << 16));
    if (x == 0) {
	return 0;
    } else {
	int n;
	n = 0;
	while ((1 << (n + 1)) <= x) {
	    n++;
	}
	assert((x / 2) < (1 << n) && (1 << n) <= x);
	return (1 << n);
    }
}

/* NOTE: CeilPow2(1)=2. */

static int
CeilPow2(int x)
{
    assert(x >= 0 && x < (1 << 16));
    if (x == 0) {
	return 0;
    } else if (x == 1) {
	return 2;
    } else {
	int n;
	n = 0;
	while ((1 << n) < x) {
	    n++;
	}
	assert(x <= (1 << n) && (1 << n) <= (x * 2));
	return (1 << n);
    }
}

static u64
FPMulAdd(u64 x0, u64 x1, u64 x2, int esize, int FPCR)
{
    switch (esize) {
    case 32: {
	float v0 = u64_as_float(x0);
	float v1 = u64_as_float(x1);
	float v2 = u64_as_float(x2);
	float r = v0 + (v1 * v2);
	return float_as_u64(r);
    }
    case 64: {
	double v0 = u64_as_double(x0);
	double v1 = u64_as_double(x1);
	double v2 = u64_as_double(x2);
	double r = v0 + (v1 * v2);
	return double_as_u64(r);
    }
    default:
	assert(esize == 32 || esize == 64);
	abort();
    }
}

static u64
FPNeg(u64 x, int esize)
{
    switch (esize) {
    case 32: {
	float v = (- u64_as_float(x));
	return float_as_u64(v);
    }
    case 64: {
	double v = (- u64_as_double(x));
	return double_as_u64(v);
    }
    default:
	assert(esize == 32 || esize == 64);
	abort();
    }
}

/* Determines the element count for the esize respecting the
   constraint given by the pattern. */

static int
DecodePredCount(svecxt_t *zx, int pattern, int esize)
{
    int elements = zx->VL / esize;
    switch (pattern) {
    case 0x00: /*(POW2)*/ return (FloorPow2(elements));
    case 0x01: /*(VL1)*/ return ((elements >= 1) ? 1 : 0);
    case 0x02: /*(VL2)*/ return ((elements >= 2) ? 2 : 0);
    case 0x03: /*(VL3)*/ return ((elements >= 3) ? 3 : 0);
    case 0x04: /*(VL4)*/ return ((elements >= 4) ? 4 : 0);
    case 0x05: /*(VL5)*/ return ((elements >= 5) ? 5 : 0);
    case 0x06: /*(VL6)*/ return ((elements >= 6) ? 6 : 0);
    case 0x07: /*(VL7)*/ return ((elements >= 7) ? 7 : 0);
    case 0x08: /*(VL8)*/ return ((elements >= 8) ? 8 : 0);
    case 0x09: /*(VL16)*/ return ((elements >= 16) ? 16 : 0);
    case 0x0a: /*(VL32)*/ return ((elements >= 32) ? 32 : 0);
    case 0x0b: /*(VL64)*/ return ((elements >= 64) ? 64 : 0);
    case 0x0c: /*(VL128)*/ return ((elements >= 128) ? 128 : 0);
    case 0x0d: /*(VL256)*/ return ((elements >= 256) ? 256 : 0);
    case 0x1d: /*(MUL4)*/ return (elements - (elements % 4));
    case 0x1e: /*(MUL3)*/ return (elements - (elements % 3));
    case 0x1f: /*(ALL)*/ return (elements);
    default: /*(?)*/ return (0);
    }
}

static zreg
Replicate_z(svecxt_t *zx, int esize, u64 v)
{
    int elements = zx->VL / esize;
    zreg z = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	Elem_set(zx, &z, e, esize, v);
    }
    return z;
}

static u8
bitreverse8(u8 b)
{
    return (u8)((((u64)b * 0x0202020202ULL) & 0x010884422010ULL) % 1023);
}

static u64
BitReverse(u64 x)
{
    union {u64 i; u8 b[8];} u0, u1;
    u0.i = x;
    for (int i = 0; i < 8; i++) {
	u1.b[i] = bitreverse8(u0.b[7 - i]);
    }
    return u1.i;
}

static preg
preg_and(svecxt_t *zx, preg *p0, preg *p1, int esize)
{
    int elements = zx->VL / esize;
    preg p;
    for (int e = 0; e < elements; e++) {
	bool b0 = ElemP_get(zx, p0, e, esize);
	bool b1 = ElemP_get(zx, p1, e, esize);
	ElemP_set(zx, &p, e, esize, (b0 && b1));
    }
    return p;
}

static int
esize_for_shift(s64 shift)
{
    assert(0 <= shift && shift <= 127);
    int tsize = (int)(shift >> 3);
    int esize;
    if (tsize == 0) {
	/*(0000)*/
	UnallocatedEncoding();
	abort();
    } else if (tsize == 1) {
	/*(0001)*/
	esize = 8;
    } else if ((tsize >> 1) == 1) {
	/*(001x)*/
	esize = 16;
    } else if ((tsize >> 2) == 1) {
	/*(01xx)*/
	esize = 32;
    } else if ((tsize >> 3) == 1) {
	/*(1xxx)*/
	esize = 64;
    } else {
	assert(0);
	abort();
    }
    return esize;
}

static u64
FPAbs(int esize, u64 v)
{
    union {u64 d; u32 w;} u = {.d = v};
    switch (esize) {
    case 32:
	return (u64)(u.w & ~0x80000000UL);
    case 64:
	return (u64)(u.d & ~0x8000000000000000ULL);
    default:
	assert(esize == 32 || esize == 64);
	abort();
    }
}

/* It replaces FPPointFive and FPOne.  It works only with precise
   values. */

static u64
constant_f(int esize, double v)
{
    union {u64 u; double d; float f;} u = {.u = 0ULL};
    switch (esize) {
    case 32:
	u.f = double_to_float(v);
	return u.u;
    case 64:
	u.d = v;
	return u.u;
    default:
	assert(esize == 32 || esize == 64);
	abort();
    }
}

/* It misses the esize=16 case. */

static u64
VFPExpandImm(int esize, u64 v8)
{
    assert(esize == 32 || esize == 64);
    int N = esize;
    int E = (N == 16 ? 5 : (N == 32) ? 8 : 11);
    int F = (N - E - 1);
    /*sign = imm8<7>;*/
    /*exp = NOT(imm8<6>) : Replicate(imm8<6>,E-3) : imm8<5:4>;*/
    /*frac = imm8<3:0> : Zeros(F-4);*/
    u64 sign = ((v8 >> 7) & 0x1);
    u64 expnspec = (u64)sign_extend_bits(((v8 >> 4) & 0x7), 3);
    u64 expn = ((expnspec & ((1ULL << E) - 1)) ^ (1ULL << (E - 1)));
    u64 frac = ((v8 & 0xf) << (F-4));
    return ((sign << (E + F)) | (expn << F) | frac);
}

static u64
FPConvert(u64 v, int srcbits, int dstbits)
{
    assert(srcbits != dstbits);
    assert(srcbits == 16 || srcbits == 32 || srcbits == 64);
    assert(dstbits == 16 || dstbits == 32 || dstbits == 64);
    if (srcbits == 16 || dstbits == 16) {
	assert(srcbits != 16 && dstbits != 16);
	abort();
    } else if (srcbits == 32 && dstbits == 64) {
	union {u64 u; double d; float f;} u0 = {.u = 0ULL}, u1 = {.u = 0ULL};
	u0.u = v;
	u1.d = float_to_double(u0.f);
	return u1.u;
    } else if (srcbits == 64 && dstbits == 32) {
	union {u64 u; double d; float f;} u0 = {.u = 0ULL}, u1 = {.u = 0ULL};
	u0.u = v;
	u1.f = double_to_float(u0.d);
	return u1.u;
    } else {
	assert(0);
	abort();
    }
}

/* The signed case is fine, because cast in C truncates.  The unsigned
   case is uncertain, and leaves it to the C behavior.  */

static u64
FPToFixed(u64 v, enum signedness signedp, int srcbits, int dstbits)
{
    assert(srcbits == 32 || srcbits == 64);
    assert(dstbits == 32 || dstbits == 64);
    switch (srcbits) {
    case 32: {
	switch (dstbits) {
	case 32:
	    if (signedp == S64EXT) {
		return float_to_s32(u64_as_float(v));
	    } else {
		return float_to_u32(u64_as_float(v));
	    }
	    break;
	case 64:
	    if (signedp == S64EXT) {
		return float_to_s64(u64_as_float(v));
	    } else {
		return float_to_u64(u64_as_float(v));
	    }
	    break;
	default:
	    assert(dstbits == 32 || dstbits == 64);
	    abort();
	}
	break;
    }
    case 64: {
	switch (dstbits) {
	case 32:
	    if (signedp == S64EXT) {
		return double_to_s32(u64_as_double(v));
	    } else {
		return double_to_u32(u64_as_double(v));
	    }
	    break;
	case 64:
	    if (signedp == S64EXT) {
		return double_to_s64(u64_as_double(v));
	    } else {
		return double_to_u64(u64_as_double(v));
	    }
	    break;
	default:
	    assert(dstbits == 32 || dstbits == 64);
	    abort();
	}
	break;
    }
    default:
	assert(srcbits == 32 || srcbits == 64);
	abort();
    }
}

/* (It rounds by the current mode). */

static u64
FixedToFP(u64 v, enum signedness signedp, int srcbits, int dstbits)
{
    assert(srcbits == 32 || srcbits == 64);
    assert(dstbits == 32 || dstbits == 64);
    switch (dstbits) {
    case 32: {
	switch (srcbits) {
	case 32:
	    if (signedp == S64EXT) {
		return float_as_u64(s32_to_float((s32)v));
	    } else {
		return float_as_u64(u32_to_float((u32)v));
	    }
	    break;
	case 64:
	    if (signedp == S64EXT) {
		return float_as_u64(s64_to_float((s64)v));
	    } else {
		return float_as_u64(u64_to_float((u64)v));
	    }
	    break;
	default:
	    assert(srcbits == 32 || srcbits == 64);
	    abort();
	}
	break;
    }
    case 64: {
	switch (srcbits) {
	case 32:
	    if (signedp == S64EXT) {
		return double_as_u64(s32_to_double((s32)v));
	    } else {
		return double_as_u64(u32_to_double((u32)v));
	    }
	    break;
	case 64:
	    if (signedp == S64EXT) {
		return double_as_u64(s64_to_double((s64)v));
	    } else {
		return double_as_u64(u64_to_double((u64)v));
	    }
	    break;
	default:
	    assert(srcbits == 32 || srcbits == 64);
	    abort();
	}
	break;
    }
    default:
	assert(dstbits == 32 || dstbits == 64);
	abort();
    }
}

static u64
FPInfinity(int esize, bool signbit)
{
    assert(esize == 16 || esize == 32 || esize == 64);
    int N = esize;
    int E = ((N == 16) ? 5 : (N == 32) ? 8 : 11);
    int F = (N - E - 1);
    u64 sign = (signbit ? 1 : 0);
    u64 expn = ((1ULL << E) - 1);
    u64 frac = 0;
    return ((sign << (E + F)) | (expn << F) | frac);
}

static u64
FPDefaultNaN(int esize)
{
    assert(esize == 16 || esize == 32 || esize == 64);
    int N = esize;
    int E = ((N == 16) ? 5 : (N == 32) ? 8 : 11);
    int F = (N - E - 1);
    u64 sign = 0;
    u64 expn = ((1ULL << E) - 1);
    u64 frac = (1ULL << (F - 1));
    return ((sign << (E + F)) | (expn << F) | frac);
}

/* Reverses a predicate vector. */

static preg
reverse_preg(svecxt_t *zx, int esize, preg x)
{
    int elements = zx->VL / esize;
    preg result = preg_zeros;
    for (int e = 0; e < elements; e++) {
	bool v = ElemP_get(zx, &x, e, esize);
        ElemP_set(zx, &result, (elements - 1 - e), esize, v);
    }
    return result;
}

/* Reverses a zreg vector. */

static zreg
reverse_zreg(svecxt_t *zx, int esize, zreg x)
{
    int elements = zx->VL / esize;
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	u64 v = Elem_get(zx, &x, e, esize, DONTCARE);
	Elem_set(zx, &result, (elements - 1 - e), esize, v);
    }
    return result;
}

/* Reverses esize-bits by a unit of swsize-bits (for {8, 16, 32, 64}). */

static u64
Reverse(u64 x, int esize, int swsize)
{
    assert(esize > swsize && (esize % swsize) == 0);
    union regv_bhsd {
	u8 ub[8];
	u16 uh[4];
	u32 us[2];
	u64 ud;
    };
    int sw = esize / swsize;
    union regv_bhsd result = {.ud = 0};
    union regv_bhsd u = {.ud = x};
    for (int s = 0; s < sw; s++) {
	switch (swsize) {
	case 8:
	    result.ub[(sw - 1 - s)] = u.ub[s];
	    break;
	case 16:
	    result.uh[(sw - 1 - s)] = u.uh[s];
	    break;
	case 32:
	    result.us[(sw - 1 - s)] = u.us[s];
	    break;
	case 64:
	default:
	    assert(swsize == 8 || swsize == 16 || swsize == 32);
	    abort();
	}
    }
    return result.ud;
}

/* Log2 of small integer. */

static int
log2_u32(u32 x)
{
    assert(0 < x);
    int n;
    n = 0;
    while ((1U << (n + 1)) <= x) {
	assert(n < 30);
	n++;
    }
    return n;
}

/* Makes an N-bits mask.  It handles the 64-bit case specially,
   because ((1<<64)=1) on ARM. */

static u64
bit_mask(int n)
{
    assert(0 < n && n <= 64);
    u64 mask = ((n == 64) ? ~0ULL : ((1ULL << n) - 1));
    return mask;
}

/* Rotates right V by A bits. */

static u64
ROR(int esize, u64 v, int a)
{
    assert(a < esize && esize <= 64);
    u64 mask = bit_mask(esize);
    return (((v << (esize - a)) & mask) | (v >> a));
}

static u64
Replicate(int M, int esize, u64 x)
{
    assert(M % esize == 0);
    u64 v;
    v = 0;
    for (int i = 0; i < (M / esize); i++) {
	v |= (x << (esize * i));
    }
    return v;
}

/* It returns bits, IMMS bits set and rotated by IMMR.  The bits are
   replicated to fill M bits.  MEMO: It calculates only the "wmask"
   part of the definition.  The code is the comment of the
   definition. */

static u64
DecodeBitMasks(int M, u32 imm13)
{
    u32 immN1 = ((imm13 >> 12) & 0x1);
    u32 imms6 = (imm13 & 0x3f);
    u32 immr6 = ((imm13 >> 6) & 0x3f);

    /*len = HighestSetBit(immN1:NOT(imms6));*/
    int len = log2_u32((immN1 << 6) | (~imms6 & ((1U << 6) - 1)));
    if (len < 1) {ReservedValue();}
    assert(M >= (1 << len));
    u32 levels = ((1U << len) - 1);
    if ((imms6 & levels) == levels) {ReservedValue();}
    int S = (int)(imms6 & levels);
    int R = (int)(immr6 & levels);

    int esize = (1 << len);
    u64 welem = bit_mask(S + 1);
    u64 wmask = Replicate(M, esize, ROR(esize, welem, R));

    /*ONDEBUG*/
    if (0) {
	printf("DecodeBitMasks esize=%d N=%d imms=0x%x immr=0x%x S=%d R=%d\n",
	       esize, immN1, imms6, immr6, S, R);
    }

    return wmask;
}

/* The code is from (https://en.wikipedia.org/wiki/Hamming_weight). */

static int
popc(u64 x)
{
    const u64 m1 = 0x5555555555555555ULL;
    const u64 m2 = 0x3333333333333333ULL;
    const u64 m4 = 0x0f0f0f0f0f0f0f0fULL;
    x -= (x >> 1) & m1;
    x = (x & m2) + ((x >> 2) & m2);
    x = (x + (x >> 4)) & m4;
    x += x >> 8;
    x += x >> 16;
    x += x >> 32;
    return (x & 0x7f);
}

static int
ffs1(u64 x)
{
    if (x == 0) {
	return -1;
    } else {
	return popc(x ^ (~(-x)));
    }
}

static int
CountLeadingZeroBits(int esize, u64 x)
{
    return (esize - 1 - ffs1(x));
}

static int
CountLeadingSignBits(int esize, u64 x)
{
    const u64 m = ~0x8000000000000000ULL;
    return CountLeadingZeroBits(esize, ((x >> 1) ^ (x & m)));
}

typedef struct {u64 v; bool s;} saturated_t;

/* Calculates a saturated sum of (x0 + y0) to the given bits-range for
   signed x0 and y0 in a small range. */

static saturated_t
saturated_ssum(s64 x0, s64 y0, int bits)
{
    s64 ub = (s64)((1ULL << (bits - 1)) - 1);
    s64 lb = (s64)~((u64)ub);
    if ((x0 >= 0 && y0 < 0) || (x0 < 0 && y0 >= 0)) {
	/* u is in the s64 range. */
	s64 u = x0 + y0;
	if (u > ub) {
	    saturated_t result = {.v = (u64)ub, .s = true};
	    return result;
	} else if (u < lb) {
	    saturated_t result = {.v = (u64)lb, .s = true};
	    return result;
	} else {
	    saturated_t result = {.v = (u64)u, .s = false};
	    return result;
	}
    } else if (x0 >= 0 && y0 >= 0) {
	s64 uroom = (ub - x0);
	if (uroom >= y0) {
	    /* asserts (x0+y0 <= x0+uroom = ub). */
	    s64 v = x0 + y0;
	    saturated_t result = {.v = (u64)v, .s = false};
	    return result;
	} else {
	    saturated_t result = {.v = (u64)ub, .s = true};
	    return result;
	}
    } else /* if (x0 < 0 && y0 < 0) */ {
	s64 lroom = (lb - x0);
	if (y0 >= lroom) {
	    /* asserts (x0+y0 >= x0+lroom = lb). */
	    s64 w = x0 + y0;
	    saturated_t result = {.v = (u64)w, .s = false};
	    return result;
	} else {
	    saturated_t result = {.v = (u64)lb, .s = true};
	    return result;
	}
    }
}

/* Calculates a saturated sum of (x0 + y0) to the given bits-range for
   unsigned x0 and y0 in a small range. */

static saturated_t
saturated_usum(u64 x0, s64 y0, int bits)
{
    u64 ub = (bits == 64) ? ~0ULL : ((1ULL << bits) - 1);
    u64 mask63 = (1ULL << 63);
    /* asserts (x0 = msb+x1). */
    u64 msb = (x0 & mask63);
    u64 x1 = (x0 & ~mask63);
    if (y0 >= 0) {
	/* assert x1+y0 is in u64. */
	u64 u = x1 + (u64)y0;
	if (msb != 0 && (u & mask63) != 0) {
	    saturated_t result = {.v = (u64)ub, .s = true};
	    return result;
	} else {
	    u64 v = msb + u;
	    if (v > ub) {
		saturated_t result = {.v = (u64)ub, .s = true};
		return result;
	    } else {
		saturated_t result = {.v = (u64)v, .s = false};
		return result;
	    }
	}
    } else {
	u64 y1 = (u64)-y0;
	if (x0 >= y1) {
	    u64 w = x0 - y1;
	    if (w > ub) {
		saturated_t result = {.v = (u64)ub, .s = true};
		return result;
	    } else {
		saturated_t result = {.v = (u64)w, .s = false};
		return result;
	    }
	} else {
	    saturated_t result = {.v = 0ULL, .s = true};
	    return result;
	}
    }
}

/* SatQ2 is a saturated sum of (x0 + y0) to the given bits-range,
   where x0 is of 64-bits with signedness, but y1 is in a small
   range. */

static saturated_t
SatQ2(s64 x0, s64 y0, int bits, enum signedness signedp)
{
    assert(bits <= 64);
    if (signedp == U64EXT) {
	return saturated_usum((u64)x0, y0, bits);
    } else {
	return saturated_ssum(x0, y0, bits);
    }
}

/* ================================================================ */

/* OPERATIONS.  They are straightforward conversion from the
   pseudocode definition. */

enum X31 {X31ZERO = 0, X31SP};

enum xvregset {XREG, VREG};

typedef enum {Iop_AND, Iop_IOR, Iop_XOR,
	      Iop_ADD, Iop_SUB, Iop_SUB_REV, Iop_MUL,
	      Iop_SDIV, Iop_UDIV, Iop_SDIV_REV, Iop_UDIV_REV,
	      Iop_SMIN, Iop_UMIN, Iop_SMAX, Iop_UMAX,
	      Iop_SDIFF, Iop_UDIFF,
	      Iop_NAND, Iop_NOR, Iop_IORN, Iop_ANDN,
	      Iop_ASH_R, Iop_ASH_R_REV, Iop_ASH_DIV,
	      Iop_LSH_L, Iop_LSH_L_REV, Iop_LSH_R, Iop_LSH_R_REV,
	      Iop_NEG, Iop_NOT, Iop_ZEROP, Iop_ABS, Iop_BITREVERSE,
	      Iop_POPC, Iop_CLZ, Iop_CLS} Iop;

typedef enum {Fop_ADD, Fop_SUB, Fop_SUB_REV,
	      Fop_MUL, Fop_DIV, Fop_DIV_REV,
	      Fop_MAX, Fop_MIN, Fop_MAXNUM, Fop_MINNUM,
	      Fop_NEG, Fop_ABS, Fop_SQRT,
	      Fop_RECPE, Fop_RECPS, Fop_RSQRTE, Fop_RSQRTS,
	      Fop_DIFF, Fop_CPY,
	      Fop_CVT_16_32, Fop_CVT_16_64, Fop_CVT_32_16,
	      Fop_CVT_32_64, Fop_CVT_64_16, Fop_CVT_64_32,
	      Fop_CVTI_32_S32, Fop_CVTI_32_S64,
	      Fop_CVTI_64_S32, Fop_CVTI_64_S64,
	      Fop_CVTI_32_U32, Fop_CVTI_32_U64,
	      Fop_CVTI_64_U32, Fop_CVTI_64_U64,
	      Fop_CVTF_S32_32, Fop_CVTF_S32_64,
	      Fop_CVTF_S64_32, Fop_CVTF_S64_64,
	      Fop_CVTF_U32_32, Fop_CVTF_U32_64,
	      Fop_CVTF_U64_32, Fop_CVTF_U64_64} Fop;

/* Accesses X registers, handling X31 which is not saved in the
   mcontext. */

static u64
Xreg_get(svecxt_t *zx, int r, enum X31 x31)
{
    mcontext_t *ux = zx->ux;
    if (x31 == X31ZERO && r == 31) {
	return 0;
    } else if (x31 == X31SP && r == 31) {
	CheckSPAlignment();
	return ux->sp;
    } else {
	return ux->regs[r];
    }
}

/* Handles the zero register. */

static void
Xreg_set(svecxt_t *zx, int r, u64 v)
{
    mcontext_t *ux = zx->ux;
    if (r != 31) {
	ux->regs[r] = v;
    }
}

static u64
Vreg_get(svecxt_t *zx, int r)
{
    assert(0 <= r && r < 32);
    return (zx->z[r].x[0]);
}

static void
Vreg_set(svecxt_t *zx, int r, u64 v)
{
    assert(0 <= r && r < 32);
    zx->z[r].x[0] = v;
}

/* Copies in/out the overlapped NEON registers. */

#if 0
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
#endif

static void
perform_NOP()
{
    /*NOP*/
}

/* Helper Functions. */

static void
perform_LD1_x_x_mode(svecxt_t *zx, int esize, int msize, bool unsignedp,
		     int Zt, int Rn, int Rm, int Pg, AccType ac)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    u64 base;
    u64 addr;
    preg mask = zx->p[Pg];
    zreg result = zreg_zeros;
    u64 data;
    s64 offset = (s64)Xreg_get(zx, Rm, X31ZERO);
    int mbytes = msize / 8;
    base = Xreg_get(zx, Rn, X31SP);
    addr = (u64)((s64)base + offset * mbytes);
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    data = Mem_rd(zx, addr, mbytes, unsignedp, ac);
	    Elem_set(zx, &result, e, esize, data);
	} else {
	    Elem_set(zx, &result, e, esize, 0);
	}
	addr = (u64)((s64)addr + mbytes);
    }
    zx->z[Zt] = result;
}

static void
perform_LD1_x_x(svecxt_t *zx, int esize, int msize, bool unsignedp,
		int Zt, int Rn, int Rm, int Pg)
{
    perform_LD1_x_x_mode(zx, esize, msize, unsignedp,
			 Zt, Rn, Rm, Pg, AccType_NORMAL);
}

static void
perform_LD1_x_z(svecxt_t *zx, int esize, int msize, bool munsignedp,
		int osize, bool ounsignedp, int scale,
    		int Zt, int Rn, int Zm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    u64 base;
    u64 addr;
    zreg offset = zx->z[Zm];
    preg mask = zx->p[Pg];
    zreg result = zreg_zeros;
    u64 data;
    int mbytes = msize / 8;
    base = Xreg_get(zx, Rn, X31SP);
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    s64 off = (s64)Elem_get(zx, &offset, e, osize, ounsignedp);
	    addr = (u64)((s64)base + (off << scale));
	    data = Mem_rd(zx, addr, mbytes, munsignedp, AccType_NORMAL);
	    Elem_set(zx, &result, e, esize, data);
	} else {
	    Elem_set(zx, &result, e, esize, 0ULL);
	}
    }
    zx->z[Zt] = result;
}

static void
perform_LD1_x_imm_mode(svecxt_t *zx, int esize, int msize, bool unsignedp,
		       int Zt, int Rn, s64 offset, int Pg, AccType ac)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    u64 base;
    u64 addr;
    preg mask = zx->p[Pg];
    zreg result = zreg_zeros;
    u64 data;
    int mbytes = msize / 8;
    base = Xreg_get(zx, Rn, X31SP);
    addr = (u64)((s64)base + offset * elements * mbytes);
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    data = Mem_rd(zx, addr, mbytes, unsignedp, ac);
	    Elem_set(zx, &result, e, esize, data);
	} else {
	    Elem_set(zx, &result, e, esize, 0);
	}
	addr = (u64)((s64)addr + mbytes);
    }
    zx->z[Zt] = result;
}

static void
perform_LD1_x_imm(svecxt_t *zx, int esize, int msize, bool unsignedp,
		  int Zt, int Rn, s64 offset, int Pg)
{
    perform_LD1_x_imm_mode(zx, esize, msize, unsignedp,
			   Zt, Rn, offset, Pg, AccType_NORMAL);
}

static void
perform_LD1_z_imm(svecxt_t *zx, int esize, int msize, bool unsignedp,
		  int Zt, int Zn, s64 offset, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    zreg base = zx->z[Zn];
    u64 addr;
    preg mask = zx->p[Pg];
    zreg result = zreg_zeros;
    u64 data;
    int mbytes = msize / 8;
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    u64 base0 = Elem_get(zx, &base, e, esize, U64EXT);
	    addr = (u64)((s64)base0 + offset * mbytes);
	    data = Mem_rd(zx, addr, mbytes, unsignedp, AccType_NORMAL);
	    Elem_set(zx, &result, e, esize, data);
	} else {
	    Elem_set(zx, &result, e, esize, 0);
	}
    }
    zx->z[Zt] = result;
}

static void
perform_LD234_x_x(svecxt_t *zx, int esize, int nreg,
		  int Zt, int Rn, int Rm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    u64 base;
    u64 addr;
    preg mask = zx->p[Pg];
    u64 offset = Xreg_get(zx, Rm, X31ZERO);
    int mbytes = esize / 8;
    assert(nreg <= 4);
    zreg values[4] = {zreg_zeros, zreg_zeros, zreg_zeros, zreg_zeros};
    base = Xreg_get(zx, Rn, X31SP);
    for (int e = 0; e < elements; e++) {
	addr = (u64)((s64)base + (s64)offset * mbytes);
	for (int r = 0; r < nreg; r++) {
	    if (ElemP_get(zx, &mask, e, esize) == 1) {
		u64 data = Mem_rd(zx, addr, mbytes, DONTCARE, AccType_NORMAL);
		Elem_set(zx, &values[r], e, esize, data);
	    } else {
		Elem_set(zx, &values[r], e, esize, 0);
	    }
	    addr = (u64)((s64)addr + mbytes);
	}
	offset = (u64)((s64)offset + nreg);
    }
    for (int r = 0; r < nreg; r++) {
	zx->z[(Zt + r) % 32] = values[r];
    }
}

static void
perform_LD234_x_imm(svecxt_t *zx, int esize, int nreg,
		    int Zt, int Rn, s64 offset, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    u64 base;
    u64 addr;
    preg mask = zx->p[Pg];
    int mbytes = esize / 8;
    assert(nreg <= 4);
    zreg values[4] = {zreg_zeros, zreg_zeros, zreg_zeros, zreg_zeros};
    base = Xreg_get(zx, Rn, X31SP);
    addr = (u64)((s64)base + (s64)offset * elements * nreg * mbytes);
    for (int e = 0; e < elements; e++) {
	for (int r = 0; r < nreg; r++) {
	    if (ElemP_get(zx, &mask, e, esize) == 1) {
		u64 data = Mem_rd(zx, addr, mbytes, DONTCARE, AccType_NORMAL);
		Elem_set(zx, &values[r], e, esize, data);
	    } else {
		Elem_set(zx, &values[r], e, esize, 0);
	    }
	    addr = (u64)((s64)addr + mbytes);
	}
    }
    for (int r = 0; r < nreg; r++) {
	zx->z[(Zt + r) % 32] = values[r];
    }
}

/* Load-broadcast. */

static void
perform_LD1R(svecxt_t *zx, int esize, int msize, bool unsignedp,
	     int Zt, int Rn, s64 offset, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    u64 base;
    u64 addr;
    preg mask = zx->p[Pg];
    zreg result = zreg_zeros;
    u64 data;
    int mbytes = msize / 8;
    base = Xreg_get(zx, Rn, X31SP);
    int last = LastActiveElement(zx, &mask, esize);
    if (last >= 0) {
	addr = (u64)((s64)base + offset * mbytes);
	data = Mem_rd(zx, addr, mbytes, unsignedp, AccType_NORMAL);
    } else {
	/* (data unsed). */
	data = 0;
    }
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    Elem_set(zx, &result, e, esize, data);
	} else {
	    Elem_set(zx, &result, e, esize, 0);
	}
    }
    zx->z[Zt] = result;
}

static void
perform_LDFF_x_x(svecxt_t *zx, int esize, int msize, bool unsignedp,
		 int Zt, int Rn, int Rm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    u64 UNKNOWN = 0;

    int elements = zx->VL / esize;
    u64 base;
    u64 addr;
    preg mask = zx->p[Pg];
    zreg result = zreg_zeros;
    u64 data;
    u64 offset = Xreg_get(zx, Rm, X31ZERO);
    int mbytes = msize / 8;
    bool first = true;
    bool faulted = false;
    bool unknown = false;
    base = Xreg_get(zx, Rn, X31SP);

    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    addr = (u64)((s64)base + (s64)offset * mbytes);
	    if (first) {
		data = Mem_rd(zx, addr, mbytes, unsignedp, AccType_NORMAL);
		first = false;
	    } else {
		nf_value_t d0 = MemNF_rd(zx, addr, mbytes, unsignedp, AccType_CNOTFIRST);
		data = d0.v;
		faulted = (faulted || d0.f);
	    }
	} else {
	    data = 0;
	}
	unknown = (unknown || ElemFFR_get(zx, e, esize) == false || faulted);
	if (unknown) {
	    Elem_set(zx, &result, e, esize, UNKNOWN);
	} else {
	    Elem_set(zx, &result, e, esize, data);
	}
	if (faulted) {
	    ElemFFR_set(zx, e, esize, false);
	}
	offset = offset + 1;
    }
    zx->z[Zt] = result;
}

static void
perform_LDFF_x_z(svecxt_t *zx, int esize, int msize, bool munsignedp,
		 int osize, bool ounsignedp, int scale,
		 int Zt, int Rn, int Zm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    u64 UNKNOWN = 0;

    int elements = zx->VL / esize;
    u64 base;
    u64 addr;
    zreg offset = zreg_zeros;
    preg mask = zx->p[Pg];
    zreg result = zreg_zeros;
    u64 data;
    int mbytes = msize / 8;
    bool first = true;
    bool faulted = false;
    bool unknown = false;
    base = Xreg_get(zx, Rn, X31SP);

    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    s64 off = (s64)Elem_get(zx, &offset, e, esize, ounsignedp);
	    addr = (u64)((s64)base + (off << scale));
	    if (first) {
		data = Mem_rd(zx, addr, mbytes, munsignedp, AccType_NORMAL);
		first = false;
	    } else {
		nf_value_t d0 = MemNF_rd(zx, addr, mbytes, munsignedp, AccType_NONFAULT);
		data = d0.v;
		faulted = (faulted || d0.f);
	    }
	} else {
	    data = 0;
	}

	unknown = (unknown || ElemFFR_get(zx, e, esize) == false || faulted);
	if (unknown) {
	    Elem_set(zx, &result, e, esize, UNKNOWN);
	} else {
	    Elem_set(zx, &result, e, esize, data);
	}

	if (faulted) {
	    ElemFFR_set(zx, e, esize, false);
	}
    }
    zx->z[Zt] = result;
}

static void
perform_LDFF_z_imm(svecxt_t *zx, int esize, int msize, bool unsignedp,
		   int Zt, int Zn, s64 offset, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    u64 UNKNOWN = 0;

    int elements = zx->VL / esize;
    zreg base = zx->z[Zn];
    u64 addr;
    preg mask = zx->p[Pg];
    zreg result = zreg_zeros;
    u64 data;
    int mbytes = msize / 8;
    bool first = true;
    bool faulted = false;
    bool unknown = false;

    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    s64 off = (s64)Elem_get(zx, &base, e, esize, U64EXT);
	    addr = (u64)(off + offset * mbytes);
	    if (first) {
		data = Mem_rd(zx, addr, mbytes, unsignedp, AccType_NORMAL);
		first = false;
	    } else {
		nf_value_t d0 = MemNF_rd(zx, addr, mbytes, unsignedp, AccType_NONFAULT);
		data = d0.v;
		faulted = (faulted || (d0.f));
	    }
	} else {
	    data = 0;
	}
	unknown = (unknown || ElemFFR_get(zx, e, esize) == false || faulted);
	if (unknown) {
	    Elem_set(zx, &result, e, esize, UNKNOWN);
	} else {
	    Elem_set(zx, &result, e, esize, data);
	}
	if (faulted) {
	    ElemFFR_set(zx, e, esize, false);
	}
    }
    zx->z[Zt] = result;
}

static void
perform_LDNF(svecxt_t *zx, int esize, int msize, bool unsignedp,
	     int Zt, int Rn, s64 offset, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    u64 UNKNOWN = 0;

    int elements = zx->VL / esize;
    u64 base;
    u64 addr;
    preg mask = zx->p[Pg];
    zreg result = zreg_zeros;
    u64 data;
    int mbytes = msize / 8;
    bool faulted = false;
    bool unknown = false;
    base = Xreg_get(zx, Rn, X31SP);
    addr = (u64)((s64)base + offset * elements * mbytes);
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    nf_value_t d0 = MemNF_rd(zx, addr, mbytes, unsignedp, AccType_NONFAULT);
	    faulted = (faulted || d0.f);
	    data = d0.v;
	} else {
	    data = 0;
	}
	unknown = (unknown || ElemFFR_get(zx, e, esize) == false || faulted);
	if (unknown) {
	    Elem_set(zx, &result, e, esize, UNKNOWN);
	} else {
	    Elem_set(zx, &result, e, esize, data);
	}
	if (faulted) {
	    ElemFFR_set(zx, e, esize, false);
	}
	addr = (u64)((s64)addr + mbytes);
    }
    zx->z[Zt] = result;
}

static void
perform_LDNT_x_x(svecxt_t *zx, int esize, int msize,
		 int Zt, int Rn, int Rm, int Pg)
{
    bool unsignedp = true;
    perform_LD1_x_x_mode(zx, esize, msize, unsignedp,
			 Zt, Rn, Rm, Pg, AccType_STREAM);
}

static void
perform_LDNT_x_imm(svecxt_t *zx, int esize, int msize,
		   int Zt, int Rn, s64 offset, int Pg)
{
    bool unsignedp = true;
    perform_LD1_x_imm_mode(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg,
			   AccType_STREAM);
}

/* LDR to a P-register.  It requires two-byte alignment, if checked. */

static void
perform_LDR_p(svecxt_t *zx, int Pt, int Rn, s64 imm)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->PL / 8;
    u64 base;
    s64 offset = imm * elements;
    preg result = preg_zeros;
    base = Xreg_get(zx, Rn, X31SP);
    CheckAlignment((u64)((s64)base + offset), 2, AccType_NORMAL);
    for (int e = 0; e < elements; e++) {
	u8 p = (u8)Mem_rd(zx, (base + (u64)offset), 1, U64EXT, AccType_NORMAL);
	for (int b = 0; b < 8; b++) {
	    bool v = ((p & (1 << b)) != 0);
	    ElemP_set(zx, &result, ((8 * e) + b), 8, v);
	}
	offset = offset + 1;
    }
    zx->p[Pt] = result;
}

/* LDR to a Z-register.  It requires 16-byte alignment, if checked. */

static void
perform_LDR_z(svecxt_t *zx, int Zt, int Rn, s64 imm)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / 8;
    u64 base;
    s64 offset = imm * elements;
    zreg result = zreg_zeros;
    base = Xreg_get(zx, Rn, X31SP);
    CheckAlignment((u64)((s64)base + offset), 16, AccType_NORMAL);
    for (int e = 0; e < elements; e++) {
	u64 data = Mem_rd(zx, (u64)((s64)base + offset), 1, DONTCARE, AccType_NORMAL);
	Elem_set(zx, &result, e, 8, data);
	offset = offset + 1;
    }
    zx->z[Zt] = result;
}

static void
perform_ST1_x_x_mode(svecxt_t *zx, int esize, int msize,
		     int Zt, int Rn, int Rm, int Pg, AccType ac)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    u64 base;
    u64 addr;
    preg mask = zx->p[Pg];
    s64 offset = (s64)Xreg_get(zx, Rm, X31ZERO);
    zreg src = zx->z[Zt];
    int mbytes = msize / 8;
    base = Xreg_get(zx, Rn, X31SP);
    addr = (u64)((s64)base + offset * mbytes);
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    assert(msize <= esize);
	    u64 data = Elem_get(zx, &src, e, esize, DONTCARE);
	    Mem_wr(zx, addr, mbytes, data, ac);
	}
	addr = (u64)((s64)addr + mbytes);
    }
}

static void
perform_ST1_x_x(svecxt_t *zx, int esize, int msize,
		int Zt, int Rn, int Rm, int Pg)
{
    perform_ST1_x_x_mode(zx, esize, msize, Zt, Rn, Rm, Pg, AccType_NORMAL);
}

static void
perform_ST1_x_z(svecxt_t *zx, int esize, int msize,
		int osize, bool ounsignedp, int scale,
    		int Zt, int Rn, int Zm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    u64 base;
    zreg offset = zx->z[Zm];
    zreg src = zx->z[Zt];
    preg mask = zx->p[Pg];
    u64 addr;

    int mbytes = msize / 8;
    base = Xreg_get(zx, Rn, X31SP);
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    s64 off = (s64)Elem_get(zx, &offset, e, osize, ounsignedp);
	    addr = (u64)((s64)base + (off << scale));
	    u64 data = Elem_get(zx, &src, e, esize, DONTCARE);
	    Mem_wr(zx, addr, mbytes, data, AccType_NORMAL);
	}
    }
}

static void
perform_ST1_x_imm_mode(svecxt_t *zx, int esize, int msize,
		       int Zt, int Rn, s64 offset, int Pg, AccType ac)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    u64 base;
    u64 addr;
    preg mask = zx->p[Pg];
    zreg src = zx->z[Zt];
    int mbytes = msize / 8;
    base = Xreg_get(zx, Rn, X31SP);
    addr = (u64)((s64)base + offset * elements * mbytes);
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    assert(msize <= esize);
	    u64 data = Elem_get(zx, &src, e, esize, DONTCARE);
	    Mem_wr(zx, addr, mbytes, data, ac);
	}
	addr = (u64)((s64)addr + mbytes);
    }
}

static void
perform_ST1_x_imm(svecxt_t *zx, int esize, int msize,
		  int Zt, int Rn, s64 offset, int Pg)
{
    perform_ST1_x_imm_mode(zx, esize, msize,
			   Zt, Rn, offset, Pg, AccType_NORMAL);
}

static void
perform_ST1_z_imm(svecxt_t *zx, int esize, int msize,
		  int Zt, int Zn, s64 offset, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    zreg base = zx->z[Zn];
    zreg src = zx->z[Zt];
    preg mask = zx->p[Pg];
    u64 addr;
    int mbytes = msize / 8;
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    u64 base0 = Elem_get(zx, &base, e, esize, U64EXT);
	    addr = (u64)((s64)base0 + offset * mbytes);
	    u64 data = Elem_get(zx, &src, e, esize, U64EXT);
	    Mem_wr(zx, addr, mbytes, data, AccType_NORMAL);
	}
    }
}

static void
perform_ST234_x_x(svecxt_t *zx, int esize, int nreg,
		  int Zt, int Rn, int Rm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    u64 base;
    u64 addr;
    preg mask = zx->p[Pg];
    s64 offset = (s64)Xreg_get(zx, Rm, X31ZERO);
    int mbytes = esize / 8;
    assert(nreg <= 4);
    zreg values[4] = {zreg_zeros, zreg_zeros, zreg_zeros, zreg_zeros};
    base = Xreg_get(zx, Rn, X31SP);
    for (int r = 0; r < nreg; r++) {
	values[r] = zx->z[(Zt + r) % 32];
    }
    for (int e = 0; e < elements; e++) {
	addr = (u64)((s64)base + offset * mbytes);
	for (int r = 0; r < nreg; r++) {
	    if (ElemP_get(zx, &mask, e, esize) == 1) {
		u64 data = Elem_get(zx, &values[r], e, esize, DONTCARE);
		Mem_wr(zx, addr, mbytes, data, AccType_NORMAL);
	    }
	    addr = (u64)((s64)addr + mbytes);
	}
	offset = offset + nreg;
    }
}

static void
perform_ST234_x_imm(svecxt_t *zx, int esize, int nreg,
		    int Zt, int Rn, s64 offset, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    u64 base;
    u64 addr;
    preg mask = zx->p[Pg];
    int mbytes = esize / 8;
    assert(nreg <= 4);
    zreg values[4] = {zreg_zeros, zreg_zeros, zreg_zeros, zreg_zeros};
    base = Xreg_get(zx, Rn, X31SP);

    for (int r = 0; r < nreg; r++) {
	values[r] = zx->z[(Zt + r) % 32];
    }
    addr = (u64)((s64)base + offset * elements * nreg * mbytes);
    for (int e = 0; e < elements; e++) {
	for (int r = 0; r < nreg; r++) {
	    if (ElemP_get(zx, &mask, e, esize) == 1) {
		u64 data = Elem_get(zx, &values[r], e, esize, DONTCARE);
		Mem_wr(zx, addr, mbytes, data, AccType_NORMAL);
	    }
	    addr = (u64)((s64)addr + mbytes);
	}
    }
}

static void
perform_STNT_x_x(svecxt_t *zx, int esize, int msize,
		 int Zt, int Rn, int Rm, int Pg)
{
    perform_ST1_x_x_mode(zx, esize, msize, Zt, Rn, Rm, Pg, AccType_STREAM);
}

static void
perform_STNT_x_imm(svecxt_t *zx, int esize,
		   int Zt, int Rn, s64 offset, int Pg)
{
    int msize = esize;
    perform_ST1_x_imm_mode(zx, esize, msize,
			   Zt, Rn, offset, Pg, AccType_STREAM);
}

static void
perform_STR_p(svecxt_t *zx, int Pt, int Rn, s64 imm)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->PL / 8;
    preg src;
    u64 base;
    s64 offset = imm * elements;
    base = Xreg_get(zx, Rn, X31SP);
    src = zx->p[Pt];
    CheckAlignment((u64)((s64)base + offset), 2, AccType_NORMAL);
    for (int e = 0; e < elements; e++) {
	u64 data;
	data = 0;
	for (int b = 0; b < 8; b++) {
	    if (ElemP_get(zx, &src, ((8 * e) + b), 8) == 1) {
		data |= (1UL << b);
	    }
	}
	Mem_wr(zx, (u64)((s64)base + offset), 1, data, AccType_NORMAL);
	offset = offset + 1;
    }
}

static void
perform_STR_z(svecxt_t *zx, int Zt, int Rn, s64 imm)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / 8;
    zreg src = zreg_zeros;
    u64 base;
    s64 offset = imm * elements;
    base = Xreg_get(zx, Rn, X31SP);
    src = zx->z[Zt];
    CheckAlignment((u64)((s64)base + offset), 16, AccType_NORMAL);
    for (int e = 0; e < elements; e++) {
	u64 data = Elem_get(zx, &src, e, 8, DONTCARE);
	Mem_wr(zx, (u64)((s64)base + offset), 1, data, AccType_NORMAL);
	offset = offset + 1;
    }
}

/* Reverses predicate elements. */

static void
perform_VECTOR_REVERSE_p(svecxt_t *zx, int esize, int Pd, int Pn)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    preg operand = zx->p[Pn];
    preg result = reverse_preg(zx, esize, operand);
    zx->p[Pd] = result;
}

/* Reverses vector elements. */

static void
perform_VECTOR_REVERSE_z(svecxt_t *zx, int esize, int Zd, int Zn)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    zreg operand = zx->z[Zn];
    zreg result = reverse_zreg(zx, esize, operand);
    zx->z[Zd] = result;
}

static void
perform_ZIP_p(svecxt_t *zx, int esize, int part,
	      int Pn, int Pm, int Pd)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int pairs = zx->VL / (esize * 2);
    preg operand1 = zx->p[Pn];
    preg operand2 = zx->p[Pm];
    preg result = preg_zeros;
    int base = part * pairs;
    for (int p = 0; p < pairs; p++) {
	bool e0 = ElemP_get(zx, &operand1, (base + p), esize);
	ElemP_set(zx, &result, (2 * p + 0), esize, e0);
	bool e1 = ElemP_get(zx, &operand2, (base + p), esize);
	ElemP_set(zx, &result, (2 * p + 1), esize, e1);
    }
    zx->p[Pd] = result;
}

static void
perform_ZIP_z(svecxt_t *zx, int esize, int part,
	      int Zn, int Zm, int Zd)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int pairs = zx->VL / (esize * 2);
    zreg operand1 = zx->z[Zn];
    zreg operand2 = zx->z[Zm];
    zreg result = zreg_zeros;
    int base = part * pairs;
    for (int p = 0; p < pairs; p++) {
	u64 e0 = Elem_get(zx, &operand1, (base + p), esize, DONTCARE);
	Elem_set(zx, &result, (2 * p + 0), esize, e0);
	u64 e1 = Elem_get(zx, &operand2, (base + p), esize, DONTCARE);
	Elem_set(zx, &result, (2 * p + 1), esize, e1);
    }
    zx->z[Zd] = result;
}

static void
perform_UNZIP_p(svecxt_t *zx, int esize, int part, int Pm, int Pn, int Pd)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    preg operand1 = zx->p[Pn];
    preg operand2 = zx->p[Pm];
    preg result = preg_zeros;
    preg zipped[2] = {operand1, operand2};
    for (int e = 0; e < elements; e++) {
	int pos0 = ((e < (elements / 2)) ? 0 : 1);
	int pos1 = ((e < (elements / 2)) ? (2 * e) : (2 * e - elements));
	bool p = ElemP_get(zx, &zipped[pos0], (pos1 + part), esize);
	ElemP_set(zx, &result, e, esize, p);
    }
    zx->p[Pd] = result;
}

static void
perform_UNZIP_z(svecxt_t *zx, int esize, int part, int Zd, int Zn, int Zm)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    zreg operand1 = zx->z[Zn];
    zreg operand2 = zx->z[Zm];
    zreg result = zreg_zeros;
    zreg zipped[2] = {operand1, operand2};
    for (int e = 0; e < elements; e++) {
	int pos0 = ((e < (elements / 2)) ? 0 : 1);
	int pos1 = ((e < (elements / 2)) ? (2 * e) : (2 * e - elements));
	u64 v = Elem_get(zx, &zipped[pos0], (pos1 + part), esize, DONTCARE);
	Elem_set(zx, &result, e, esize, v);
    }
    zx->z[Zd] = result;
}

static void
perform_UNPACK_p(svecxt_t *zx, int esize, int hi,
		 int Pn, int Pd)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    assert(esize >= 16);
    int elements = zx->VL / esize;
    preg operand = zx->p[Pn];
    preg result = preg_zeros;
    for (int e = 0; e < elements; e++) {
	int pos = (hi ? (e + elements) : e);
	bool p = ElemP_get(zx, &operand, pos, (esize / 2));
	ElemP_set(zx, &result, e, esize, p);
    }
    zx->p[Pd] = result;
}

/* Extracts half-sized elements (high-half/low-half) to a full-sized
   elements. */

static void
perform_UNPACK_z(svecxt_t *zx, int esize, int hi, bool unsignedp,
		 int Zn, int Zd)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    int hsize = esize / 2;
    zreg operand = zx->z[Zn];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	u64 element;
	if (hi) {
	    element = Elem_get(zx, &operand, (e + elements), hsize, unsignedp);
	} else{
	    element = Elem_get(zx, &operand, e, hsize, unsignedp);
	}
	Elem_set(zx, &result, e, esize, element);
    }
    zx->z[Zd] = result;
}

static void
perform_PTRUE_p(svecxt_t *zx, int esize, int Pd, int pat, bool setflags)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    int count = DecodePredCount(zx, pat, esize);
    preg result = preg_zeros;
    for (int e = 0; e < elements; e++) {
	ElemP_set(zx, &result, e, esize, (e < count));
    }
    if (setflags) {
	NZCV_set(zx, PredTest(zx, &result, &result, esize));
    }
    zx->p[Pd] = result;
}

/* Scans a predicate vector with and-not.  BEFORE means
   inclusive-scan.  When merging, masked entries are unmodified.  When
   the register number Pm!=99, scan continues from the last value in
   P[Pm].  Note the pseudo-codes for BRK[A,B][-,S] and BRKP[A,B][-,S]
   use the different flags whose polarity is inverted (BREAK and
   LAST).  This code uses LAST. */

static void
perform_BREAK_p(svecxt_t *zx, int esize, bool merging, bool setflags,
		int break_before0_after1,
		int Pd, int Pmn, int Pprevious, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    bool propagating = (Pprevious != 99);
    assert(break_before0_after1 == 0 || break_before0_after1 == 1);
    assert(!propagating || (!merging && esize == 8 && Pprevious != 99));
    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    preg operand_last = (propagating ? zx->p[Pprevious] : preg_zeros);
    preg operand = zx->p[Pmn];
    preg operand_merge = zx->p[Pd];
    bool last;
    if (propagating) {
	assert(!merging && esize == 8 && Pprevious != 99);
	last = (LastActive(zx, &mask, &operand_last, 8) == 1);
    } else {
	last = true;
    }
    preg result = preg_zeros;
    for (int e = 0; e < elements; e++) {
	bool element = (ElemP_get(zx, &operand, e, esize) == 1);
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    if (break_before0_after1 == 0) {
		last = last && !element;
	    }
	    ElemP_set(zx, &result, e, esize, (last ? true : false));
	    if (break_before0_after1 == 1) {
		last = last && !element;
	    }
	} else if (merging) {
	    bool p = ElemP_get(zx, &operand_merge, e, esize);
	    ElemP_set(zx, &result, e, esize, p);
	} else {
	    ElemP_set(zx, &result, e, esize, false);
	}
    }
    if (setflags) {
	NZCV_set(zx, PredTest(zx, &mask, &result, esize));
    }
    zx->p[Pd] = result;
}

static void
perform_BREAK_NEXT_p(svecxt_t *zx, int esize, bool setflags,
		     int Pd, int Pn, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    assert(esize == 8);
    preg mask = zx->p[Pg];
    preg operand1 = zx->p[Pn];
    preg operand2 = zx->p[Pd];
    preg result = preg_zeros;
    if (LastActive(zx, &mask, &operand1, 8) == 1) {
	result = operand2;
    } else {
	result = preg_zeros;
    }
    if (setflags) {
	preg ones = preg_all_ones;
	NZCV_set(zx, PredTest(zx, &ones, &result, 8));
    }
    zx->p[Pd] = result;
}

/* Extracts a last active element.  When CONDITIONAL, it keeps the old
   value if there are no active elements.  MEMO: csize/rsize is
   unused (it is 32 or 64 for X-registers). */

static void
perform_LAST_xv(svecxt_t *zx, int esize, bool conditional,
		bool isBefore, enum xvregset X0V1, int Rdn, int Zm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    u64 opr = (X0V1 == XREG) ? Xreg_get(zx, Rdn, X31ZERO) : Vreg_get(zx, Rdn);
    u64 operand1 = sign_extend(opr, esize, U64EXT);
    zreg operand2 = zx->z[Zm];
    u64 result;
    int last = LastActiveElement(zx, &mask, esize);
    if (last < 0) {
	if (conditional) {
	    result = operand1;
	} else {
	    if (isBefore) {
		last = elements - 1;
	    } else {
		last = 0;
	    }
	    result = Elem_get(zx, &operand2, last, esize, U64EXT);
	}
    } else {
	if (!isBefore) {
	    last = last + 1;
	    if (last >= elements) {
		last = 0;
	    }
	}
	result = Elem_get(zx, &operand2, last, esize, U64EXT);
    }
    if (X0V1 == XREG) {
	Xreg_set(zx, Rdn, result);
    } else {
	Vreg_set(zx, Rdn, result);
    }
}

/* Extracts a last active element and duplicates it. */

static void
perform_LAST_z(svecxt_t *zx, int esize, bool isBefore,
	       int Zd, int Zm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg operand1 = zx->z[Zd];
    zreg operand2 = zx->z[Zm];
    zreg result = zreg_zeros;
    int last = LastActiveElement(zx, &mask, esize);
    if (last < 0) {
	result = operand1;
    } else {
	if (!isBefore) {
	    last = last + 1;
	    if (last >= elements) {
		last = 0;
	    }
	}
	u64 v = Elem_get(zx, &operand2, last, esize, DONTCARE);
	for (int e = 0; e < elements; e++) {
	    Elem_set(zx, &result, e, esize, v);
	}
    }
    zx->z[Zd] = result;
}

/* Gets the element count for the element size.  The count is
   constrained by the pattern argument. */

static void
perform_ELEMENT_COUNT(svecxt_t *zx, int esize, int Rd, int pat, u64 imm)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int count = DecodePredCount(zx, pat, esize);
    Xreg_set(zx, Rd, ((u64)count * imm));
}

static void
perform_TERM_x(svecxt_t *zx, int esize, SVECmp op, int Rn, int Rm)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    u64 element1 = Xreg_get(zx, Rn, X31ZERO);
    u64 element2 = Xreg_get(zx, Rm, X31ZERO);
    bool term;
    switch (op) {
    case Cmp_EQ:
	term = (element1 == element2);
	break;
    case Cmp_NE:
	term = (element1 != element2);
	break;
    default:
	assert(op == Cmp_EQ|| op == Cmp_NE);
	abort();
    }
    if (term) {
	bool4 nzcv = NZCV_get(zx);
	nzcv.n = 1;
	nzcv.v = 0;
	NZCV_set(zx, nzcv);
    } else {
	bool4 nzcv = NZCV_get(zx);
	nzcv.n = 0;
	nzcv.v = (! nzcv.c);
	NZCV_set(zx, nzcv);
    }
}

static bool
compare_i64(SVECmp op, bool signedp, u64 x, u64 y)
{
    if (signedp == S64EXT) {
	switch (op) {
	case Cmp_EQ: return ((s64)x == (s64)y);
	case Cmp_NE: return ((s64)x != (s64)y);
	case Cmp_GE: return ((s64)x >= (s64)y);
	case Cmp_LT: return ((s64)x < (s64)y);
	case Cmp_GT: return ((s64)x > (s64)y);
	case Cmp_LE: return ((s64)x <= (s64)y);
	default:
	    assert(op == Cmp_EQ || op == Cmp_NE
		   || op == Cmp_GE || op == Cmp_LT
		   || op == Cmp_GT || op == Cmp_LE);
	    abort();
	}
    } else {
	switch (op) {
	case Cmp_EQ: return ((u64)x == (u64)y);
	case Cmp_NE: return ((u64)x != (u64)y);
	case Cmp_GE: return ((u64)x >= (u64)y);
	case Cmp_LT: return ((u64)x < (u64)y);
	case Cmp_GT: return ((u64)x > (u64)y);
	case Cmp_LE: return ((u64)x <= (u64)y);
	default:
	    assert(op == Cmp_EQ || op == Cmp_NE
		   || op == Cmp_GE || op == Cmp_LT
		   || op == Cmp_GT || op == Cmp_LE);
	    abort();
	}
    }
}

static inline void
perform_WHILE_p(svecxt_t *zx, int esize, int rsize,
		bool unsignedp, SVECmp cmp, int Rn, int Rm, int Pd)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    assert(cmp == Cmp_GE || cmp == Cmp_LT || cmp == Cmp_GT || cmp == Cmp_LE);
    int elements = zx->VL / esize;
    preg mask = preg_all_ones;
    u64 operand1 = sign_extend(Xreg_get(zx, Rn, X31ZERO), rsize, unsignedp);
    u64 operand2 = sign_extend(Xreg_get(zx, Rm, X31ZERO), rsize, unsignedp);
    preg result = preg_zeros;
    bool last;
    last = true;
    for (int e = 0; e < elements; e++) {
	bool cond = compare_i64(cmp, unsignedp, operand1, operand2);
	last = last && cond;
	ElemP_set(zx, &result, e, esize, (last ? 1 : 0));
	if (cmp == Cmp_GE || cmp == Cmp_GT) {
	    operand1 = operand1 - 1;
	} else {
	    operand1 = operand1 + 1;
	}
    }
    NZCV_set(zx, PredTest(zx, &mask, &result, esize));
    zx->p[Pd] = result;
}

static void
perform_ICMP_z_z_imm(svecxt_t *zx, int esize, SVECmp op, bool unsignedp,
		     bool z0imm1, int Pd, int Zn, int Zm, s64 imm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    assert(!(z0imm1 == false) || imm == (s64)0);
    assert(!(z0imm1 == true) || Zm == 99);
    assert(op == Cmp_EQ || op == Cmp_NE
	   || op == Cmp_GE || op == Cmp_LT
	   || op == Cmp_GT || op == Cmp_LE);

    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg operand1 = zx->z[Zn];
    zreg operand2 = ((z0imm1 == false) ? zx->z[Zm] : zreg_zeros);
    preg result = preg_zeros;
    for (int e = 0; e < elements; e++) {
	u64 element1 = Elem_get(zx, &operand1, e, esize, unsignedp);
	u64 element2;
	if (z0imm1 == false) {
	    element2 = Elem_get(zx, &operand2, e, esize, unsignedp);
	} else {
	    element2 = (u64)imm;
	}
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    bool cond = compare_i64(op, unsignedp, element1, element2);
	    ElemP_set(zx, &result, e, esize, cond);
	} else {
	    ElemP_set(zx, &result, e, esize, 0);
	}
    }
    NZCV_set(zx, PredTest(zx, &mask, &result, esize));
    zx->p[Pd] = result;
}

static void
perform_ICMP_z_z(svecxt_t *zx, int esize, SVECmp op, bool unsignedp,
		 int Pd, int Zn, int Zm, int Pg)
{
    perform_ICMP_z_z_imm(zx, esize, op, unsignedp,
			 false, Pd, Zn, Zm, 0, Pg);
}

static void
perform_ICMP_z_imm(svecxt_t *zx, int esize, SVECmp op, bool unsignedp,
		   int Pd, int Zn, s64 imm, int Pg)
{
    perform_ICMP_z_z_imm(zx, esize, op, unsignedp,
			 true, Pd, Zn, 99, imm, Pg);
}

static void
perform_ICMP_z_z_wide2nd(svecxt_t *zx, int esize, SVECmp op, bool unsignedp,
			 int Pd, int Zn, int Zm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    assert(op == Cmp_EQ || op == Cmp_NE
	   || op == Cmp_GE || op == Cmp_LT
	   || op == Cmp_GT || op == Cmp_LE);

    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg operand1 = zx->z[Zn];
    zreg operand2 = zx->z[Zm];
    preg result = preg_zeros;
    for (int e = 0; e < elements; e++) {
	int e2 = ((e * esize) / 64);
	u64 element1 = Elem_get(zx, &operand1, e, esize, unsignedp);
	u64 element2 = Elem_get(zx, &operand2, e2, 64, unsignedp);
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    bool cond = compare_i64(op, unsignedp, element1, element2);
	    ElemP_set(zx, &result, e, esize, cond);
	} else {
	    ElemP_set(zx, &result, e, esize, 0);
	}
    }
    NZCV_set(zx, PredTest(zx, &mask, &result, esize));
    zx->p[Pd] = result;
}

/* The unary operations (NEG/NOT/ZEROP/ABS) work on the Y operand,
   ignoring the X.  The all are predicated. */

static u64
calculate_iop(int esize, Iop op, u64 x, u64 y)
{
    u64 v;
    switch (op) {
    case Iop_AND: v = (x & y); break;
    case Iop_IOR: v = (x | y); break;
    case Iop_XOR: v = (x ^ y); break;
    case Iop_ANDN: v = (x & ~y); break;

    case Iop_ADD: v = (x + y); break;
    case Iop_SUB: v = (x - y); break;
    case Iop_SUB_REV: v = (y - x); break;
    case Iop_MUL: v = (x * y); break;
    case Iop_SDIV: v = (u64)((s64)x / (s64)y); break;
    case Iop_UDIV: v = ((u64)x / (u64)y); break;
    case Iop_SDIV_REV: v = (u64)((s64)y / (s64)x); break;
    case Iop_UDIV_REV: v = ((u64)y / (u64)x); break;
    case Iop_SMIN: v = (((s64)x <= (s64)y) ? x : y); break;
    case Iop_UMIN: v = (((u64)x <= (u64)y) ? x : y); break;
    case Iop_SMAX: v = (((s64)x <= (s64)y) ? y : x); break;
    case Iop_UMAX: v = (((u64)x <= (u64)y) ? y : x); break;
    case Iop_SDIFF: v = (((s64)x <= (s64)y) ? (y - x) : (x - y)); break;
    case Iop_UDIFF: v = (((u64)x <= (u64)y) ? (y - x) : (x - y)); break;

    case Iop_NEG: v = (u64)(-(s64)y); break;
    case Iop_NOT: v = (u64)(~y); break;
    case Iop_ZEROP: v = (u64)((y == 0) ? 1 : 0); break;
    case Iop_ABS: v = (u64)(((s64)y >= 0) ? (s64)y : -(s64)y); break;
    case Iop_BITREVERSE: v = BitReverse(y); break;
    case Iop_POPC: v = (u64)(u32)popc(y); break;
    case Iop_CLZ: v = (u64)(u32)CountLeadingZeroBits(esize, y); break;
    case Iop_CLS: v = (u64)(u32)CountLeadingSignBits(esize, y); break;

    case Iop_ASH_R: v = (u64)((s64)x >> y); break;
    case Iop_ASH_R_REV: v = (u64)((s64)y >> x); break;
    case Iop_ASH_DIV: {
	/* Offset for rounding toward zero. */
	u64 x1 = (((s64)x >= 0) ? x : (x + (u64)((1 << y) - 1)));
	v = (u64)((s64)x1 >> y);
	break;
    }
    case Iop_LSH_L: v = (u64)((u64)x << y); break;
    case Iop_LSH_L_REV: v = (u64)((u64)y << x); break;
    case Iop_LSH_R: v = (u64)((u64)x >> y); break;
    case Iop_LSH_R_REV: v = (u64)((u64)y >> x); break;

    default:
	assert(op == Iop_AND || op == Iop_IOR || op == Iop_XOR
	       || op == Iop_ANDN
	       || op == Iop_ADD || op == Iop_SUB || op == Iop_SUB_REV
	       || op == Iop_MUL
	       || op == Iop_SDIV || op == Iop_UDIV
	       || op == Iop_SDIV_REV || op == Iop_UDIV_REV
	       || op == Iop_SMIN || op == Iop_UMIN
	       || op == Iop_SMAX || op == Iop_UMAX
	       || op == Iop_SDIFF || op == Iop_UDIFF
	       || op == Iop_NEG || op == Iop_NOT
	       || op == Iop_ZEROP || op == Iop_ABS
	       || op == Iop_BITREVERSE
	       || op == Iop_ASH_R || op == Iop_ASH_DIV
	       || op == Iop_LSH_L || op == Iop_LSH_L_REV
	       || op == Iop_LSH_R || op == Iop_LSH_R_REV);
	abort();
    }
    return v;
}

static bool
calculate_boolean_op(Iop op, u64 x, u64 y)
{
    bool v;
    switch (op) {
    case Iop_AND: v = (x && y); break;
    case Iop_IOR: v = (x || y); break;
    case Iop_XOR: v = (x != y); break;
    case Iop_NAND: v = !(x && y); break;
    case Iop_NOR: v = !(x || y); break;
    case Iop_ANDN: v = (x && !y); break;
    case Iop_IORN: v = (x || !y); break;
    default:
	assert(op == Iop_AND || op == Iop_IOR || op == Iop_XOR
	       || op == Iop_NAND || op == Iop_NOR || op == Iop_ANDN
	       || op == Iop_IORN);
	abort();
    }
    return v;
}

static double
sqrt_df(double x)
{
    /*NEON*/
    float64x2_t v0 = vdupq_n_f64(x);
    float64x2_t v1 = vsqrtq_f64(v0);
    double z = vgetq_lane_f64(v1, 0);
    return z;
}

static float
sqrt_sf(float x)
{
    /*NEON*/
    float32x2_t v0 = vdup_n_f32(x);
    float32x2_t v1 = vsqrt_f32(v0);
    float z = vget_lane_f32(v1, 0);
    return z;
}

/* MEMO: C99 fmax()/fmin() ignores a nan (on a single side). */

static double
max_df(double x, double y)
{
    if (isnan(x) || isnan(y)) {
	return nan("");
    } else {
	return ((x >= y) ? x : y);
    }
}

static float
max_sf(float x, float y)
{
    if (isnan(x) || isnan(y)) {
	return nanf("");
    } else {
	return ((x >= y) ? x : y);
    }
}

static double
min_df(double x, double y)
{
    if (isnan(x) || isnan(y)) {
	return nan("");
    } else {
	return ((x <= y) ? x : y);
    }
}

static float
min_sf(float x, float y)
{
    if (isnan(x) || isnan(y)) {
	return nanf("");
    } else {
	return ((x <= y) ? x : y);
    }
}

static double
max_df_ignore_nan(double x, double y)
{
    if (isnan(x) && isnan(y)) {
	return nan("");
    } else if (isnan(x)) {
	return y;
    } else if (isnan(y)) {
	return x;
    } else {
	return max_df(x, y);
    }
}

static double
min_df_ignore_nan(double x, double y)
{
    if (isnan(x) && isnan(y)) {
	return nan("");
    } else if (isnan(x)) {
	return y;
    } else if (isnan(y)) {
	return x;
    } else {
	return min_df(x, y);
    }
}

static float
max_sf_ignore_nan(float x, float y)
{
    if (isnan(x) && isnan(y)) {
	return nanf("");
    } else if (isnan(x)) {
	return y;
    } else if (isnan(y)) {
	return x;
    } else {
	return max_sf(x, y);
    }
}

static float
min_sf_ignore_nan(float x, float y)
{
    if (isnan(x) && isnan(y)) {
	return nanf("");
    } else if (isnan(x)) {
	return y;
    } else if (isnan(y)) {
	return x;
    } else {
	return min_sf(x, y);
    }
}

/* Unary operators takes the 2nd argument. */

static u64
calculate_fop(int esize, Fop op, u64 ux, u64 uy)
{
    switch (esize) {
    case 32: {
	float x = u64_as_float(ux);
	float y = u64_as_float(uy);
	switch (op) {
	case Fop_ADD: return float_as_u64(x + y);
	case Fop_SUB: return float_as_u64(x - y);
	case Fop_SUB_REV: return float_as_u64(y - x);
	case Fop_MUL: return float_as_u64(x * y);
	case Fop_DIV: return float_as_u64(x / y);
	case Fop_DIV_REV: return float_as_u64(y / x);
	case Fop_MAX: return float_as_u64(max_sf(x, y));
	case Fop_MIN: return float_as_u64(min_sf(x, y));
	case Fop_MAXNUM: return float_as_u64(max_sf_ignore_nan(x, y));
	case Fop_MINNUM: return float_as_u64(min_sf_ignore_nan(x, y));
	case Fop_NEG: return float_as_u64(-y);
	case Fop_ABS: return FPAbs(esize, uy);
	case Fop_SQRT: return float_as_u64(sqrt_sf(y));
	case Fop_RECPE: return float_as_u64(/*NEON*/ vrecpes_f32(y));
	case Fop_RECPS: return float_as_u64(/*NEON*/ vrecpss_f32(x, y));
	case Fop_RSQRTE: return float_as_u64(/*NEON*/ vrsqrtes_f32(y));
	case Fop_RSQRTS: return float_as_u64(/*NEON*/ vrsqrtss_f32(x, y));

	case Fop_DIFF: return FPAbs(esize, float_as_u64(x - y));
	case Fop_CPY: return uy;

	case Fop_CVT_16_32: return FPConvert(uy, 16, 32);
	case Fop_CVT_16_64: assert(esize != 32); abort(); break;
	case Fop_CVT_32_16: return FPConvert(uy, 32, 16);
	case Fop_CVT_32_64: assert(esize != 32); abort(); break;
	case Fop_CVT_64_16: assert(esize != 32); abort(); break;
	case Fop_CVT_64_32: assert(esize != 32); abort(); break;

	case Fop_CVTI_32_S32: return FPToFixed(uy, S64EXT, 32, 32);
	case Fop_CVTI_32_S64: assert(esize != 32); abort(); break;
	case Fop_CVTI_64_S32: assert(esize != 32); abort(); break;
	case Fop_CVTI_64_S64: assert(esize != 32); abort(); break;

	case Fop_CVTI_32_U32: return FPToFixed(uy, U64EXT, 32, 32);
	case Fop_CVTI_32_U64: assert(esize != 32); abort(); break;
	case Fop_CVTI_64_U32: assert(esize != 32); abort(); break;
	case Fop_CVTI_64_U64: assert(esize != 32); abort(); break;

	case Fop_CVTF_S32_32: return FixedToFP(uy, S64EXT, 32, 32);
	case Fop_CVTF_S32_64: assert(esize != 32); abort(); break;
	case Fop_CVTF_S64_32: assert(esize != 32); abort(); break;
	case Fop_CVTF_S64_64: assert(esize != 32); abort(); break;

	case Fop_CVTF_U32_32: return FixedToFP(uy, U64EXT, 32, 32);
	case Fop_CVTF_U32_64: assert(esize != 32); abort(); break;
	case Fop_CVTF_U64_32: assert(esize != 32); abort(); break;
	case Fop_CVTF_U64_64: assert(esize != 32); abort(); break;

	default:
	    assert(op == Fop_ADD || op == Fop_DIFF);
	    abort();
	}
    }
    case 64: {
	double x = u64_as_double(ux);
	double y = u64_as_double(uy);
	switch (op) {
	case Fop_ADD: return double_as_u64(x + y);
	case Fop_SUB: return double_as_u64(x - y);
	case Fop_SUB_REV: return double_as_u64(y - x);
	case Fop_MUL: return double_as_u64(x * y);
	case Fop_DIV: return double_as_u64(x / y);
	case Fop_DIV_REV: return double_as_u64(y / x);
	case Fop_MAX: return double_as_u64(max_df(x, y));
	case Fop_MIN: return double_as_u64(min_df(x, y));
	case Fop_MAXNUM: return double_as_u64(max_df_ignore_nan(x, y));
	case Fop_MINNUM: return double_as_u64(min_df_ignore_nan(x, y));
	case Fop_NEG: return double_as_u64(-y);
	case Fop_ABS: return FPAbs(esize, uy);
	case Fop_SQRT: return double_as_u64(sqrt_df(y));
	case Fop_RECPE: return double_as_u64(/*NEON*/ vrecped_f64(y));
	case Fop_RECPS: return double_as_u64(/*NEON*/ vrecpsd_f64(x, y));
	case Fop_RSQRTE: return double_as_u64(/*NEON*/ vrsqrted_f64(y));
	case Fop_RSQRTS: return double_as_u64(/*NEON*/ vrsqrtsd_f64(x, y));

	case Fop_DIFF: return FPAbs(esize, double_as_u64(x - y));
	case Fop_CPY: return uy;

	case Fop_CVT_16_32: assert(esize != 64); abort(); break;
	case Fop_CVT_16_64: return FPConvert(uy, 16, 64);
	case Fop_CVT_32_16: assert(esize != 64); abort(); break;
	case Fop_CVT_32_64: return FPConvert(uy, 32, 64);
	case Fop_CVT_64_16: return FPConvert(uy, 64, 16);
	case Fop_CVT_64_32: return FPConvert(uy, 64, 32);

	case Fop_CVTI_32_S32: assert(esize != 64); abort(); break;
	case Fop_CVTI_32_S64: return FPToFixed(uy, S64EXT, 32, 64);
	case Fop_CVTI_64_S32: return FPToFixed(uy, S64EXT, 64, 32);
	case Fop_CVTI_64_S64: return FPToFixed(uy, S64EXT, 64, 64);

	case Fop_CVTI_32_U32: assert(esize != 64); abort(); break;
	case Fop_CVTI_32_U64: return FPToFixed(uy, U64EXT, 32, 64);
	case Fop_CVTI_64_U32: return FPToFixed(uy, U64EXT, 64, 32);
	case Fop_CVTI_64_U64: return FPToFixed(uy, U64EXT, 64, 64);

	case Fop_CVTF_S32_32: assert(esize != 64); abort(); break;
	case Fop_CVTF_S32_64: return FixedToFP(uy, S64EXT, 32, 64);
	case Fop_CVTF_S64_32: return FixedToFP(uy, S64EXT, 64, 32);
	case Fop_CVTF_S64_64: return FixedToFP(uy, S64EXT, 64, 64);

	case Fop_CVTF_U32_32: assert(esize != 64); abort(); break;
	case Fop_CVTF_U32_64: return FixedToFP(uy, U64EXT, 32, 64);
	case Fop_CVTF_U64_32: return FixedToFP(uy, U64EXT, 64, 32);
	case Fop_CVTF_U64_64: return FixedToFP(uy, U64EXT, 64, 64);

	default:
	    assert(op == Fop_ADD || op == Fop_DIFF);
	    abort();
	}
    }
    default:
	assert(esize == 32 || esize == 64);
	abort();
    }
}

static void
perform_INC_x(svecxt_t *zx, int esize, int Rdn, int pat, s64 imm)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int count = DecodePredCount(zx, pat, esize);
    u64 operand1 = Xreg_get(zx, Rdn, X31ZERO);
    Xreg_set(zx, Rdn, (u64)((s64)operand1 + (count * imm)));
}

static void
perform_INC_z(svecxt_t *zx, int esize, int Zdn, int pat, s64 imm)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    int count = DecodePredCount(zx, pat, esize);
    zreg operand1 = zx->z[Zdn];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	u64 v = Elem_get(zx, &operand1, e, esize, DONTCARE);
	Elem_set(zx, &result, e, esize, (u64)((s64)v + (count * imm)));
    }
    zx->z[Zdn] = result;
}

static void
perform_INC_x_pred(svecxt_t *zx, int esize, int Rdn, int Pg, int increment)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    assert(increment == 1 || increment == -1);
    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    u64 operand = Xreg_get(zx, Rdn, X31ZERO);
    int count = 0;
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    count = count + increment;
	}
    }
    Xreg_set(zx, Rdn, (u64)((s64)operand + count));
}

static void
perform_INC_z_pred(svecxt_t *zx, int esize, int Zdn, int Pg, int increment)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg operand = zx->z[Zdn];
    zreg result = zreg_zeros;
    int count = 0;
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    count = count + increment;
	}
    }
    for (int e = 0; e < elements; e++) {
	u64 v = Elem_get(zx, &operand, e, esize, DONTCARE);
	Elem_set(zx, &result, e, esize, (u64)((s64)v + count));
    }
    zx->z[Zdn] = result;
}

static void
perform_ADR(svecxt_t *zx, int esize, int osize, int mbytes, bool unsignedp,
	    int Zn, int Zm, int Zd)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    assert(osize <= esize);
    int elements = zx->VL / esize;
    zreg base = zx->z[Zn];
    zreg offs = zx->z[Zm];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	u64 addr = Elem_get(zx, &base, e, esize, DONTCARE);
	u64 off0 = Elem_get(zx, &offs, e, esize, DONTCARE);
	s64 offset = (s64)sign_extend(off0, osize, unsignedp);
	Elem_set(zx, &result, e, esize, (u64)((s64)addr + (offset * mbytes)));
    }
    zx->z[Zd] = result;
}

/* Sign-extends. */

static void
perform_EXTEND(svecxt_t *zx, int esize, int src_esize, bool unsignedp,
	       int Zn, int Zd, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg operand = zx->z[Zn];
    zreg dest = zx->z[Zd];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	u64 element = Elem_get(zx, &operand, e, esize, unsignedp);
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    u64 v = sign_extend(element, src_esize, unsignedp);
	    Elem_set(zx, &result, e, esize, v);
	} else {
	    u64 v = Elem_get(zx, &dest, e, esize, DONTCARE);
	    Elem_set(zx, &result, e, esize, v);
	}
    }
    zx->z[Zd] = result;
}

static void
perform_IOP_z_wide2nd(svecxt_t *zx, int esize, bool wide2ndp, Iop op,
		      int Zd, int Zn, int Zm)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    enum signedness signedp = ((op == Iop_ASH_R) ? S64EXT : DONTCARE);

    int elements = zx->VL / esize;
    zreg operand1 = zx->z[Zn];
    zreg operand2 = zx->z[Zm];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	int e2 = ((!wide2ndp) ? e : ((e * esize) / 64));
	u64 element1 = Elem_get(zx, &operand1, e, esize, signedp);
	u64 element2 = Elem_get(zx, &operand2, e2, esize, DONTCARE);
	u64 v = calculate_iop(esize, op, element1, element2);
	Elem_set(zx, &result, e, esize, v);
    }
    zx->z[Zd] = result;
}

static void
perform_IOP_z(svecxt_t *zx, int esize, Iop op,
	      int Zd, int Zn, int Zm)
{
    perform_IOP_z_wide2nd(zx, esize, false, op, Zd, Zn, Zm);
}

static void
perform_IOP_z_imm(svecxt_t *zx, int esize, Iop op,
		  int Zd, int Zn, u64 imm)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    zreg operand1 = zx->z[Zn];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	u64 element1 = Elem_get(zx, &operand1, e, esize, DONTCARE);
	u64 element2 = imm;
	u64 v = calculate_iop(esize, op, element1, element2);
	Elem_set(zx, &result, e, esize, v);
    }
    zx->z[Zd] = result;
}

static void
perform_IOP_z_imm_pred(svecxt_t *zx, int esize, Iop op,
		       int Zdn, u64 imm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    assert(op == Iop_ASH_R);

    int elements = zx->VL / esize;
    zreg operand1 = zx->z[Zdn];
    preg mask = zx->p[Pg];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    u64 element1 = Elem_get(zx, &operand1, e, esize, DONTCARE);
	    u64 element2 = imm;
	    u64 v = calculate_iop(esize, op, element1, element2);
	    Elem_set(zx, &result, e, esize, v);
	} else {
	    u64 element1 = Elem_get(zx, &operand1, e, esize, DONTCARE);
	    Elem_set(zx, &result, e, esize, element1);
	}
    }
    zx->z[Zdn] = result;
}

static void
perform_IOP_z_pred_wide2nd(svecxt_t *zx, int esize, bool wide2ndp, Iop op,
			   int Zdn, int Zm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    enum signedness signedp = ((op == Iop_ASH_R) ? S64EXT : DONTCARE);

    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg operand1 = zx->z[Zdn];
    zreg operand2 = zx->z[Zm];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	int e2 = ((!wide2ndp) ? e : ((e * esize) / 64));
	u64 element1 = Elem_get(zx, &operand1, e, esize, signedp);
	u64 element2 = Elem_get(zx, &operand2, e2, esize, DONTCARE);
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    u64 v = calculate_iop(esize, op, element1, element2);
	    Elem_set(zx, &result, e, esize, v);
	} else {
	    u64 v = Elem_get(zx, &operand1, e, esize, DONTCARE);
	    Elem_set(zx, &result, e, esize, v);
	}
    }
    zx->z[Zdn] = result;
}

static void
perform_IOP_z_pred(svecxt_t *zx, int esize, Iop op,
		   int Zdn, int Zm, int Pg)
{
    perform_IOP_z_pred_wide2nd(zx, esize, false, op, Zdn, Zm, Pg);
}

/* Reverses subelements inside an element. */

static void
perform_IREVERSE(svecxt_t *zx, int esize, int swsize, int Zd, int Zn, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg operand = zx->z[Zn];
    zreg dest = zx->z[Zd];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    u64 element = Elem_get(zx, &operand, e, esize, DONTCARE);
	    u64 v = Reverse(element, esize, swsize);
	    Elem_set(zx, &result, e, esize, v);
	} else {
	    u64 v = Elem_get(zx, &dest, e, esize, DONTCARE);
	    Elem_set(zx, &result, e, esize, v);
	}
    }
    zx->z[Zd] = result;
}

static void
perform_LOP_p(svecxt_t *zx, int esize, bool setflags, Iop op,
	      int Pd, int Pn, int Pm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    preg operand1 = zx->p[Pn];
    preg operand2 = zx->p[Pm];
    preg result = preg_zeros;
    for (int e = 0; e < elements; e++) {
	bool element1 = ElemP_get(zx, &operand1, e, esize);
	bool element2 = ElemP_get(zx, &operand2, e, esize);
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    bool v = calculate_boolean_op(op, element1, element2);
	    ElemP_set(zx, &result, e, esize, v);
	} else {
	    ElemP_set(zx, &result, e, esize, 0);
	}
    }
    if (setflags) {
	NZCV_set(zx, PredTest(zx, &mask, &result, esize));
    }
    zx->p[Pd] = result;
}

static bool
compare_f64(SVECmp op, int esize , u64 ux, u64 uy)
{
    switch (esize) {
    case 32: {
	float x = u64_as_float(ux);
	float y = u64_as_float(uy);
	switch (op) {
	case Cmp_EQ: return (x == y);
	case Cmp_NE: return (x != y);
	case Cmp_GE: return isgreaterequal(x, y);
	case Cmp_LT: return isless(x, y);
	case Cmp_GT: return isgreater(x, y);
	case Cmp_LE: return islessequal(x, y);
	case Cmp_UN: return isunordered(x, y);
	default:
	    assert(op == Cmp_EQ || op == Cmp_NE
		   || op == Cmp_GE || op == Cmp_LT
		   || op == Cmp_GT || op == Cmp_LE
		   || op == Cmp_UN);
	    abort();
	}
    }
    case 64: {
	double x = u64_as_double(ux);
	double y = u64_as_double(uy);
	switch (op) {
	case Cmp_EQ: return (x == y);
	case Cmp_NE: return (x != y);
	case Cmp_GE: return isgreaterequal(x, y);
	case Cmp_LT: return isless(x, y);
	case Cmp_GT: return isgreater(x, y);
	case Cmp_LE: return islessequal(x, y);
	case Cmp_UN: return isunordered(x, y);
	default:
	    assert(op == Cmp_EQ || op == Cmp_NE
		   || op == Cmp_GE || op == Cmp_LT
		   || op == Cmp_GT || op == Cmp_LE
		   || op == Cmp_UN);
	    abort();
	}
    }
    default:
	assert(esize == 32 || esize == 64);
	abort();
    }
}

static void
perform_FCMP_z(svecxt_t *zx, int esize, SVECmp op, bool absolutep,
	       int Pd, int Zn, int Zm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    assert(esize == 32 || esize == 64);
    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg operand1 = zx->z[Zn];
    zreg operand2 = zx->z[Zm];
    preg result = preg_zeros;
    for (int e = 0; e < elements; e++) {
	u64 element1 = Elem_get(zx, &operand1, e, esize, DONTCARE);
	u64 element2 = Elem_get(zx, &operand2, e, esize, DONTCARE);
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    u64 e1 = (absolutep ? FPAbs(esize, element1) : element1);
	    u64 e2 = (absolutep ? FPAbs(esize, element2) : element2);
	    bool r = compare_f64(op, esize, e1, e2);
	    ElemP_set(zx, &result, e, esize, r);
	} else {
	    ElemP_set(zx, &result, e, esize, 0);
	}
    }
    zx->p[Pd] = result;
}

static void
perform_FCMP_zero(svecxt_t *zx, int esize, SVECmp op, bool absolutep,
		  int Pd, int Zn, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    assert(absolutep == false);
    assert(esize == 32 || esize == 64);
    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg operand1 = zx->z[Zn];
    preg result = preg_zeros;
    for (int e = 0; e < elements; e++) {
	u64 element1 = Elem_get(zx, &operand1, e, esize, DONTCARE);
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    bool r = compare_f64(op, esize, element1, 0ULL);
	    ElemP_set(zx, &result, e, esize, r);
	} else {
	    ElemP_set(zx, &result, e, esize, 0);
	}
    }
    zx->p[Pd] = result;
}

static void
perform_FOP_z(svecxt_t *zx, int esize, Fop op, bool predicatedp,
	      int Zd, int Zn, int Zm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    assert((predicatedp == (Zn == 99)) && (predicatedp == (Pg != 99)));
    int elements = zx->VL / esize;
    preg mask = (predicatedp ? zx->p[Pg] : preg_all_ones);
    zreg operand1 = (predicatedp ? zx->z[Zd] : zx->z[Zn]);
    zreg operand2 = zx->z[Zm];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	u64 element1 = Elem_get(zx, &operand1, e, esize, DONTCARE);
	u64 element2 = Elem_get(zx, &operand2, e, esize, DONTCARE);
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    u64 v = calculate_fop(esize, op, element1, element2);
	    Elem_set(zx, &result, e, esize, v);
	} else {
	    Elem_set(zx, &result, e, esize, element1);
	}
    }
    zx->z[Zd] = result;
}

static void
perform_FOP_imm(svecxt_t *zx, int esize, Fop op, bool predicatedp,
		int Zdn, u64 imm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    assert(predicatedp == (Pg != 99));
    int elements = zx->VL / esize;
    preg mask = (predicatedp ? zx->p[Pg] : preg_all_ones);
    zreg operand1 = zx->z[Zdn];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	u64 element1 = Elem_get(zx, &operand1, e, esize, DONTCARE);
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    u64 v = calculate_fop(esize, op, element1, imm);
	    Elem_set(zx, &result, e, esize, v);
	} else {
	    Elem_set(zx, &result, e, esize, element1);
	}
    }
    zx->z[Zdn] = result;
}

/* Zd := Za + sub_op(Zn * Zm) */

static void
perform_IFMA(svecxt_t *zx, int esize, int Zd,
	     int Za, bool sub_op, int Zn, int Zm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg oldvalue = zx->z[Zd];
    zreg operand1 = zx->z[Zn];
    zreg operand2 = zx->z[Zm];
    zreg operand3 = zx->z[Za];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	u64 element1 = Elem_get(zx, &operand1, e, esize, DONTCARE);
	u64 element2 = Elem_get(zx, &operand2, e, esize, DONTCARE);
	u64 element3 = Elem_get(zx, &operand3, e, esize, DONTCARE);
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    if (sub_op) {
		u64 v = (element3 - (element1 * element2));
		Elem_set(zx, &result, e, esize, v);
	    } else {
		u64 v = (element3 + (element1 * element2));
		Elem_set(zx, &result, e, esize, v);
	    }
	} else {
	    u64 v = Elem_get(zx, &oldvalue, e, esize, DONTCARE);
	    Elem_set(zx, &result, e, esize, v);
	}
    }
    zx->z[Zd] = result;
}

/* Zd := (op3_neg Za) + ((op1_neg Zn) * Zm) */

static void
perform_FFMA(svecxt_t *zx, int esize, int Zd,
	     bool op3_neg, int Za, bool op1_neg, int Zn, int Zm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg operand1 = zx->z[Zn];
    zreg operand2 = zx->z[Zm];
    zreg operand3 = zx->z[Za];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	assert(esize == 32 || esize == 64);
	u64 element1 = Elem_get(zx, &operand1, e, esize, DONTCARE);
	u64 element2 = Elem_get(zx, &operand2, e, esize, DONTCARE);
	u64 element3 = Elem_get(zx, &operand3, e, esize, DONTCARE);
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    if (op1_neg) {element1 = FPNeg(element1, esize);}
	    if (op3_neg) {element3 = FPNeg(element3, esize);}
	    u64 v = FPMulAdd(element3, element1, element2, esize, zx->FPCR);
	    Elem_set(zx, &result, e, esize, v);
	} else {
	    Elem_set(zx, &result, e, esize, element3);
	}
    }
    zx->z[Zd] = result;
}

static void
perform_FREDUCE_seq(svecxt_t *zx, int esize, Fop op, int Vd, int Zm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    u64 operand1 = Vreg_get(zx, Vd);
    zreg operand2 = zx->z[Zm];
    u64 result = operand1;
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    u64 element = Elem_get(zx, &operand2, e, esize, DONTCARE);
	    result = calculate_fop(esize, op, result, element);
	}
    }
    Vreg_set(zx, Vd, result);
}

/* This is Reduce() and ReducePredicated() in the definition. */

static u64
reduce_fop_rec(int esize, Fop op, u64 v[], int b, int n)
{
    assert(b >= 0 && powerof2p(n));
    if (n == 1) {
	return v[b];
    } else {
	int h = n / 2;
	u64 x = reduce_fop_rec(esize, op, v, b, h);
	u64 y = reduce_fop_rec(esize, op, v, (b + h), h);
	return calculate_fop(esize, op, x, y);
    }
}

static void
perform_FREDUCE_rec(svecxt_t *zx, int esize, Fop op, u64 unitv,
		    int Vd, int Zm, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg input = zx->z[Zm];
    int elements2p = CeilPow2(elements);
    u64 operands[elements2p];
    for (int e = 0; e < elements2p; e++) {
	if (e < elements && ElemP_get(zx, &mask, e, esize) == 1) {
	    operands[e] = Elem_get(zx, &input, e, esize, DONTCARE);
	} else {
	    operands[e] = unitv;
	}
    }
    u64 v = reduce_fop_rec(esize, op, operands, 0, elements2p);
    Vreg_set(zx, Vd, v);
}

static void
perform_IREDUCE_seq(svecxt_t *zx, int esize, Iop op,
		    enum signedness signedp, u64 unitv,
		    int Vd, int Zn, int Pg)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg operand = zx->z[Zn];
    u64 result;
    result = unitv;
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    u64 element = Elem_get(zx, &operand, e, esize, signedp);
	    result = calculate_iop(esize, op, result, element);
	}
    }
    Vreg_set(zx, Vd, result);
}

static void
perform_DOTP(svecxt_t *zx, int esize, bool indexed, enum signedness signedp,
	     int Zda, int Zn, int Zm, int index)
{
    assert(zx != 0 && zx->ux != 0);
    /*mcontext_t *ux = zx->ux;*/
    CheckSVEEnabled();

    /*vectors/indexed*/
    int elements = zx->VL / esize;
    int eltspersegment = 128 / esize;
    zreg operand1 = zx->z[Zn];
    zreg operand2 = zx->z[Zm];
    zreg operand3 = zx->z[Zda];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	int segmentbase = e - (e % eltspersegment);
	int s = (!indexed ? e : (segmentbase + index));
	u64 res = Elem_get(zx, &operand3, e, esize, signedp);
	for (int i = 0; i < 4; i++) {
	    u64 element1 = Elem_get(zx, &operand1, (4 * e + i),
				    (esize / 4), signedp);
	    u64 element2 = Elem_get(zx, &operand2, (4 * s + i),
				    (esize / 4), signedp);
	    res = res + element1 * element2;
	}
	Elem_set(zx, &result, e, esize, res);
    }
    zx->z[Zda] = result;
}

/* ================================================================ */

/* INSTRUCTIONS */

/* Instruction names consist of yasve_ + name + _0xopcode.  Note that
   QUALS defined in the instruction table in binutils is not
   sufficient to distinguish variants. */

static inline void yasve_abs_0x0416a000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_ABS, Zd, Zn, Pg);
}
static inline void yasve_add_0x04200000 (CTXARG, int Zd, int Zn, int Zm) {
    /* (vectors, unpredicated) */
    //NOTTESTED00();
    int esize = (8 << size);
    perform_IOP_z(zx, esize, Iop_ADD, Zd, Zn, Zm);
}
static inline void yasve_add_0x2520c000 (CTXARG, int Zdn, int _Zd, s64 imm0) {
    /* (immediate) */
    //NOTTESTED01();
    int esize = (8 << size);
    int sh = opr_sh13(opc);
    if (((size << 1) | sh) == 1) {ReservedValue();}
    u64 imm = (u64)imm0;
    if (sh == 1) {
	imm = imm << 8;
    }
    perform_IOP_z_imm(zx, esize, Iop_ADD, Zdn, Zdn, imm);
}
static inline void yasve_add_0x04000000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    /* (vectors, predicated) */
    //NOTTESTED00();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_ADD, Zd, Zm, Pg);
}
static inline void yasve_addpl_0x04605000 (CTXARG, int Rd, int Rn, s64 imm) {
    NOTTESTED();
    CheckSVEEnabled();
    u64 operand1 = Xreg_get(zx, Rn, X31SP);
    u64 result = (u64)((s64)operand1 + (imm * (zx->PL / 8)));
    if (Rd == 31) {
	zx->ux->sp = result;
    } else {
	zx->ux->regs[Rd] = result;
    }
}
static inline void yasve_addvl_0x04205000 (CTXARG, int Rd, int Rn, s64 imm) {
    //NOTTESTED00();
    CheckSVEEnabled();
    u64 operand1 = Xreg_get(zx, Rn, X31SP);
    u64 result = (u64)((s64)operand1 + (imm * (zx->VL / 8)));
    if (Rd == 31) {
	zx->ux->sp = result;
    } else {
	zx->ux->regs[Rd] = result;
    }
}
static inline void yasve_adr_0x0420a000 (CTXARG, int Zd, svemo_t mo, int Zm, int Zn, int msz, int _0) {
    /* (Unpacked 32-bit signed offsets) */
    /* SVE_ADDR_ZZ_SXTW=(Zn, Zm/32, opr_msz, 0) */
    assert(mo == OPR_ZnSS_Zm32_MSZ);
    NOTTESTED();
    int esize = 64;
    int osize = 32;
    bool unsignedp = false;
    int mbytes = (1 << msz);
    perform_ADR(zx, esize, osize, mbytes, unsignedp, Zn, Zm, Zd);
}
static inline void yasve_adr_0x0460a000 (CTXARG, int Zd, svemo_t mo, int Zm, int Zn, int msz, int _0) {
    /* (Unpacked 32-bit unsigned offsets) */
    /* SVE_ADDR_ZZ_UXTW=(Zn, Zm/32, opr_msz, 0) */
    assert(mo == OPR_ZnSS_Zm32_MSZ);
    NOTTESTED();
    int esize = 64;
    int osize = 32;
    bool unsignedp = true;
    int mbytes = (1 << msz);
    perform_ADR(zx, esize, osize, mbytes, unsignedp, Zn, Zm, Zd);
}
static inline void yasve_adr_0x04a0a000 (CTXARG, int Zd, svemo_t mo, int Zm, int Zn, int msz, int _sz) {
    /* (Packed offsets) */
    /* SVE_ADDR_ZZ_LSL=(Zn, Zm, opr_msz, opr_sz22) */
    assert(mo == OPR_ZnSS_ZmSS_MSZ_LSL);
    NOTTESTED();
    int esize = (32 << sz);
    int osize = esize;
    bool unsignedp = true;
    int mbytes = (1 << msz);
    perform_ADR(zx, esize, osize, mbytes, unsignedp, Zn, Zm, Zd);
}
static inline void yasve_and_0x04203000 (CTXARG, int Zd, int Zn, int Zm) {
    NOTTESTED();
    int esize = 64;
    perform_IOP_z(zx, esize, Iop_AND, Zd, Zn, Zm);
}
static inline void yasve_and_0x05800000 (CTXARG, int Zd, int _Zd, s64 imm) {
    //NOTTESTED00();
    int esize = 64;
    u64 immv = DecodeBitMasks(esize, (u32)imm);
    perform_IOP_z_imm(zx, esize, Iop_AND, Zd, Zd, immv);
}
static inline void yasve_and_0x041a0000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_AND, Zd, Zm, Pg);
}
static inline void yasve_and_0x25004000 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    //NOTTESTED00();
    int esize = 8;
    bool setflags = false;
    perform_LOP_p(zx, esize, setflags, Iop_AND, Pd, Pn, Pm, Pg);
}
static inline void yasve_ands_0x25404000 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    bool setflags = true;
    perform_LOP_p(zx, esize, setflags, Iop_AND, Pd, Pn, Pm, Pg);
}
static inline void yasve_andv_0x041a2000 (CTXARG, int Vd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    u64 unitv = (~0ULL);
    perform_IREDUCE_seq(zx, esize, Iop_AND, U64EXT, unitv, Vd, Zn, Pg);
}
static inline void yasve_asr_0x04208000 (CTXARG, int Zd, int Zn, int Zm) {
    /* (wide elements, unpredicated) */
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    perform_IOP_z_wide2nd(zx, esize, true, Iop_ASH_R, Zd, Zn, Zm);
}
static inline void yasve_asr_0x04209000 (CTXARG, int Zd, int Zn, s64 imm) {
    //NOTTESTED00();
    int esize = esize_for_shift(imm);
    u64 shift = (u64)((2 * esize) - imm);
    perform_IOP_z_imm(zx, esize, Iop_ASH_R, Zd, Zn, shift);
}
static inline void yasve_asr_0x04108000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_ASH_R, Zd, Zm, Pg);
}
static inline void yasve_asr_0x04188000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    /* (wide elements, predicated) */
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    perform_IOP_z_pred_wide2nd(zx, esize, true, Iop_ASH_R, Zd, Zm, Pg);
}
static inline void yasve_asr_0x04008000 (CTXARG, int Zd, int Pg, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = esize_for_shift(imm);
    u64 shift = (u64)((2 * esize) - imm);
    perform_IOP_z_imm_pred(zx, esize, Iop_ASH_R, Zd, shift, Pg);
}
static inline void yasve_asrd_0x04048000 (CTXARG, int Zd, int Pg, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = esize_for_shift(imm);
    u64 shift = (u64)((2 * esize) - imm);
    perform_IOP_z_imm_pred(zx, esize, Iop_ASH_DIV, Zd, shift, Pg);
}
static inline void yasve_asrr_0x04148000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_ASH_R_REV, Zd, Zm, Pg);
}
static inline void yasve_bic_0x04e03000 (CTXARG, int Zd, int Zn, int Zm) {
    NOTTESTED();
    int esize = 64;
    perform_IOP_z(zx, esize, Iop_ANDN, Zd, Zn, Zm);
}
static inline void yasve_bic_0x041b0000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_ANDN, Zd, Zm, Pg);
}
static inline void yasve_bic_0x25004010 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    bool setflags = false;
    perform_LOP_p(zx, esize, setflags, Iop_ANDN, Pd, Pn, Pm, Pg);
}
static inline void yasve_bics_0x25404010 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    bool setflags = true;
    perform_LOP_p(zx, esize, setflags, Iop_ANDN, Pd, Pn, Pm, Pg);
}
static inline void yasve_brka_0x25104000 (CTXARG, int Pd, int Pg, int Pn) {
    NOTTESTED();
    int esize = 8;
    int M = opr_m4(opc);
    bool merging = (M == 1);
    bool setflags = false;
    perform_BREAK_p(zx, esize, merging, setflags, 1, Pd, Pn, 99, Pg);
}
static inline void yasve_brkas_0x25504000 (CTXARG, int Pd, int Pg, int Pn) {
    NOTTESTED();
    int esize = 8;
    bool merging = false;
    bool setflags = true;
    perform_BREAK_p(zx, esize, merging, setflags, 1, Pd, Pn, 99, Pg);
}
static inline void yasve_brkb_0x25904000 (CTXARG, int Pd, int Pg, int Pn) {
    NOTTESTED();
    int esize = 8;
    int M = opr_m4(opc);
    bool merging = (M == 1);
    bool setflags = false;
    perform_BREAK_p(zx, esize, merging, setflags, 0, Pd, Pn, 99, Pg);
}
static inline void yasve_brkbs_0x25d04000 (CTXARG, int Pd, int Pg, int Pn) {
    NOTTESTED();
    int esize = 8;
    bool merging = false;
    bool setflags = true;
    perform_BREAK_p(zx, esize, merging, setflags, 0, Pd, Pn, 99, Pg);
}
static inline void yasve_brkn_0x25184000 (CTXARG, int Pd, int Pg, int Pn, int _Pd) {
    NOTTESTED();
    bool setflags = false;
    perform_BREAK_NEXT_p(zx, 8, setflags, Pd, Pn, Pg);
}
static inline void yasve_brkns_0x25584000 (CTXARG, int Pd, int Pg, int Pn, int _Pd) {
    NOTTESTED();
    bool setflags = true;
    perform_BREAK_NEXT_p(zx, 8, setflags, Pd, Pn, Pg);
}
static inline void yasve_brkpa_0x2500c000 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    bool setflags = false;
    perform_BREAK_p(zx, esize, false, setflags, 1, Pd, Pm, Pn, Pg);
}
static inline void yasve_brkpas_0x2540c000 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    bool setflags = true;
    perform_BREAK_p(zx, esize, false, setflags, 1, Pd, Pm, Pn, Pg);
}
static inline void yasve_brkpb_0x2500c010 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    bool setflags = false;
    perform_BREAK_p(zx, esize, false, setflags, 0, Pd, Pm, Pn, Pg);
}
static inline void yasve_brkpbs_0x2540c010 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    bool setflags = true;
    perform_BREAK_p(zx, esize, false, setflags, 0, Pd, Pm, Pn, Pg);
}
static inline void yasve_clasta_0x05288000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    bool isBefore = false;
    perform_LAST_z(zx, esize, isBefore, Zd, Zm, Pg);
}
static inline void yasve_clasta_0x052a8000 (CTXARG, int Vd, int Pg, int _0, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    bool isBefore = false;
    perform_LAST_xv(zx, esize, true, isBefore, VREG, Vd, Zm, Pg);
}
static inline void yasve_clasta_0x0530a000 (CTXARG, int Rd, int Pg, int _0, int Zm) {
    NOTTESTED();
    int Rdn = Rd;
    int esize = (8 << size);
    /*int csize = (esize < 64 ? 32 : 64);*/
    bool isBefore = false;
    perform_LAST_xv(zx, esize, true, isBefore, XREG, Rdn, Zm, Pg);
}
static inline void yasve_clastb_0x05298000 (CTXARG, int Zd, int Pg, int _0, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    bool isBefore = true;
    perform_LAST_z(zx, esize, isBefore, Zd, Zm, Pg);
}
static inline void yasve_clastb_0x052b8000 (CTXARG, int Vd, int Pg, int _0, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    bool isBefore = true;
    perform_LAST_xv(zx, esize, true, isBefore, VREG, Vd, Zm, Pg);
}
static inline void yasve_clastb_0x0531a000 (CTXARG, int Rdn, int Pg, int _0, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    /*int csize = (esize < 64 ? 32 : 64);*/
    bool isBefore = true;
    perform_LAST_xv(zx, esize, true, isBefore, XREG, Rdn, Zm, Pg);
}
static inline void yasve_cls_0x0418a000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_CLS, Zd, Zn, Pg);
}
static inline void yasve_clz_0x0419a000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_CLZ, Zd, Zn, Pg);
}
static inline void yasve_cmpeq_0x24002000 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    SVECmp op = Cmp_EQ;
    bool unsignedp = false;
    perform_ICMP_z_z_wide2nd(zx, esize, op, unsignedp, Pd, Zn, Zm, Pg);
}
static inline void yasve_cmpeq_0x2400a000 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_EQ;
    bool unsignedp = false;
    perform_ICMP_z_z(zx, esize, op, unsignedp, Pd, Zn, Zm, Pg);
}
static inline void yasve_cmpeq_0x25008000 (CTXARG, int Pd, int Pg, int Zn, s64 imm) {
    //NOTTESTED00();
    int esize = (8 << size);
    SVECmp op = Cmp_EQ;
    bool unsignedp = false;
    perform_ICMP_z_imm(zx, esize, op, unsignedp, Pd, Zn, imm, Pg);
}
static inline void yasve_cmpge_0x24004000 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    SVECmp op = Cmp_GE;
    bool unsignedp = false;
    perform_ICMP_z_z_wide2nd(zx, esize, op, unsignedp, Pd, Zn, Zm, Pg);
}
static inline void yasve_cmpge_0x24008000 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_GE;
    bool unsignedp = false;
    perform_ICMP_z_z(zx, esize, op, unsignedp, Pd, Zn, Zm, Pg);
}
static inline void yasve_cmpge_0x25000000 (CTXARG, int Pd, int Pg, int Zn, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_GE;
    bool unsignedp = false;
    perform_ICMP_z_imm(zx, esize, op, unsignedp, Pd, Zn, imm, Pg);
}
static inline void yasve_cmpgt_0x24004010 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    SVECmp op = Cmp_GT;
    bool unsignedp = false;
    perform_ICMP_z_z_wide2nd(zx, esize, op, unsignedp, Pd, Zn, Zm, Pg);
}
static inline void yasve_cmpgt_0x24008010 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_GT;
    bool unsignedp = false;
    perform_ICMP_z_z(zx, esize, op, unsignedp, Pd, Zn, Zm, Pg);
}
static inline void yasve_cmpgt_0x25000010 (CTXARG, int Pd, int Pg, int Zn, s64 imm) {
    //NOTTESTED00();
    int esize = (8 << size);
    SVECmp op = Cmp_GT;
    bool unsignedp = false;
    perform_ICMP_z_imm(zx, esize, op, unsignedp, Pd, Zn, imm, Pg);
}
static inline void yasve_cmphi_0x24000010 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_GT;
    bool unsignedp = true;
    perform_ICMP_z_z(zx, esize, op, unsignedp, Pd, Zn, Zm, Pg);
}
static inline void yasve_cmphi_0x2400c010 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    SVECmp op = Cmp_GT;
    bool unsignedp = true;
    perform_ICMP_z_z_wide2nd(zx, esize, op, unsignedp, Pd, Zn, Zm, Pg);
}
static inline void yasve_cmphi_0x24200010 (CTXARG, int Pd, int Pg, int Zn, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_GT;
    bool unsignedp = true;
    perform_ICMP_z_imm(zx, esize, op, unsignedp, Pd, Zn, imm, Pg);
}
static inline void yasve_cmphs_0x24000000 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_GE;
    bool unsignedp = true;
    perform_ICMP_z_z(zx, esize, op, unsignedp, Pd, Zn, Zm, Pg);
}
static inline void yasve_cmphs_0x2400c000 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    SVECmp op = Cmp_GE;
    bool unsignedp = true;
    perform_ICMP_z_z_wide2nd(zx, esize, op, unsignedp, Pd, Zn, Zm, Pg);
}
static inline void yasve_cmphs_0x24200000 (CTXARG, int Pd, int Pg, int Zn, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_GE;
    bool unsignedp = true;
    perform_ICMP_z_imm(zx, esize, op, unsignedp, Pd, Zn, imm, Pg);
}
static inline void yasve_cmple_0x24006010 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    SVECmp op = Cmp_LE;
    bool unsignedp = false;
    perform_ICMP_z_z_wide2nd(zx, esize, op, unsignedp, Pd, Zn, Zm, Pg);
}
static inline void yasve_cmple_0x25002010 (CTXARG, int Pd, int Pg, int Zn, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_LE;
    bool unsignedp = false;
    perform_ICMP_z_imm(zx, esize, op, unsignedp, Pd, Zn, imm, Pg);
}
static inline void yasve_cmplo_0x2400e000 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    SVECmp op = Cmp_LT;
    bool unsignedp = true;
    perform_ICMP_z_z_wide2nd(zx, esize, op, unsignedp, Pd, Zn, Zm, Pg);
}
static inline void yasve_cmplo_0x24202000 (CTXARG, int Pd, int Pg, int Zn, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_LT;
    bool unsignedp = true;
    perform_ICMP_z_imm(zx, esize, op, unsignedp, Pd, Zn, imm, Pg);
}
static inline void yasve_cmpls_0x2400e010 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    SVECmp op = Cmp_LE;
    bool unsignedp = true;
    perform_ICMP_z_z_wide2nd(zx, esize, op, unsignedp, Pd, Zn, Zm, Pg);
}
static inline void yasve_cmpls_0x24202010 (CTXARG, int Pd, int Pg, int Zn, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_LE;
    bool unsignedp = true;
    perform_ICMP_z_imm(zx, esize, op, unsignedp, Pd, Zn, imm, Pg);
}
static inline void yasve_cmplt_0x24006000 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    SVECmp op = Cmp_LT;
    bool unsignedp = false;
    perform_ICMP_z_z_wide2nd(zx, esize, op, unsignedp, Pd, Zn, Zm, Pg);
}
static inline void yasve_cmplt_0x25002000 (CTXARG, int Pd, int Pg, int Zn, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_LT;
    bool unsignedp = false;
    perform_ICMP_z_imm(zx, esize, op, unsignedp, Pd, Zn, imm, Pg);
}
static inline void yasve_cmpne_0x24002010 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    SVECmp op = Cmp_NE;
    bool unsignedp = false;
    perform_ICMP_z_z_wide2nd(zx, esize, op, unsignedp, Pd, Zn, Zm, Pg);
}
static inline void yasve_cmpne_0x2400a010 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_NE;
    bool unsignedp = false;
    perform_ICMP_z_z(zx, esize, op, unsignedp, Pd, Zn, Zm, Pg);
}
static inline void yasve_cmpne_0x25008010 (CTXARG, int Pd, int Pg, int Zn, s64 imm) {
    //NOTTESTED00();
    int esize = (8 << size);
    SVECmp op = Cmp_NE;
    bool unsignedp = false;
    perform_ICMP_z_imm(zx, esize, op, unsignedp, Pd, Zn, imm, Pg);
}
static inline void yasve_cnot_0x041ba000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_ZEROP, Zd, Zn, Pg);
}
static inline void yasve_cnt_0x041aa000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_POPC, Zd, Zn, Pg);
}
static inline void yasve_cntb_0x0420e000 (CTXARG, int Rd, int pattern, s64 imm) {
    //NOTTESTED18();
    int esize = 8;
    int pat = pattern;
    u64 immv = ((u64)imm + 1);
    perform_ELEMENT_COUNT(zx, esize, Rd, pat, immv);
}
static inline void yasve_cntd_0x04e0e000 (CTXARG, int Rd, int pattern, s64 imm) {
    //NOTTESTED21();
    int esize = 64;
    int pat = pattern;
    u64 immv = ((u64)imm + 1);
    perform_ELEMENT_COUNT(zx, esize, Rd, pat, immv);
}
static inline void yasve_cnth_0x0460e000 (CTXARG, int Rd, int pattern, s64 imm) {
    NOTTESTED();
    int esize = 16;
    int pat = pattern;
    u64 immv = ((u64)imm + 1);
    perform_ELEMENT_COUNT(zx, esize, Rd, pat, immv);
}
static inline void yasve_cntp_0x25208000 (CTXARG, int Rd, int Pg, int Pn) {
    NOTTESTED();
    int esize = (8 << size);
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    preg operand = zx->p[Pn];
    u64 sum = 0;
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1
	    && ElemP_get(zx, &operand, e, esize) == 1) {
	    sum = sum + 1;
	}
    }
    Xreg_set(zx, Rd, sum);
}
static inline void yasve_cntw_0x04a0e000 (CTXARG, int Rd, int pattern, s64 imm) {
    //NOTTESTED00();
    int esize = 32;
    int pat = pattern;
    u64 immv = ((u64)imm + 1);
    perform_ELEMENT_COUNT(zx, esize, Rd, pat, immv);
}
static inline void yasve_compact_0x05218000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    /*int esize = (32 << sz);*/
    int esize = (8 << size);
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg operand1 = zx->z[Zn];
    zreg result = zreg_zeros;
    int x = 0;
    for (int e = 0; e < elements; e++) {
	Elem_set(zx, &result, e, esize, 0);
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    u64 element = Elem_get(zx, &operand1, e, esize, DONTCARE);
	    Elem_set(zx, &result, x, esize, element);
	    x = x + 1;
	}
    }
    zx->z[Zd] = result;
}
static inline void yasve_cpy_0x05208000 (CTXARG, int Zd, int Pg, int Vn) {
    NOTTESTED();
    int esize = (8 << size);
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    u64 operand1 = Vreg_get(zx, Vn);
    zreg dest = zx->z[Zd];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    Elem_set(zx, &result, e, esize, operand1);
	} else {
	    u64 v = Elem_get(zx, &dest, e, esize, DONTCARE);
	    Elem_set(zx, &result, e, esize, v);
	}
    }
    zx->z[Zd] = result;
}
static inline void yasve_cpy_0x0528a000 (CTXARG, int Zd, int Pg, int Rn) {
    NOTTESTED();
    int esize = (8 << size);
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    u64 operand1 = Xreg_get(zx, Rn, X31SP);
    zreg dest = zx->z[Zd];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    Elem_set(zx, &result, e, esize, operand1);
	} else {
	    u64 v = Elem_get(zx, &dest, e, esize, DONTCARE);
	    Elem_set(zx, &result, e, esize, v);
	}
    }
    zx->z[Zd] = result;
}
static inline void yasve_cpy_0x05100000 (CTXARG, int Zd, int Pg, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    int sh = opr_sh13(opc);
    int M = opr_m14(opc);
    bool merging = (M == 1);
    if (((size << 1) | sh) == 1) {ReservedValue();}
    s64 immv = imm;
    if (sh == 1) {
	immv = (immv << 8);
    }
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg dest = zx->z[Zd];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    Elem_set(zx, &result, e, esize, (u64)immv);
	} else if (merging){
	    u64 v = Elem_get(zx, &dest, e, esize, DONTCARE);
	    Elem_set(zx, &result, e, esize, v);
	} else {
	    Elem_set(zx, &result, e, esize, 0);
	}
    }
    zx->z[Zd] = result;
}
static inline void yasve_ctermeq_0x25a02000 (CTXARG, int Rn, int Rm) {
    NOTTESTED();
    int esize = (32 << sz);
    SVECmp op = Cmp_EQ;
    perform_TERM_x(zx, esize, op, Rn, Rm);
}
static inline void yasve_ctermne_0x25a02010 (CTXARG, int Rn, int Rm) {
    NOTTESTED();
    int esize = (32 << sz);
    SVECmp op = Cmp_NE;
    perform_TERM_x(zx, esize, op, Rn, Rm);
}
static inline void yasve_decb_0x0430e400 (CTXARG, int Rdn, int pattern, s64 imm) {
    NOTTESTED();
    int esize = 8;
    s64 immv = imm + 1;
    perform_INC_x(zx, esize, Rdn, pattern, (- immv));
}
static inline void yasve_decd_0x04f0c400 (CTXARG, int Zd, int pattern, s64 imm) {
    NOTTESTED();
    int esize = 64;
    s64 immv = imm + 1;
    perform_INC_z(zx, esize, Zd, pattern, (- immv));
}
static inline void yasve_decd_0x04f0e400 (CTXARG, int Rdn, int pattern, s64 imm) {
    NOTTESTED();
    int esize = 64;
    s64 immv = imm + 1;
    perform_INC_x(zx, esize, Rdn, pattern, (- immv));
}
static inline void yasve_dech_0x0470c400 (CTXARG, int Zd, int pattern, s64 imm) {
    NOTTESTED();
    int esize = 16;
    s64 immv = imm + 1;
    perform_INC_z(zx, esize, Zd, pattern, (- immv));
}
static inline void yasve_dech_0x0470e400 (CTXARG, int Rd, int pattern, s64 imm) {
    NOTTESTED();
    int esize = 16;
    s64 immv = imm + 1;
    perform_INC_x(zx, esize, Rd, pattern, (- immv));
}
static inline void yasve_decp_0x252d8000 (CTXARG, int Zd, int Pg) {
    NOTTESTED();
    if (size == 0) {UnallocatedEncoding();}
    int esize = (8 << size);
    perform_INC_z_pred(zx, esize, Zd, Pg, -1);
}
static inline void yasve_decp_0x252d8800 (CTXARG, int Rdn, int Pg) {
    NOTTESTED();
    int esize = (8 << size);
    perform_INC_x_pred(zx, esize, Rdn, Pg, -1);
}
static inline void yasve_decw_0x04b0c400 (CTXARG, int Zd, int pattern, s64 imm) {
    NOTTESTED();
    int esize = 32;
    s64 immv = imm + 1;
    perform_INC_z(zx, esize, Zd, pattern, (- immv));
}
static inline void yasve_decw_0x04b0e400 (CTXARG, int Rdn, int pattern, s64 imm) {
    NOTTESTED();
    int esize = 32;
    s64 immv = imm + 1;
    perform_INC_x(zx, esize, Rdn, pattern, (- immv));
}
static inline void yasve_dup_0x05203800 (CTXARG, int Zd, int Rn) {
    /* (scalar) */
    //NOTTESTED00();
    int esize = (8 << size);
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    u64 operand = Xreg_get(zx, Rn, X31SP);
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	Elem_set(zx, &result, e, esize, operand);
    }
    zx->z[Zd] = result;
}
static inline void yasve_dup_0x05202000 (CTXARG, int Zd, int Zn, s64 imm) {
    /* (indexed) */
    //NOTTESTED00();
    int esize;
    int index;
    int tsz = (imm & 0xf);
    if (tsz == 0x0) {
	/*(0000)*/
	UnallocatedEncoding();
    } else if (tsz == 0x8) {
	/*(1000)*/
	esize = 64;
	index = (int)(((u64)imm) >> 4);
    } else if ((tsz & 0x7) == 0x4) {
	/*(x100)*/
	esize = 32;
	index = (int)(((u64)imm) >> 3);
    } else if ((tsz & 0x3) == 0x2) {
	/*(xx10)*/
	esize = 16;
	index = (int)(((u64)imm) >> 2);
    } else if ((tsz & 0x1) == 0x1) {
	/*(xxx1)*/
	esize = 8;
	index = (int)(((u64)imm) >> 1);
    } else {
	assert(0);
	abort();
    }
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    zreg operand1 = zx->z[Zn];
    zreg result = zreg_zeros;
    u64 element;
    if (index >= elements) {
	element = 0;
    } else {
	element = Elem_get(zx, &operand1, index, esize, DONTCARE);
    }
    result = Replicate_z(zx, esize, element);
    zx->z[Zd] = result;
}
static inline void yasve_dup_0x2538c000 (CTXARG, int Zd, s64 imm) {
    /* (immediate) */
    //NOTTESTED00();
    int esize = (8 << size);
    int sh = opr_sh13(opc);
    u64 immv = (u64)imm;
    if (((size << 1) | sh) == 1) {ReservedValue();}
    if (sh == 1) {immv = immv << 8;}
    CheckSVEEnabled();
    zreg result = Replicate_z(zx, esize, immv);
    zx->z[Zd] = result;
}
static inline void yasve_dupm_0x05c00000 (CTXARG, int Zd, s64 imm) {
    NOTTESTED();
    int esize = 64;
    u64 immv = DecodeBitMasks(esize, (u32)imm);
    CheckSVEEnabled();
    zreg result = Replicate_z(zx, esize, immv);
    zx->z[Zd] = result;
}
static inline void yasve_eor_0x04a03000 (CTXARG, int Zd, int Zn, int Zm) {
    NOTTESTED();
    int esize = 64;
    perform_IOP_z(zx, esize, Iop_XOR, Zd, Zn, Zm);
}
static inline void yasve_eor_0x05400000 (CTXARG, int Zd, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = 64;
    u64 immv = DecodeBitMasks(esize, (u32)imm);
    perform_IOP_z_imm(zx, esize, Iop_XOR, Zd, Zd, immv);
}
static inline void yasve_eor_0x04190000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_XOR, Zd, Zm, Pg);
}
static inline void yasve_eor_0x25004200 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    bool setflags = false;
    perform_LOP_p(zx, esize, setflags, Iop_XOR, Pd, Pn, Pm, Pg);
}
static inline void yasve_eors_0x25404200 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    bool setflags = true;
    perform_LOP_p(zx, esize, setflags, Iop_XOR, Pd, Pn, Pm, Pg);
}
static inline void yasve_eorv_0x04192000 (CTXARG, int Vd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    u64 unitv = 0ULL;
    perform_IREDUCE_seq(zx, esize, Iop_XOR, U64EXT, unitv, Vd, Zn, Pg);
}
static inline void yasve_ext_0x05200000 (CTXARG, int Zd, int _Zd, int Zm, s64 imm) {
    NOTTESTED();
    int esize = 8;
    int position = (int)imm;
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    zreg operand1 = zx->z[Zd];
    zreg operand2 = zx->z[Zm];
    zreg result = zreg_zeros;
    if (position >= elements) {
	position = 0;
    }
    position = position << 3;
    size_t vlen = (size_t)(zx->VL / 8);
    u8 concat[vlen * 2];
    memcpy(&concat[0], &operand1, vlen);
    memcpy(&concat[vlen], &operand2, vlen);
    memset(&result, 0, sizeof(result));
    memcpy(&result, &concat[position / 8], vlen);
    zx->z[Zd] = result;
}

static inline void yasve_fabd_0x65088000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_DIFF, true, Zd, 99, Zm, Pg);
}

static inline void yasve_fabs_0x041ca000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_ABS, true, Zd, 99, Zn, Pg);
}

static inline void yasve_facge_0x6500c010 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_GE;
    perform_FCMP_z(zx, esize, op, true, Pd, Zn, Zm, Pg);
}
static inline void yasve_facgt_0x6500e010 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_GT;
    perform_FCMP_z(zx, esize, op, true, Pd, Zn, Zm, Pg);
}

static inline void yasve_fadd_0x65000000 (CTXARG, int Zd, int Zn, int Zm) {
    /* (vectors, unpredicated) */
    //NOTTESTED00();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_ADD, false, Zd, Zn, Zm, 99);
}
static inline void yasve_fadd_0x65008000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    /* (vectors, predicated) */
    //NOTTESTED00();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_ADD, true, Zd, 99, Zm, Pg);
}
static inline void yasve_fadd_0x65188000 (CTXARG, int Zd, int Pg, int _Zd, s64 imm) {
    /* (immediate) */
    NOTTESTED();
    int esize = (8 << size);
    u64 immv = constant_f(esize, ((imm == 0) ? 0.5 : 1.0));
    perform_FOP_imm(zx, esize, Fop_ADD, true, Zd, immv, Pg);
}

static inline void yasve_fadda_0x65182000 (CTXARG, int Vd, int Pg, int _Vd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_FREDUCE_seq(zx, esize, Fop_ADD, Vd, Zm, Pg);
}

static inline void yasve_faddv_0x65002000 (CTXARG, int Vd, int Pg, int Zn) {
    //NOTTESTED00();
    int esize = (8 << size);
    u64 unitv = ((esize == 16) ? fp16_as_u64(0.0)
		 : ((esize == 32) ? float_as_u64(0.0)
		    : double_as_u64(0.0)));
    perform_FREDUCE_rec(zx, esize, Fop_ADD, unitv, Vd, Zn, Pg);
}

static inline void yasve_fcadd_0x64008000 (CTXARG, int Zd, int Pg, int _Zd, int Zm, s64 imm) {
    /* (SVE_Zd, SVE_Pg3, SVE_Zd, SVE_Zm_5, SVE_IMM_ROT1) */
    TBD("fcadd");
}

static inline void yasve_fcmla_0x64000000 (CTXARG, int Zd, int Pg, int Zn, int Zm, s64 imm) {
    /* (vectors) */
    /* (SVE_Zd, SVE_Pg3, SVE_Zn, SVE_Zm_16, IMM_ROT2) */
    TBD("fcmla");
}
static inline void yasve_fcmla_0x64a01000 (CTXARG, int Zd, int Zn, int Zm, s64 imm) {
    /* (indexed/Half-precision) */
    /* (SVE_Zd, SVE_Zn, SVE_Zm3_INDEX, SVE_IMM_ROT2) */
    TBD("fcmla");
}
static inline void yasve_fcmla_0x64e01000 (CTXARG, int Zd, int Zn, int Zm, s64 imm) {
    /* (indexed/Single-precision) */
    /* (SVE_Zd, SVE_Zn, SVE_Zm4_INDEX, SVE_IMM_ROT2) */
    TBD("fcmla");
}

static inline void yasve_fcmeq_0x65122000 (CTXARG, int Pd, int Pg, int Zn, s64 zero) {
    /* (zero) */
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_EQ;
    perform_FCMP_zero(zx, esize, op, false, Pd, Zn, Pg);
}
static inline void yasve_fcmeq_0x65006000 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    /* (vectors) */
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_EQ;
    perform_FCMP_z(zx, esize, op, false, Pd, Zn, Zm, Pg);
}
static inline void yasve_fcmge_0x65102000 (CTXARG, int Pd, int Pg, int Zn, s64 zero) {
    /* (zero) */
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_GE;
    perform_FCMP_zero(zx, esize, op, false, Pd, Zn, Pg);
}
static inline void yasve_fcmge_0x65004000 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    /* (vectors) */
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_GE;
    perform_FCMP_z(zx, esize, op, false, Pd, Zn, Zm, Pg);
}
static inline void yasve_fcmgt_0x65102010 (CTXARG, int Pd, int Pg, int Zn, s64 zero) {
    /* (zero) */
    //NOTTESTED00();
    int esize = (8 << size);
    SVECmp op = Cmp_GT;
    perform_FCMP_zero(zx, esize, op, false, Pd, Zn, Pg);
}
static inline void yasve_fcmgt_0x65004010 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    /* (vectors) */
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_GT;
    perform_FCMP_z(zx, esize, op, false, Pd, Zn, Zm, Pg);
}
static inline void yasve_fcmle_0x65112010 (CTXARG, int Pd, int Pg, int Zn, s64 zero) {
    /* (zero) */
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_LE;
    perform_FCMP_zero(zx, esize, op, false, Pd, Zn, Pg);
}
static inline void yasve_fcmlt_0x65112000 (CTXARG, int Pd, int Pg, int Zn, s64 zero) {
    /* (zero) */
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_LT;
    perform_FCMP_zero(zx, esize, op, false, Pd, Zn, Pg);
}
static inline void yasve_fcmne_0x65132000 (CTXARG, int Pd, int Pg, int Zn, s64 zero) {
    /* (zero) */
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_NE;
    perform_FCMP_zero(zx, esize, op, false, Pd, Zn, Pg);
}
static inline void yasve_fcmne_0x65006010 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    /* (vectors) */
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_NE;
    perform_FCMP_z(zx, esize, op, false, Pd, Zn, Zm, Pg);
}
static inline void yasve_fcmuo_0x6500c000 (CTXARG, int Pd, int Pg, int Zn, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    SVECmp op = Cmp_UN;
    perform_FCMP_z(zx, esize, op, false, Pd, Zn, Zm, Pg);
}

static inline void yasve_fcpy_0x0510c000 (CTXARG, int Zd, int Pg, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    u64 immv = VFPExpandImm(esize, (u64)imm);
    perform_FOP_imm(zx, esize, Fop_CPY, true, Zd, immv, Pg);
}

static inline void yasve_fcvt_0x6588a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Single-precision to half-precision) */
    NOTTESTED();
    int esize = 32;
    perform_FOP_z(zx, esize, Fop_CVT_32_16, true, Zd, 99, Zn, Pg);
}
static inline void yasve_fcvt_0x6589a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Half-precision to single-precision) */
    NOTTESTED();
    int esize = 32;
    perform_FOP_z(zx, esize, Fop_CVT_16_32, true, Zd, 99, Zn, Pg);
}
static inline void yasve_fcvt_0x65c8a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Double-precision to half-precision) */
    NOTTESTED();
    int esize = 64;
    perform_FOP_z(zx, esize, Fop_CVT_64_16, true, Zd, 99, Zn, Pg);
}
static inline void yasve_fcvt_0x65c9a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Half-precision to double-precision) */
    NOTTESTED();
    int esize = 64;
    perform_FOP_z(zx, esize, Fop_CVT_16_64, true, Zd, 99, Zn, Pg);
}
static inline void yasve_fcvt_0x65caa000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Double-precision to single-precision) */
    NOTTESTED();
    int esize = 64;
    perform_FOP_z(zx, esize, Fop_CVT_64_32, true, Zd, 99, Zn, Pg);
}
static inline void yasve_fcvt_0x65cba000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Single-precision to double-precision) */
    NOTTESTED();
    int esize = 64;
    perform_FOP_z(zx, esize, Fop_CVT_32_64, true, Zd, 99, Zn, Pg);
}

static inline void yasve_fcvtzs_0x655aa000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Half-precision to 16-bit) */
    TBD("fcvtzs");
}
static inline void yasve_fcvtzs_0x655ca000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Half-precision to 32-bit) */
    TBD("fcvtzs");
}
static inline void yasve_fcvtzs_0x655ea000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Half-precision to 64-bit) */
    TBD("fcvtzs");
}
static inline void yasve_fcvtzs_0x659ca000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Single-precision to 32-bit) */
    NOTTESTED();
    int esize = 32;
    perform_FOP_z(zx, esize, Fop_CVTI_32_S32, true, Zd, 99, Zn, Pg);
}
static inline void yasve_fcvtzs_0x65d8a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Double-precision to 32-bit) */
    NOTTESTED();
    int esize = 64;
    perform_FOP_z(zx, esize, Fop_CVTI_64_S32, true, Zd, 99, Zn, Pg);
}
static inline void yasve_fcvtzs_0x65dca000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Single-precision to 64-bit) */
    NOTTESTED();
    int esize = 64;
    perform_FOP_z(zx, esize, Fop_CVTI_32_S64, true, Zd, 99, Zn, Pg);
}
static inline void yasve_fcvtzs_0x65dea000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Double-precision to 64-bit) */
    //NOTTESTED14();
    int esize = 64;
    perform_FOP_z(zx, esize, Fop_CVTI_64_S64, true, Zd, 99, Zn, Pg);
}

static inline void yasve_fcvtzu_0x655ba000 (CTXARG, int Zd, int Pg3, int Zn) {
    /* (Half-precision to 16-bit) */
    TBD("fcvtzu");
}
static inline void yasve_fcvtzu_0x655da000 (CTXARG, int Zd, int Pg3, int Zn) {
    /* (Half-precision to 32-bit) */
    TBD("fcvtzu");
}
static inline void yasve_fcvtzu_0x655fa000 (CTXARG, int Zd, int Pg3, int Zn) {
    /* (Half-precision to 64-bit) */
    TBD("fcvtzu");
}
static inline void yasve_fcvtzu_0x659da000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Single-precision to 32-bit) */
    NOTTESTED();
    int esize = 32;
    perform_FOP_z(zx, esize, Fop_CVTI_32_U32, true, Zd, 99, Zn, Pg);
}
static inline void yasve_fcvtzu_0x65d9a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Double-precision to 32-bit) */
    NOTTESTED();
    int esize = 64;
    perform_FOP_z(zx, esize, Fop_CVTI_64_U32, true, Zd, 99, Zn, Pg);
}
static inline void yasve_fcvtzu_0x65dda000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Single-precision to 64-bit) */
    NOTTESTED();
    int esize = 64;
    perform_FOP_z(zx, esize, Fop_CVTI_32_U64, true, Zd, 99, Zn, Pg);
}
static inline void yasve_fcvtzu_0x65dfa000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (Double-precision to 64-bit) */
    NOTTESTED();
    int esize = 64;
    perform_FOP_z(zx, esize, Fop_CVTI_64_U64, true, Zd, 99, Zn, Pg);
}

static inline void yasve_fdiv_0x650d8000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_DIV, true, Zd, 99, Zm, Pg);
}
static inline void yasve_fdivr_0x650c8000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_DIV_REV, true, Zd, 99, Zm, Pg);
}
static inline void yasve_fdup_0x2539c000 (CTXARG, int Zd, s64 imm) {
    //NOTTESTED00();
    int esize = (8 << size);
    u64 immv = VFPExpandImm(esize, (u64)imm);
    perform_FOP_imm(zx, esize, Fop_CPY, false, Zd, immv, 99);
}
static inline void yasve_fexpa_0x0420b800 (CTXARG, int Zd, int Zn) {
    /* (SVE_Zd, SVE_Zn) */
    TBD("fexpa");
}
static inline void yasve_fmad_0x65208000 (CTXARG, int Zd, int Pg, int Zm, int Za) {
    /* (Zd := Za + Zd * Zm) */
    NOTTESTED();
    int esize = (8 << size);
    bool op1_neg = false;
    bool op3_neg = false;
    perform_FFMA(zx, esize, Zd, op3_neg, Za, op1_neg, Zd, Zm, Pg);
}
static inline void yasve_fmax_0x65068000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_MAX, true, Zd, 99, Zm, Pg);
}
static inline void yasve_fmax_0x651e8000 (CTXARG, int Zd, int Pg, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    u64 immv = constant_f(esize, ((imm == 0) ? 0.0 : 1.0));
    perform_FOP_imm(zx, esize, Fop_MAX, true, Zd, immv, Pg);
}
static inline void yasve_fmaxnm_0x65048000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_MAXNUM, true, Zd, 99, Zm, Pg);
}
static inline void yasve_fmaxnm_0x651c8000 (CTXARG, int Zd, int Pg, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    u64 immv = constant_f(esize, ((imm == 0) ? 0.0 : 1.0));
    perform_FOP_imm(zx, esize, Fop_MAXNUM, true, Zd, immv, Pg);
}
static inline void yasve_fmaxnmv_0x65042000 (CTXARG, int Vd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    u64 unitv = FPDefaultNaN(esize);
    perform_FREDUCE_rec(zx, esize, Fop_MAXNUM, unitv, Vd, Zn, Pg);
}
static inline void yasve_fmaxv_0x65062000 (CTXARG, int Vd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    u64 unitv = FPInfinity(esize, 1);
    perform_FREDUCE_rec(zx, esize, Fop_MAX, unitv, Vd, Zn, Pg);
}
static inline void yasve_fmin_0x65078000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_MIN, true, Zd, 99, Zm, Pg);
}
static inline void yasve_fmin_0x651f8000 (CTXARG, int Zd, int Pg, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    u64 immv = constant_f(esize, ((imm == 0) ? 0.0 : 1.0));
    perform_FOP_imm(zx, esize, Fop_MIN, true, Zd, immv, Pg);
}
static inline void yasve_fminnm_0x65058000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_MINNUM, true, Zd, 99, Zm, Pg);
}
static inline void yasve_fminnm_0x651d8000 (CTXARG, int Zd, int Pg, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    u64 immv = constant_f(esize, ((imm == 0) ? 0.0 : 1.0));
    perform_FOP_imm(zx, esize, Fop_MINNUM, true, Zd, immv, Pg);
}
static inline void yasve_fminnmv_0x65052000 (CTXARG, int Vd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    u64 unitv = FPDefaultNaN(esize);
    perform_FREDUCE_rec(zx, esize, Fop_MINNUM, unitv, Vd, Zn, Pg);
}
static inline void yasve_fminv_0x65072000 (CTXARG, int Vd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    u64 unitv = FPInfinity(esize, 0);
    perform_FREDUCE_rec(zx, esize, Fop_MIN, unitv, Vd, Zn, Pg);
}

static inline void yasve_fmla_0x65200000 (CTXARG, int Zd, int Pg, int Zn, int Zm) {
    /* (vectors) */
    /* (Zd := Zd + Zn * Zm) */
    //NOTTESTED00();
    int esize = (8 << size);
    bool op1_neg = false;
    bool op3_neg = false;
    perform_FFMA(zx, esize, Zd, op3_neg, Zd, op1_neg, Zn, Zm, Pg);
}

static inline void yasve_fmla_0x64200000 (CTXARG, int Zd, int Zn, int Zm) {
    /* (indexed/Half-precision) */
    TBD("fmla");
}
static inline void yasve_fmla_0x64a00000 (CTXARG, int Zd, int Zn, int Zm) {
    /* (indexed/Single-precision) */
    TBD("fmla");
}
static inline void yasve_fmla_0x64e00000 (CTXARG, int Zd, int Zn, int Zm) {
    /* (indexed/Double-precision) */
    TBD("fmla");
}

static inline void yasve_fmls_0x65202000 (CTXARG, int Zd, int Pg, int Zn, int Zm) {
    /* (Zd := Zd + -Zn * Zm) */
    //NOTTESTED01();
    int esize = (8 << size);
    bool op1_neg = true;
    bool op3_neg = false;
    perform_FFMA(zx, esize, Zd, op3_neg, Zd, op1_neg, Zn, Zm, Pg);
}
static inline void yasve_fmls_0x64200400 (CTXARG, int Zd, int Zn, int Zm) {
    /* (indexed/Half-precision) */
    TBD("fmla");
}
static inline void yasve_fmls_0x64a00400 (CTXARG, int Zd, int Zn, int Zm) {
    /* (indexed/Single-precision) */
    TBD("fmla");
}
static inline void yasve_fmls_0x64e00400 (CTXARG, int Zd, int Zn, int Zm) {
    /* (indexed/Double-precision) */
    TBD("fmla");
}

static inline void yasve_fmsb_0x6520a000 (CTXARG, int Zd, int Pg, int Zm, int Za) {
    /* (Zd = Za + -Zd * Zm) */
    NOTTESTED();
    int esize = (8 << size);
    bool op1_neg = true;
    bool op3_neg = false;
    perform_FFMA(zx, esize, Zd, op3_neg, Za, op1_neg, Zd, Zm, Pg);
}
static inline void yasve_fmul_0x65000800 (CTXARG, int Zd, int Zn, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_MUL, false, Zd, Zn, Zm, 99);
  }
static inline void yasve_fmul_0x65028000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_MUL, true, Zd, 99, Zm, Pg);
}
static inline void yasve_fmul_0x651a8000 (CTXARG, int Zd, int Pg, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    u64 immv = constant_f(esize, ((imm == 0) ? 0.5 : 2.0));
    perform_FOP_imm(zx, esize, Fop_MUL, true, Zd, immv, Pg);
}
static inline void yasve_fmul_0x64202000 (CTXARG, int Zd, int Zn, int Zm) {
    /* (indexed/Half-precision) */
    TBD("fmla");
}
static inline void yasve_fmul_0x64a02000 (CTXARG, int Zd, int Zn, int Zm) {
    /* (indexed/Single-precision) */
    TBD("fmla");
}
static inline void yasve_fmul_0x64e02000 (CTXARG, int Zd, int Zn, int Zm) {
    /* (indexed/Double-precision) */
    TBD("fmla");
}

static inline void yasve_fmulx_0x650a8000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    /* (SVE_Zd, SVE_Pg3, SVE_Zd, SVE_Zm_5) */
    TBD("fmulx");
}

static inline void yasve_fneg_0x041da000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_NEG, true, Zd, 99, Zn, Pg);
}

static inline void yasve_fnmad_0x6520c000 (CTXARG, int Zd, int Pg, int Zm, int Za) {
    /* (Zd := -Za + -Zd * Zm) */
    NOTTESTED();
    int esize = (8 << size);
    bool op1_neg = true;
    bool op3_neg = true;
    perform_FFMA(zx, esize, Zd, op3_neg, Za, op1_neg, Zd, Zm, Pg);
}
static inline void yasve_fnmla_0x65204000 (CTXARG, int Zd, int Pg, int Zn, int Zm) {
    /* (Zd := -Zd + -Zn * Zm) */
    NOTTESTED();
    int esize = (8 << size);
    bool op1_neg = true;
    bool op3_neg = true;
    perform_FFMA(zx, esize, Zd, op3_neg, Zd, op1_neg, Zn, Zm, Pg);
}
static inline void yasve_fnmls_0x65206000 (CTXARG, int Zd, int Pg, int Zn, int Zm) {
    /* (Zd := -Zd + Zn * Zm) */
    NOTTESTED();
    int esize = (8 << size);
    bool op1_neg = false;
    bool op3_neg = true;
    perform_FFMA(zx, esize, Zd, op3_neg, Zd, op1_neg, Zn, Zm, Pg);
}
static inline void yasve_fnmsb_0x6520e000 (CTXARG, int Zd, int Pg, int Zm, int Za) {
    /* (Zd := -Za + Zd * Zm) */
    NOTTESTED();
    int esize = (8 << size);
    bool op1_neg = false;
    bool op3_neg = true;
    perform_FFMA(zx, esize, Zd, op3_neg, Za, op1_neg, Zd, Zm, Pg);
}
static inline void yasve_frecpe_0x650e3000 (CTXARG, int Zd, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    /* (Use dummy Zn.) */
    perform_FOP_z(zx, esize, Fop_RECPE, false, Zd, Zn, Zn, 99);
}
static inline void yasve_frecps_0x65001800 (CTXARG, int Zd, int Zn, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_RECPS, false, Zd, Zn, Zm, 99);
}
static inline void yasve_frecpx_0x650ca000 (CTXARG, int Zd, int Pg, int Zn) {
    TBD("frecpx");
}
static inline void yasve_frinta_0x6504a000 (CTXARG, int Zd, int Pg, int Zn) {
    TBD("frinta");
}
static inline void yasve_frinti_0x6507a000 (CTXARG, int Zd, int Pg, int Zn) {
    TBD("frinti");
}
static inline void yasve_frintm_0x6502a000 (CTXARG, int Zd, int Pg, int Zn) {
    TBD("frintm");
}
static inline void yasve_frintn_0x6500a000 (CTXARG, int Zd, int Pg, int Zn) {
    TBD("frintn");
}
static inline void yasve_frintp_0x6501a000 (CTXARG, int Zd, int Pg, int Zn) {
    TBD("frintp");
}
static inline void yasve_frintx_0x6506a000 (CTXARG, int Zd, int Pg, int Zn) {
    TBD("frintx");
}
static inline void yasve_frintz_0x6503a000 (CTXARG, int Zd, int Pg, int Zn) {
    TBD("frintz");
}
static inline void yasve_frsqrte_0x650f3000 (CTXARG, int Zd, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    /* (Dummy Zn) */
    perform_FOP_z(zx, esize, Fop_RSQRTE, false, Zd, Zn, Zn, 99);
}
static inline void yasve_frsqrts_0x65001c00 (CTXARG, int Zd, int Zn, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    /* (Dummy Zn) */
    perform_FOP_z(zx, esize, Fop_RSQRTS, false, Zd, Zn, Zn, 99);
}
static inline void yasve_fscale_0x65098000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    TBD("fscale");
}
static inline void yasve_fsqrt_0x650da000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_SQRT, true, Zd, 99, Zn, Pg);
}
static inline void yasve_fsub_0x65000400 (CTXARG, int Zd, int Zn, int Zm) {
    //NOTTESTED14();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_SUB, false, Zd, Zn, Zm, 99);
}
static inline void yasve_fsub_0x65018000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_SUB, true, Zd, 99, Zm, Pg);
}
static inline void yasve_fsub_0x65198000 (CTXARG, int Zd, int Pg, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    u64 immv = constant_f(esize, ((imm == 0) ? 0.5 : 1.0));
    perform_FOP_imm(zx, esize, Fop_SUB, true, Zd, immv, Pg);
}
static inline void yasve_fsubr_0x65038000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_FOP_z(zx, esize, Fop_SUB_REV, true, Zd, 99, Zm, Pg);
}
static inline void yasve_fsubr_0x651b8000 (CTXARG, int Zd, int Pg, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    u64 immv = constant_f(esize, ((imm == 0) ? 0.5 : 1.0));
    perform_FOP_imm(zx, esize, Fop_SUB_REV, true, Zd, immv, Pg);
}
static inline void yasve_ftmad_0x65108000 (CTXARG, int Zd, int _Zd, int Zm, s64 imm) {
    TBD("ftmad");
}
static inline void yasve_ftsmul_0x65000c00 (CTXARG, int Zd, int Zn, int Zm) {
    TBD("ftsmul");
}
static inline void yasve_ftssel_0x0420b000 (CTXARG, int Zd, int Zn, int Zm) {
    TBD("ftssel");
}

static inline void yasve_incb_0x0430e000 (CTXARG, int Rdn, int pattern, s64 imm) {
    //NOTTESTED01();
    int esize = 8;
    s64 immv = imm + 1;
    perform_INC_x(zx, esize, Rdn, pattern, immv);
}
static inline void yasve_incd_0x04f0c000 (CTXARG, int Zd, int pattern, s64 imm) {
    //NOTTESTED02();
    int esize = 64;
    s64 immv = imm + 1;
    perform_INC_z(zx, esize, Zd, pattern, immv);
}
static inline void yasve_incd_0x04f0e000 (CTXARG, int Rdn, int pattern, s64 imm) {
    //NOTTESTED00();
    int esize = 64;
    s64 immv = imm + 1;
    perform_INC_x(zx, esize, Rdn, pattern, immv);
}
static inline void yasve_inch_0x0470c000 (CTXARG, int Zd, int pattern, s64 imm) {
    NOTTESTED();
    int esize = 16;
    s64 immv = imm + 1;
    perform_INC_z(zx, esize, Zd, pattern, immv);
}
static inline void yasve_inch_0x0470e000 (CTXARG, int Rdn, int pattern, s64 imm) {
    //NOTTESTED00();
    int esize = 16;
    s64 immv = imm + 1;
    perform_INC_x(zx, esize, Rdn, pattern, immv);
}
static inline void yasve_incp_0x252c8000 (CTXARG, int Zd, int Pg) {
    NOTTESTED();
    if (size == 0) {UnallocatedEncoding();}
    int esize = (8 << size);
    perform_INC_z_pred(zx, esize, Zd, Pg, 1);
}
static inline void yasve_incp_0x252c8800 (CTXARG, int Rd, int Pg) {
    NOTTESTED();
    int esize = (8 << size);
    perform_INC_x_pred(zx, esize, Rd, Pg, 1);
}
static inline void yasve_incw_0x04b0c000 (CTXARG, int Zd, int pattern, s64 imm) {
    //NOTTESTED00();
    int esize = 32;
    s64 immv = imm + 1;
    perform_INC_z(zx, esize, Zd, pattern, immv);
}
static inline void yasve_incw_0x04b0e000 (CTXARG, int Rd, int pattern, s64 imm) {
    //NOTTESTED00();
    int esize = 32;
    s64 immv = imm + 1;
    perform_INC_x(zx, esize, Rd, pattern, immv);
}
static inline void yasve_index_0x04204c00 (CTXARG, int Zd, int Rn, int Rm) {
    /* (scalars) */
    NOTTESTED();
    int esize = (8 << size);
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    s64 element1 = (s64)Xreg_get(zx, Rn, X31ZERO);
    s64 element2 = (s64)Xreg_get(zx, Rm, X31ZERO);
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	s64 index = (element1 + e * element2);
	Elem_set(zx, &result, e, esize, (u64)index);
    }
    zx->z[Zd] = result;
}
static inline void yasve_index_0x04204000 (CTXARG, int Zd, s64 imm1, s64 imm2) {
    /* (immediates) */
    /* NOTE: The positions of simm5 and simm5b. */
    //NOTTESTED00();
    int esize = (8 << size);
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	s64 index = (imm1 + e * imm2);
	Elem_set(zx, &result, e, esize, (u64)index);
    }
    zx->z[Zd] = result;
}
static inline void yasve_index_0x04204400 (CTXARG, int Zd, int Rn, s64 imm) {
    /* (scalar, immediate) */
    NOTTESTED();
    int esize = (8 << size);
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    s64 element1 = (s64)Xreg_get(zx, Rn, X31ZERO);
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	s64 index = (element1 + e * imm);
	Elem_set(zx, &result, e, esize, (u64)index);
    }
    zx->z[Zd] = result;
}
static inline void yasve_index_0x04204800 (CTXARG, int Zd, s64 imm, int Rm) {
    /* (immediate, scalar) */
    /* NOTE: The position of simm5b. */
    //NOTTESTED21();
    int esize = (8 << size);
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    s64 element2 = (s64)Xreg_get(zx, Rm, X31ZERO);
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	s64 index = (imm + e * element2);
	Elem_set(zx, &result, e, esize, (u64)index);
    }
    zx->z[Zd] = result;
}
static inline void yasve_insr_0x05243800 (CTXARG, int Zd, int Rm) {
    NOTTESTED();
    int esize = (8 << size);
    CheckSVEEnabled();
    size_t vlen = (size_t)(zx->VL / 8);
    zreg dest = zx->z[Zd];
    u64 src = Xreg_get(zx, Rm, X31ZERO);
    u8 buf[vlen * 2];
    memcpy(&buf[esize / 8], &dest, vlen);
    memcpy(&buf[0], &src, (size_t)(esize / 8));
    memcpy(&dest, &buf[0], vlen);
    zx->z[Zd] = dest;
}
static inline void yasve_insr_0x05343800 (CTXARG, int Zd, int Vm) {
    NOTTESTED();
    int esize = (8 << size);
    CheckSVEEnabled();
    size_t vlen = (size_t)(zx->VL / 8);
    zreg dest = zx->z[Zd];
    u64 src = Vreg_get(zx, Vm);
    u8 buf[vlen * 2];
    memcpy(&buf[esize / 8], &dest, vlen);
    memcpy(&buf[0], &src, (size_t)(esize / 8));
    memcpy(&dest, &buf[0], vlen);
    zx->z[Zd] = dest;
}
static inline void yasve_lasta_0x0520a000 (CTXARG, int Rdn, int Pg, int Zn) {
    /* (scalar) */
    NOTTESTED();
    int esize = (8 << size);
    /*int rsize = (esize < 64 ? 32 : 64);*/
    bool isBefore = false;
    perform_LAST_xv(zx, esize, false, isBefore, XREG, Rdn, Zn, Pg);
}
static inline void yasve_lasta_0x05228000 (CTXARG, int Vd, int Pg, int Zn) {
    /* (SIMD&FP scalar) */
    NOTTESTED();
    int esize = (8 << size);
    bool isBefore = false;
    perform_LAST_xv(zx, esize, false, isBefore, VREG, Vd, Zn, Pg);
}
static inline void yasve_lastb_0x0521a000 (CTXARG, int Rdn, int Pg, int Zn) {
    /* (scalar) */
    NOTTESTED();
    int esize = (8 << size);
    /*int rsize = (esize < 64 ? 32 : 64);*/
    bool isBefore = true;
    perform_LAST_xv(zx, esize, false, isBefore, XREG, Rdn, Zn, Pg);
}
static inline void yasve_lastb_0x05238000 (CTXARG, int Vd, int Pg, int Zn) {
    /* (SIMD&FP scalar) */
    NOTTESTED();
    int esize = (8 << size);
    bool isBefore = true;
    perform_LAST_xv(zx, esize, false, isBefore, VREG, Vd, Zn, Pg);
}

static inline void yasve_ld1b_0xa4004000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/8-bit element) */
    /* operand: SVE_ADDR_RX=(Rn, Rm, 0, 0) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 8;
    int msize = 8;
    bool unsignedp = true;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld1b_0xa400a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/8-bit element) */
    /* operand: SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 8;
    int msize = 8;
    bool unsignedp = true;
    perform_LD1_x_imm(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1b_0xc4004000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* operand: SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = true;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1b_0xc440c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* operand: SVE_ADDR_RZ=(Rn, Zm, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = true;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1b_0xc420c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/64-bit element) */
    /* operand: SVE_ADDR_ZI_U5=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = true;
    perform_LD1_z_imm(zx, esize, msize, unsignedp, Zt, Zn, imm, Pg);
}
static inline void yasve_ld1b_0xa4604000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/64-bit element) */
    /* operand: SVE_ADDR_RX=(Rn, Rm, 0, 0) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = true;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld1b_0xa460a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/64-bit element) */
    /* operand: SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = true;
    perform_LD1_x_imm(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1b_0xa4204000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/16-bit element) */
    /* operand: SVE_ADDR_RX=(Rn, Rm, 0, 0) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 16;
    int msize = 8;
    bool unsignedp = true;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld1b_0xa420a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/16-bit element) */
    /* operand: SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    int msize = 8;
    bool unsignedp = true;
    perform_LD1_x_imm(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1b_0x84004000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _0) {
    /* (scalar plus vector/32-bit unscaled offset) */
    /* operand: SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = true;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1b_0x8420c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _0) {
    /* (vector plus immediate/32-bit element) */
    /* operand: SVE_ADDR_ZI_U5=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = true;
    perform_LD1_z_imm(zx, esize, msize, unsignedp, Zt, Zn, imm, Pg);
}
static inline void yasve_ld1b_0xa4404000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/32-bit element) */
    /* operand: SVE_ADDR_RX=(Rn, Rm, 0, 0) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = true;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld1b_0xa440a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/32-bit element) */
    /* operand: SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = true;
    perform_LD1_x_imm(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}

static inline void yasve_ld1h_0xc4804000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int lsl) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* operand: SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = true;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1h_0xc4a04000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int lsl) {
    /* (scalar plus vector/32-bit unpacked scaled offset) */
    /* operand: SVE_ADDR_RZ_XTW1_22=(Rn, Zm/32, opr_xs22, LSL=1) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL1);
    NOTTESTED();
    int esize = 63;
    int msize = 16;
    bool unsignedp = true;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 1;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1h_0xc4c0c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int lsl) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* operand: SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = true;
    int offs_size = 32;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1h_0xc4e0c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int lsl) {
    /* (scalar plus vector/64-bit scaled offset) */
    /* operand: SVE_ADDR_RZ_LSL1=(Rn, Zm/64, opr_xs22, LSL=1) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL1);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = true;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 1;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1h_0xc4a0c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int lsl) {
    /* (vector plus immediate/64-bit element) */
    /* operand: SVE_ADDR_ZI_U5x2=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = true;
    perform_LD1_z_imm(zx, esize, msize, unsignedp, Zt, Zn, imm, Pg);
}
static inline void yasve_ld1h_0xa4e04000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/64-bit element) */
    /* operand: SVE_ADDR_RX_LSL1=(Rn, Rm, 0, LSL=1) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = true;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld1h_0xa4e0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/64-bit element) */
    /* operand: SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = true;
    perform_LD1_x_imm(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1h_0xa4a04000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/16-bit element) */
    /* operand: SVE_ADDR_RX_LSL1=(Rn, Rm, 0, LSL=1) */
    assert(mo == OPR_Rn_Rm_LSL1);
    //NOTTESTED00();
    int esize = 16;
    int msize = 16;
    bool unsignedp = true;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld1h_0xa4a0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/16-bit element) */
    /* operand: SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    int msize = 16;
    bool unsignedp = true;
    perform_LD1_x_imm(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1h_0x84804000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int lsl) {
    /* (scalar plus vector/32-bit unscaled offset) */
    /* operand: SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = true;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1h_0x84a04000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int lsl) {
    /* (scalar plus vector/32-bit scaled offset) */
    /* operand: SVE_ADDR_RZ_XTW1_22=(Rn, Zm/32, opr_xs22, LSL=1) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL1);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = true;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 1;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1h_0x84a0c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int lsl) {
    /* (vector plus immediate/32-bit element) */
    /* operand: SVE_ADDR_ZI_U5x2=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = true;
    perform_LD1_z_imm(zx, esize, msize, unsignedp, Zt, Zn, imm, Pg);
}
static inline void yasve_ld1h_0xa4c04000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/32-bit element) */
    /* operand: SVE_ADDR_RX_LSL1=(Rn, Rm, 0, LSL=1) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = true;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld1h_0xa4c0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/32-bit element) */
    /* operand: SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = true;
    perform_LD1_x_imm(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}

static inline void yasve_ld1d_0xc5804000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int lsl) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* operand: SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    bool unsignedp = true;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1d_0xc5a04000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int lsl) {
    /* (scalar plus vector/32-bit unpacked scaled offset) */
    /* operand: SVE_ADDR_RZ_XTW3_22=(Rn, Zm/32, opr_xs22, LSL=3) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL3);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    bool unsignedp = true;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 3;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1d_0xc5c0c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int _0, int lsl) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* operand: SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    //NOTTESTED21();
    int esize = 64;
    int msize = 64;
    bool unsignedp = true;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1d_0xc5e0c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int _0, int lsl) {
    /* (scalar plus vector/64-bit scaled offset) */
    /* operand: SVE_ADDR_RZ_LSL3=(Rn, Zm/64, opr_xs22, LSL=3) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL3);
    //NOTTESTED00();
    int esize = 64;
    int msize = 64;
    bool unsignedp = true;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 3;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1d_0xc5a0c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int _0, int lsl) {
    /* (vector plus immediate) */
    /* operand: SVE_ADDR_ZI_U5x8=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    bool unsignedp = true;
    perform_LD1_z_imm(zx, esize, msize, unsignedp, Zt, Zn, imm, Pg);
}
static inline void yasve_ld1d_0xa5e04000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int lsl) {
    /* (scalar plus scalar) */
    /* operand: SVE_ADDR_RX_LSL3=(Rn, Rm, 0, LSL=3) */
    assert(mo == OPR_Rn_Rm_LSL3);
    //NOTTESTED00();
    int esize = 64;
    int msize = 64;
    bool unsignedp = true;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld1d_0xa5e0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int lsl) {
    /* (scalar plus immediate) */
    /* operand: SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    bool unsignedp = true;
    perform_LD1_x_imm(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}

static inline void yasve_ld1rb_0x84408000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (8-bit element) */
    /* operand: SVE_ADDR_RI_U6=(Rn, opr_uimm6, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 8;
    int msize = 8;
    bool unsignedp = true;
    perform_LD1R(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1rb_0x8440e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (64-bit element) */
    /* operand: SVE_ADDR_RI_U6=(Rn, opr_uimm6, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = true;
    perform_LD1R(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1rb_0x8440a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (16-bit element) */
    /* operand: SVE_ADDR_RI_U6=(Rn, opr_uimm6, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    int msize = 8;
    bool unsignedp = true;
    perform_LD1R(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1rb_0x8440c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (32-bit element) */
    /* operand: SVE_ADDR_RI_U6=(Rn, opr_uimm6, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = true;
    perform_LD1R(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}

static inline void yasve_ld1rd_0x85c0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* operand: SVE_ADDR_RI_U6x8=(Rn, opr_uimm6, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    //NOTTESTED18();
    int esize = 64;
    int msize = 64;
    bool unsignedp = true;
    perform_LD1R(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}

static inline void yasve_ld1rh_0x84c0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (64-bit element) */
    /* operand: SVE_ADDR_RI_U6x2=(Rn, opr_uimm6, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = true;
    perform_LD1R(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1rh_0x84c0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (16-bit element) */
    /* operand: SVE_ADDR_RI_U6x2=(Rn, opr_uimm6, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    int msize = 16;
    bool unsignedp = true;
    perform_LD1R(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1rh_0x84c0c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (32-bit element) */
    /* operand: SVE_ADDR_RI_U6x2=(Rn, opr_uimm6, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = true;
    perform_LD1R(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}

static inline void yasve_ld1rqb_0xa4002000 (TBDARG) {TBD("ld1rqb");}
static inline void yasve_ld1rqb_0xa4000000 (TBDARG) {TBD("ld1rqb");}
static inline void yasve_ld1rqd_0xa5802000 (TBDARG) {TBD("ld1rqd");}
static inline void yasve_ld1rqd_0xa5800000 (TBDARG) {TBD("ld1rqd");}
static inline void yasve_ld1rqh_0xa4802000 (TBDARG) {TBD("ld1rqh");}
static inline void yasve_ld1rqh_0xa4800000 (TBDARG) {TBD("ld1rqh");}
static inline void yasve_ld1rqw_0xa5002000 (TBDARG) {TBD("ld1rqw");}
static inline void yasve_ld1rqw_0xa5000000 (TBDARG) {TBD("ld1rqw");}

static inline void yasve_ld1rsb_0x85c08000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (64-bit element) */
    /* operand: SVE_ADDR_RI_U6=(Rn, opr_uimm6, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = false;
    perform_LD1R(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1rsb_0x85c0c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (16-bit element) */
    /* operand: SVE_ADDR_RI_U6=(Rn, opr_uimm6, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    int msize = 8;
    bool unsignedp = false;
    perform_LD1R(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1rsb_0x85c0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (32-bit element) */
    /* operand: SVE_ADDR_RI_U6=(Rn, opr_uimm6, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = false;
    perform_LD1R(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}

static inline void yasve_ld1rsh_0x85408000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (64-bit element) */
    /* operand: SVE_ADDR_RI_U6x2=(Rn, opr_uimm6, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = false;
    perform_LD1R(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1rsh_0x8540a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (32-bit element) */
    /* operand: SVE_ADDR_RI_U6x2=(Rn, opr_uimm6, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = false;
    perform_LD1R(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}

static inline void yasve_ld1rsw_0x84c08000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* operand: SVE_ADDR_RI_U6x4=(Rn, opr_uimm6, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = false;
    perform_LD1R(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}

static inline void yasve_ld1rw_0x8540e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (64-bit element) */
    /* SVE_ADDR_RI_U6x4=(Rn, opr_uimm6, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = true;
    perform_LD1R(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1rw_0x8540c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (32-bit element) */
    /* SVE_ADDR_RI_U6x4=(Rn, opr_uimm6, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    //NOTTESTED00();
    int esize = 32;
    int msize = 32;
    bool unsignedp = true;
    perform_LD1R(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}

static inline void yasve_ld1sb_0xc4000000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = false;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1sb_0xc4408000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = false;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1sb_0xc4208000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/64-bit element) */
    /* SVE_ADDR_ZI_U5=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = false;
    perform_LD1_z_imm(zx, esize, msize, unsignedp, Zt, Zn, imm, Pg);
}
static inline void yasve_ld1sb_0xa5804000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/64-bit element) */
    /* SVE_ADDR_RX=(Rn, Rm, 0, 0) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = false;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld1sb_0xa580a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/64-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = false;
    perform_LD1_x_imm(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1sb_0xa5c04000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/16-bit element) */
    /* SVE_ADDR_RX=(Rn, Rm, 0, 0) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 16;
    int msize = 8;
    bool unsignedp = false;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld1sb_0xa5c0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/16-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    int msize = 8;
    bool unsignedp = false;
    perform_LD1_x_imm(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1sb_0x84000000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = false;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1sb_0x84208000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/32-bit element) */
    /* SVE_ADDR_ZI_U5=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = false;
    perform_LD1_z_imm(zx, esize, msize, unsignedp, Zt, Zn, imm, Pg);
}
static inline void yasve_ld1sb_0xa5a04000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/32-bit element) */
    /* SVE_ADDR_RX=(Rn, Rm, 0, 0) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = false;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld1sb_0xa5a0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/32-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = false;
    perform_LD1_x_imm(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}

static inline void yasve_ld1sh_0xc4800000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = false;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1sh_0xc4a00000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked scaled offset) */
    /* SVE_ADDR_RZ_XTW1_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL1);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = false;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 1;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1sh_0xc4c08000 (CTXARG, int Zt, int Pg, svemo_t mo,
int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = false;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1sh_0xc4e08000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit scaled offset) */
    /* SVE_ADDR_RZ_LSL1=(Rn, Zm/64, opr_xs22, LSL=1) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL1);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = false;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 1;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1sh_0xc4a08000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/64-bit element) */
    /* SVE_ADDR_ZI_U5x2=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = false;
    perform_LD1_z_imm(zx, esize, msize, unsignedp, Zt, Zn, imm, Pg);
}
static inline void yasve_ld1sh_0xa5004000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/64-bit element) */
    /* SVE_ADDR_RX_LSL1=(Rn, Rm, 0, LSL=1) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = false;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld1sh_0xa500a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/64-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = false;
    perform_LD1_x_imm(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1sh_0x84800000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = false;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1sh_0x84a00000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit scaled offset) */
    /* SVE_ADDR_RZ_XTW1_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL1);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = false;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 1;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1sh_0x84a08000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/32-bit element) */
    /* SVE_ADDR_ZI_U5x2=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = false;
    perform_LD1_z_imm(zx, esize, msize, unsignedp, Zt, Zn, imm, Pg);
}
static inline void yasve_ld1sh_0xa5204000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/32-bit element) */
    /* SVE_ADDR_RX_LSL1=(Rn, Rm, 0, LSL=1) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = false;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld1sh_0xa520a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/32-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = false;
    perform_LD1_x_imm(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}

static inline void yasve_ld1sw_0xc5000000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = false;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1sw_0xc5200000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked scaled offset) */
    /* SVE_ADDR_RZ_XTW2_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL2);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = false;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 2;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1sw_0xc5408000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = false;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1sw_0xc5608000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit scaled offset) */
    /* SVE_ADDR_RZ_LSL2=(Rn, Zm/64, opr_xs22, LSL=2) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL2);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = false;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 2;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1sw_0xc5208000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate) */
    /* SVE_ADDR_ZI_U5x4=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = false;
    perform_LD1_z_imm(zx, esize, msize, unsignedp, Zt, Zn, imm, Pg);
}
static inline void yasve_ld1sw_0xa4804000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL2=(Rn, Rm, 0, LSL=2) */
    assert(mo == OPR_Rn_Rm_LSL2);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = false;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld1sw_0xa480a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = false;
    perform_LD1_x_imm(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}

static inline void yasve_ld1w_0xc5004000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = true;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1w_0xc5204000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked scaled offset) */
    /* SVE_ADDR_RZ_XTW2_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL2);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = true;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 2;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1w_0xc540c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = true;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1w_0xc560c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit scaled offset) */
    /* SVE_ADDR_RZ_LSL2=(Rn, Zm/64, opr_xs22, LSL=2) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL2);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = true;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 2;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1w_0xc520c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/64-bit element) */
    /* SVE_ADDR_ZI_U5x4=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = true;
    perform_LD1_z_imm(zx, esize, msize, unsignedp, Zt, Zn, imm, Pg);
}
static inline void yasve_ld1w_0xa5604000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/64-bit element) */
    /* SVE_ADDR_RX_LSL2=(Rn, Rm, 0, LSL=1) */
    assert(mo == OPR_Rn_Rm_LSL2);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = true;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld1w_0xa560a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/64-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = true;
    perform_LD1_x_imm(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}
static inline void yasve_ld1w_0x85004000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 32;
    bool unsignedp = true;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1w_0x85204000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit scaled offset) */
    /* SVE_ADDR_RZ_XTW2_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL2);
    //NOTTESTED00();
    int esize = 32;
    int msize = 32;
    bool unsignedp = true;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 2;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ld1w_0x8520c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/32-bit element) */
    /* SVE_ADDR_ZI_U5x4=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 32;
    bool unsignedp = true;
    perform_LD1_z_imm(zx, esize, msize, unsignedp, Zt, Zn, imm, Pg);
}
static inline void yasve_ld1w_0xa5404000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/32-bit element) */
    /* SVE_ADDR_RX_LSL2=(Rn, Rm, 0, LSL=2) */
    assert(mo == OPR_Rn_Rm_LSL2);
    //NOTTESTED00();
    int esize = 32;
    int msize = 32;
    bool unsignedp = true;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld1w_0xa540a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/32-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 32;
    bool unsignedp = true;
    perform_LD1_x_imm(zx, esize, msize, unsignedp, Zt, Rn, imm, Pg);
}

static inline void yasve_ld2b_0xa420c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX=(Rn, Rm, 0, 0) */
    NOTTESTED();
    assert(mo == OPR_Rn_Rm_LSL0);
    int esize = 8;
    int nreg = 2;
    perform_LD234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld2b_0xa420e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x2xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 8;
    int nreg = 2;
    perform_LD234_x_imm(zx, esize, nreg, Zt, Rn, imm, Pg);
}

static inline void yasve_ld2d_0xa5a0c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL3=(Rn, Rm, 0, LSL=3) */
    assert(mo == OPR_Rn_Rm_LSL3);
    NOTTESTED();
    int esize = 64;
    int nreg = 2;
    perform_LD234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld2d_0xa5a0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x2xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int nreg = 2;
    perform_LD234_x_imm(zx, esize, nreg, Zt, Rn, imm, Pg);
}

static inline void yasve_ld2h_0xa4a0c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL1=(Rn, Rm, 0, LSL=1) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 16;
    int nreg = 2;
    perform_LD234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld2h_0xa4a0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x2xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    int nreg = 2;
    perform_LD234_x_imm(zx, esize, nreg, Zt, Rn, imm, Pg);
}

static inline void yasve_ld2w_0xa520c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL2=(Rn, Rm, 0, LSL=2) */
    assert(mo == OPR_Rn_Rm_LSL2);
    NOTTESTED();
    int esize = 32;
    int nreg = 2;
    perform_LD234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld2w_0xa520e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x2xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int nreg = 2;
    perform_LD234_x_imm(zx, esize, nreg, Zt, Rn, imm, Pg);
}

static inline void yasve_ld3b_0xa440c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX=(Rn, Rm, 0, 0) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 8;
    int nreg = 3;
    perform_LD234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld3b_0xa440e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x3xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 8;
    int nreg = 3;
    perform_LD234_x_imm(zx, esize, nreg, Zt, Rn, imm, Pg);
}

static inline void yasve_ld3d_0xa5c0c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL3=(Rn, Rm, 0, LSL=3) */
    assert(mo == OPR_Rn_Rm_LSL3);
    NOTTESTED();
    int esize = 64;
    int nreg = 3;
    perform_LD234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld3d_0xa5c0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x3xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int nreg = 3;
    perform_LD234_x_imm(zx, esize, nreg, Zt, Rn, imm, Pg);
}

static inline void yasve_ld3h_0xa4c0c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL1=(Rn, Rm, 0, LSL=1) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 16;
    int nreg = 3;
    perform_LD234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld3h_0xa4c0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x3xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    int nreg = 3;
    perform_LD234_x_imm(zx, esize, nreg, Zt, Rn, imm, Pg);
}

static inline void yasve_ld3w_0xa540c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL2=(Rn, Rm, 0, LSL=2) */
    assert(mo == OPR_Rn_Rm_LSL2);
    NOTTESTED();
    int esize = 32;
    int nreg = 3;
    perform_LD234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld3w_0xa540e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x3xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int nreg = 3;
    perform_LD234_x_imm(zx, esize, nreg, Zt, Rn, imm, Pg);
}

static inline void yasve_ld4b_0xa460c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX=(Rn, Rm, 0, 0) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 8;
    int nreg = 4;
    perform_LD234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld4b_0xa460e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 8;
    int nreg = 4;
    perform_LD234_x_imm(zx, esize, nreg, Zt, Rn, imm, Pg);
}

static inline void yasve_ld4d_0xa5e0c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL3=(Rn, Rm, 0, LSL=3) */
    assert(mo == OPR_Rn_Rm_LSL3);
    NOTTESTED();
    int esize = 64;
    int nreg = 4;
    perform_LD234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld4d_0xa5e0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int nreg = 4;
    perform_LD234_x_imm(zx, esize, nreg, Zt, Rn, imm, Pg);
}

static inline void yasve_ld4h_0xa4e0c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL1=(Rn, Rm, 0, LSL=1) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 16;
    int nreg = 4;
    perform_LD234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld4h_0xa4e0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    int nreg = 4;
    perform_LD234_x_imm(zx, esize, nreg, Zt, Rn, imm, Pg);
}

static inline void yasve_ld4w_0xa560c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL2=(Rn, Rm, 0, LSL=2) */
    assert(mo == OPR_Rn_Rm_LSL2);
    NOTTESTED();
    int esize = 32;
    int nreg = 4;
    perform_LD234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_ld4w_0xa560e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int nreg = 4;
    perform_LD234_x_imm(zx, esize, nreg, Zt, Rn, imm, Pg);
}

static inline void yasve_ldff1b_0xa4006000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/8-bit element) */
    /* SVE_ADDR_RR=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 8;
    int msize = 8;
    bool unsignedp = true;
    perform_LDFF_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ldff1b_0xc4006000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    int offs_size = 32;
    bool unsignedp = true;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1b_0xc440e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    int offs_size = 64;
    bool unsignedp = true;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1b_0xc420e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/64-bit element) */
    /* SVE_ADDR_ZI_U5=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDFF_z_imm(zx, esize, msize, unsignedp,
		       Zt, Zn, offset, Pg);
}
static inline void yasve_ldff1b_0xa4606000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/64-bit element) */
    /* SVE_ADDR_RR=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = true;
    perform_LDFF_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ldff1b_0xa4206000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/16-bit element) */
    /* SVE_ADDR_RR=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 16;
    int msize = 8;
    bool unsignedp = true;
    perform_LDFF_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ldff1b_0x84006000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    int offs_size = 32;
    bool unsignedp = true;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1b_0x8420e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int _0, int _1) {
    /* (vector plus immediate/32-bit element) */
    /* SVE_ADDR_ZI_U5=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDFF_z_imm(zx, esize, msize, unsignedp,
		       Zt, Zn, offset, Pg);
}
static inline void yasve_ldff1b_0xa4406000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/32-bit element) */
    /* SVE_ADDR_RR=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = true;
    perform_LDFF_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}

static inline void yasve_ldff1d_0xc5806000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    int offs_size = 32;
    bool unsignedp = true;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1d_0xc5a06000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked scaled offset) */
    /* SVE_ADDR_RZ_XTW3_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL3);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    int offs_size = 32;
    bool unsignedp = true;
    bool offs_unsignedp = (xs == 0);
    int scale = 3;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1d_0xc5c0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    int offs_size = 64;
    bool unsignedp = true;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1d_0xc5e0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit scaled offset) */
    /* SVE_ADDR_RZ_LSL3=(Rn, Zm/64, opr_xs22, LSL=3) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL3);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    int offs_size = 64;
    bool unsignedp = true;
    bool offs_unsignedp = (xs == 0);
    int scale = 3;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1d_0xc5a0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int _0, int _1) {
    /* (vector plus immediate) */
    /* SVE_ADDR_ZI_U5x8=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDFF_z_imm(zx, esize, msize, unsignedp,
		       Zt, Zn, offset, Pg);
}
static inline void yasve_ldff1d_0xa5e06000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RR_LSL3=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL3);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    bool unsignedp = true;
    perform_LDFF_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}

static inline void yasve_ldff1h_0xc4806000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    int offs_size = 32;
    bool unsignedp = true;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1h_0xc4a06000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked scaled offset) */
    /* SVE_ADDR_RZ_XTW1_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL1);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    int offs_size = 32;
    bool unsignedp = true;
    bool offs_unsignedp = (xs == 0);
    int scale = 1;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1h_0xc4c0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    int offs_size = 64;
    bool unsignedp = true;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1h_0xc4e0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit scaled offset) */
    /* SVE_ADDR_RZ_LSL1=(Rn, Zm/64, opr_xs22, LSL=1) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL1);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    int offs_size = 64;
    bool unsignedp = true;
    bool offs_unsignedp = true;
    int scale = 1;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1h_0xc4a0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/64-bit element) */
    /* SVE_ADDR_ZI_U5x2=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDFF_z_imm(zx, esize, msize, unsignedp,
		       Zt, Zn, offset, Pg);
}
static inline void yasve_ldff1h_0xa4e06000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/64-bit element) */
    /* SVE_ADDR_RR_LSL1=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = true;
    perform_LDFF_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ldff1h_0xa4a06000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/16-bit element) */
    /* SVE_ADDR_RR_LSL1=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 16;
    int msize = 16;
    bool unsignedp = true;
    perform_LDFF_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ldff1h_0x84806000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    int offs_size = 32;
    bool unsignedp = true;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1h_0x84a06000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit scaled offset) */
    /* SVE_ADDR_RZ_XTW1_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL1);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    int offs_size = 32;
    bool unsignedp = true;
    bool offs_unsignedp = (xs == 0);
    int scale = 1;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1h_0x84a0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/32-bit element) */
    /* SVE_ADDR_ZI_U5x2=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDFF_z_imm(zx, esize, msize, unsignedp,
		       Zt, Zn, offset, Pg);
}
static inline void yasve_ldff1h_0xa4c06000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/32-bit element) */
    /* SVE_ADDR_RR_LSL1=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = true;
    perform_LDFF_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}

static inline void yasve_ldff1sb_0xc4002000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = false;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1sb_0xc440a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = false;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1sb_0xc420a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/64-bit element) */
    /* SVE_ADDR_ZI_U5=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = false;
    perform_LD1_z_imm(zx, esize, msize, unsignedp, Zt, Zn, imm, Pg);
}
static inline void yasve_ldff1sb_0xa5806000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/64-bit element) */
    /* SVE_ADDR_RR=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = false;
    perform_LD1_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ldff1sb_0xa5c06000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/16-bit element) */
    /* SVE_ADDR_RR=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 16;
    int msize = 8;
    bool unsignedp = false;
    perform_LDFF_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ldff1sb_0x84002000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = false;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LD1_x_z(zx, esize, msize, unsignedp,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1sb_0x8420a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/32-bit element) */
    /* SVE_ADDR_ZI_U5=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = false;
    s64 offset = imm;
    perform_LDFF_z_imm(zx, esize, msize, unsignedp,
		       Zt, Zn, offset, Pg);
}
static inline void yasve_ldff1sb_0xa5a06000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/32-bit element) */
    /* SVE_ADDR_RR=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = false;
    perform_LDFF_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}

static inline void yasve_ldff1sh_0xc4802000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    int offs_size = 32;
    bool unsignedp = false;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1sh_0xc4a02000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked scaled offset) */
    /* SVE_ADDR_RZ_XTW1_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL1);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    int offs_size = 32;
    bool unsignedp = false;
    bool offs_unsignedp = (xs == 0);
    int scale = 1;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1sh_0xc4c0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    int offs_size = 64;
    bool unsignedp = false;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1sh_0xc4e0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit scaled offset) */
    /* SVE_ADDR_RZ_LSL1=(Rn, Zm/64, opr_xs22, LSL=1) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL1);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    int offs_size = 64;
    bool unsignedp = false;
    bool offs_unsignedp = true;
    int scale = 1;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1sh_0xc4a0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/64-bit element) */
    /* SVE_ADDR_ZI_U5x2=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = false;
    s64 offset = imm;
    perform_LDFF_z_imm(zx, esize, msize, unsignedp,
		       Zt, Zn, offset, Pg);
}
static inline void yasve_ldff1sh_0xa5006000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/64-bit element) */
    /* SVE_ADDR_RR_LSL1=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = false;
    perform_LDFF_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ldff1sh_0x84802000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    int offs_size = 32;
    bool unsignedp = false;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1sh_0x84a02000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit scaled offset) */
    /* SVE_ADDR_RZ_XTW1_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL1);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    int offs_size = 32;
    bool unsignedp = false;
    bool offs_unsignedp = (xs == 0);
    int scale = 1;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1sh_0x84a0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/32-bit element) */
    /* SVE_ADDR_ZI_U5x2=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = false;
    s64 offset = imm;
    perform_LDFF_z_imm(zx, esize, msize, unsignedp,
		       Zt, Zn, offset, Pg);
}
static inline void yasve_ldff1sh_0xa5206000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/32-bit element) */
    /* SVE_ADDR_RR_LSL1=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = false;
    perform_LDFF_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}

static inline void yasve_ldff1sw_0xc5002000 (CTXARG, int Zt, int Pg, svemo_t mo
, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    int offs_size = 32;
    bool unsignedp = false;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1sw_0xc5202000 (CTXARG, int Zt, int Pg, svemo_t mo
, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked scaled offset) */
    /* SVE_ADDR_RZ_XTW2_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL2);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    int offs_size = 32;
    bool unsignedp = false;
    bool offs_unsignedp = (xs == 0);
    int scale = 2;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1sw_0xc540a000 (CTXARG, int Zt, int Pg, svemo_t mo
, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    int offs_size = 64;
    bool unsignedp = false;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1sw_0xc560a000 (CTXARG, int Zt, int Pg, svemo_t mo
, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit scaled offset) */
    /* SVE_ADDR_RZ_LSL2=(Rn, Zm/64, opr_xs22, LSL=2) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL2);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    int offs_size = 64;
    bool unsignedp = false;
    bool offs_unsignedp = true;
    int scale = 2;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1sw_0xc520a000 (CTXARG, int Zt, int Pg, svemo_t mo
, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate) */
    /* SVE_ADDR_ZI_U5x4=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = false;
    s64 offset = imm;
    perform_LDFF_z_imm(zx, esize, msize, unsignedp,
		       Zt, Zn, offset, Pg);
}
static inline void yasve_ldff1sw_0xa4806000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RR_LSL2=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL2);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = false;
    perform_LDFF_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}

static inline void yasve_ldff1w_0xc5006000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    int offs_size = 32;
    bool unsignedp = true;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1w_0xc5206000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked scaled offset) */
    /* SVE_ADDR_RZ_XTW2_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL2);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    int offs_size = 32;
    bool unsignedp = true;
    bool offs_unsignedp = (xs == 0);
    int scale = 2;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1w_0xc540e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    int offs_size = 64;
    bool unsignedp = true;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1w_0xc560e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit scaled offset) */
    /* SVE_ADDR_RZ_LSL2=(Rn, Zm/64, opr_xs22, LSL=2) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL2);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    int offs_size = 64;
    bool unsignedp = true;
    bool offs_unsignedp = true;
    int scale = 2;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1w_0xc520e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/64-bit element) */
    /* SVE_ADDR_ZI_U5x4=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDFF_z_imm(zx, esize, msize, unsignedp,
		       Zt, Zn, offset, Pg);
}
static inline void yasve_ldff1w_0xa5606000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/64-bit element) */
    /* SVE_ADDR_RR_LSL2=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL2);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = true;
    perform_LDFF_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}
static inline void yasve_ldff1w_0x85006000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unscaled offset) */
    /* SVE_ADDR_RZ_XTW_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 32;
    int offs_size = 32;
    bool unsignedp = true;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1w_0x85206000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit scaled offset) */
    /* SVE_ADDR_RZ_XTW2_22=(Rn, Zm/32, opr_xs22, 0) */
    assert(mo == OPR_Rn_Zm32_XS22_LSL2);
    NOTTESTED();
    int esize = 32;
    int msize = 32;
    int offs_size = 32;
    bool unsignedp = true;
    bool offs_unsignedp = (xs == 0);
    int scale = 2;
    perform_LDFF_x_z(zx, esize, msize, unsignedp,
		     offs_size, offs_unsignedp, scale,
		     Zt, Rn, Zm, Pg);
}
static inline void yasve_ldff1w_0x8520e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/32-bit element) */
    /* SVE_ADDR_ZI_U5x4=(Zn, opr_uimm5, 0, 0) */
    NOTTESTED();
    assert(mo == OPR_ZnSS_IMM);
    int esize = 32;
    int msize = 32;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDFF_z_imm(zx, esize, msize, unsignedp,
		       Zt, Zn, offset, Pg);
}
static inline void yasve_ldff1w_0xa5406000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/32-bit element) */
    /* SVE_ADDR_RR_LSL2=(Rn, Rm, 0, lsl) */
    NOTTESTED();
    assert(mo == OPR_Rn_Rm_LSL2);
    int esize = 32;
    int msize = 32;
    bool unsignedp = true;
    perform_LDFF_x_x(zx, esize, msize, unsignedp, Zt, Rn, Rm, Pg);
}

static inline void yasve_ldnf1b_0xa410a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (8-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 8;
    int msize = 8;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDNF(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg);
}
static inline void yasve_ldnf1b_0xa430a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (16-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    int msize = 8;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDNF(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg);
}
static inline void yasve_ldnf1b_0xa450a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (32-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDNF(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg);
}
static inline void yasve_ldnf1b_0xa470a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (64-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDNF(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg);
}

static inline void yasve_ldnf1d_0xa5f0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDNF(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg);
}

static inline void yasve_ldnf1h_0xa4b0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (16-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    int msize = 16;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDNF(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg);
}
static inline void yasve_ldnf1h_0xa4d0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (32-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDNF(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg);
}
static inline void yasve_ldnf1h_0xa4f0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (64-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDNF(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg);
}
static inline void yasve_ldnf1sb_0xa590a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (64-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    bool unsignedp = false;
    s64 offset = imm;
    perform_LDNF(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg);
}
static inline void yasve_ldnf1sb_0xa5b0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (32-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    bool unsignedp = false;
    s64 offset = imm;
    perform_LDNF(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg);
}
static inline void yasve_ldnf1sb_0xa5d0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (16-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    int msize = 8;
    bool unsignedp = false;
    s64 offset = imm;
    perform_LDNF(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg);
}
static inline void yasve_ldnf1sh_0xa510a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (64-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    bool unsignedp = false;
    s64 offset = imm;
    perform_LDNF(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg);
}
static inline void yasve_ldnf1sh_0xa530a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (32-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    bool unsignedp = false;
    s64 offset = imm;
    perform_LDNF(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg);
}
static inline void yasve_ldnf1sw_0xa490a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = false;
    s64 offset = imm;
    perform_LDNF(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg);
}
static inline void yasve_ldnf1w_0xa550a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (32-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 32;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDNF(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg);
}
static inline void yasve_ldnf1w_0xa570a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (64-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    bool unsignedp = true;
    s64 offset = imm;
    perform_LDNF(zx, esize, msize, unsignedp, Zt, Rn, offset, Pg);
}

static inline void yasve_ldnt1b_0xa400c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX=(Rn, Rm, 0, 0) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 8;
    int msize = 8;
    perform_LDNT_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_ldnt1b_0xa400e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 8;
    int msize = 8;
    s64 offset = imm;
    perform_LDNT_x_imm(zx, esize, msize, Zt, Rn, offset, Pg);
}
static inline void yasve_ldnt1d_0xa580c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL3=(Rn, Rm, 0, LSL=3) */
    assert(mo == OPR_Rn_Rm_LSL3);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    perform_LDNT_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_ldnt1d_0xa580e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    s64 offset = imm;
    perform_LDNT_x_imm(zx, esize, msize, Zt, Rn, offset, Pg);
}
static inline void yasve_ldnt1h_0xa480c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL1=(Rn, Rm, 0, LSL=1) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 16;
    int msize = 16;
    perform_LDNT_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_ldnt1h_0xa480e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    int msize = 16;
    s64 offset = imm;
    perform_LDNT_x_imm(zx, esize, msize, Zt, Rn, offset, Pg);
}
static inline void yasve_ldnt1w_0xa500c000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL2=(Rn, Rm, 0, LSL=2) */
    assert(mo == OPR_Rn_Rm_LSL2);
    NOTTESTED();
    int esize = 32;
    int msize = 32;
    perform_LDNT_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_ldnt1w_0xa500e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, LSL=0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 32;
    s64 offset = imm;
    perform_LDNT_x_imm(zx, esize, msize, Zt, Rn, offset, Pg);
}

static inline void yasve_ldr_0x85800000 (CTXARG, int Pt, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* operand: SVE_ADDR_RI_S9xVL=(Rn, opr_simm9, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    perform_LDR_p(zx, Pt, Rn, imm);
}
static inline void yasve_ldr_0x85804000 (CTXARG, int Zt, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* operand: SVE_ADDR_RI_S9xVL=(Rn, opr_simm9, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    perform_LDR_z(zx, Zt, Rn, imm);
}

static inline void yasve_lsl_0x04208c00 (CTXARG, int Zd, int Zn, int Zm) {
    /* (wide elements, unpredicated) */
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    perform_IOP_z_wide2nd(zx, esize, true, Iop_LSH_L, Zd, Zn, Zm);
}
static inline void yasve_lsl_0x04209c00 (CTXARG, int Zd, int Zn, s64 imm) {
    //NOTTESTED00();
    int esize = esize_for_shift(imm);
    u64 shift = (u64)(imm - esize);
    perform_IOP_z_imm(zx, esize, Iop_LSH_L, Zd, Zn, shift);
}
static inline void yasve_lsl_0x04138000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_LSH_L, Zd, Zm, Pg);
}
static inline void yasve_lsl_0x041b8000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    /* (wide elements, predicated) */
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    perform_IOP_z_pred_wide2nd(zx, esize, true, Iop_LSH_L, Zd, Zm, Pg);
}
static inline void yasve_lsl_0x04038000 (CTXARG, int Zd, int Pg, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = esize_for_shift(imm);
    u64 shift = (u64)(imm - esize);
    perform_IOP_z_imm_pred(zx, esize, Iop_LSH_L, Zd, shift, Pg);
}
static inline void yasve_lslr_0x04178000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_LSH_L_REV, Zd, Zm, Pg);
}
static inline void yasve_lsr_0x04208400 (CTXARG, int Zd, int Zn, int Zm) {
    /* (wide elements, unpredicated) */
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    perform_IOP_z_wide2nd(zx, esize, true, Iop_LSH_R, Zd, Zn, Zm);
}
static inline void yasve_lsr_0x04209400 (CTXARG, int Zd, int Zn, s64 imm) {
    NOTTESTED();
    int esize = esize_for_shift(imm);
    u64 shift = (u64)(imm - esize);
    perform_IOP_z_imm(zx, esize, Iop_LSH_R, Zd, Zn, shift);
}
static inline void yasve_lsr_0x04118000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_LSH_R, Zd, Zm, Pg);
}
static inline void yasve_lsr_0x04198000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    /* (wide elements, predicated) */
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    perform_IOP_z_pred_wide2nd(zx, esize, true, Iop_LSH_R, Zd, Zm, Pg);
}
static inline void yasve_lsr_0x04018000 (CTXARG, int Zd, int Pg, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = esize_for_shift(imm);
    u64 shift = (u64)(imm - esize);
    perform_IOP_z_imm_pred(zx, esize, Iop_LSH_R, Zd, shift, Pg);
}
static inline void yasve_lsrr_0x04158000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_LSH_R_REV, Zd, Zm, Pg);
}
static inline void yasve_mad_0x0400c000 (CTXARG, int Zd, int Pg, int Zm, int Za) {
    /* (Zd := Za + Zd * Zm) */
    //NOTTESTED00();
    int esize = (8 << size);
    bool sub_op = false;
    perform_IFMA(zx, esize, Zd, Za, sub_op, Zd, Zm, Pg);
}
static inline void yasve_mla_0x04004000 (CTXARG, int Zd, int Pg, int Zn, int Zm) {
    /* (Zd := Zd + Zn * Zm) */
    NOTTESTED();
    int esize = (8 << size);
    bool sub_op = false;
    perform_IFMA(zx, esize, Zd, Zd, sub_op, Zn, Zm, Pg);
}
static inline void yasve_mls_0x04006000 (CTXARG, int Zd, int Pg, int Zn, int Zm) {
    /* (Zd := Zd - Zn * Zm) */
    NOTTESTED();
    int esize = (8 << size);
    bool sub_op = true;
    perform_IFMA(zx, esize, Zd, Zd, sub_op, Zn, Zm, Pg);
}
static inline void yasve_movprfx_0x0420bc00 (CTXARG, int Zd, int Zn) {
    /* (unpredicated) */
    //NOTTESTED00();
    CheckSVEEnabled();
    zreg result = zx->z[Zn];
    zx->z[Zd] = result;
}
static inline void yasve_movprfx_0x04102000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (predicated) */
    //NOTTESTED00();
    int esize = (8 << size);
    int M = opr_m16(opc);
    bool merging = (M == 1);
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg operand1 = zx->z[Zn];
    zreg dst = zx->z[Zd];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    u64 element = Elem_get(zx, &operand1, e, esize, DONTCARE);
	    Elem_set(zx, &result, e, esize, element);
	} else if (merging) {
	    u64 element = Elem_get(zx, &dst, e, esize, DONTCARE);
	    Elem_set(zx, &result, e, esize, element);
	} else {
	    Elem_set(zx, &result, e, esize, 0);
	}
    }
    zx->z[Zd] = result;
}
static inline void yasve_msb_0x0400e000 (CTXARG, int Zd, int Pg, int Zm, int Za) {
    /* (Zd := Za - Zd * Zm) */
    //NOTTESTED14();
    int esize = (8 << size);
    bool sub_op = true;
    perform_IFMA(zx, esize, Zd, Za, sub_op, Zd, Zm, Pg);
}
static inline void yasve_mul_0x2530c000 (CTXARG, int Zd, int _Zd, s64 imm) {
    //NOTTESTED00();
    int esize = (8 << size);
    perform_IOP_z_imm(zx, esize, Iop_MUL, Zd, Zd, (u64)imm);
}
static inline void yasve_mul_0x04100000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    //NOTTESTED00();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_MUL, Zd, Zm, Pg);
}
static inline void yasve_nand_0x25804210 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    bool setflags = false;
    perform_LOP_p(zx, esize, setflags, Iop_NAND, Pd, Pn, Pm, Pg);
}
static inline void yasve_nands_0x25c04210 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    bool setflags = true;
    perform_LOP_p(zx, esize, setflags, Iop_NAND, Pd, Pn, Pm, Pg);
}
static inline void yasve_neg_0x0417a000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_NEG, Zd, Zn, Pg);
}
static inline void yasve_nor_0x25804200 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    bool setflags = false;
    perform_LOP_p(zx, esize, setflags, Iop_NOR, Pd, Pn, Pm, Pg);
}
static inline void yasve_nors_0x25c04200 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    bool setflags = true;
    perform_LOP_p(zx, esize, setflags, Iop_NOR, Pd, Pn, Pm, Pg);
}
static inline void yasve_not_0x041ea000 (CTXARG, int Zd, int Pg, int Zn) {
    //NOTTESTED00();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_NOT, Zd, Zn, Pg);
}
static inline void yasve_orn_0x25804010 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    bool setflags = false;
    perform_LOP_p(zx, esize, setflags, Iop_IORN, Pd, Pn, Pm, Pg);
}
static inline void yasve_orns_0x25c04010 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    bool setflags = true;
    perform_LOP_p(zx, esize, setflags, Iop_IORN, Pd, Pn, Pm, Pg);
}
static inline void yasve_orr_0x04603000 (CTXARG, int Zd, int Zn, int Zm) {
    //NOTTESTED00();
    int esize = 64;
    perform_IOP_z(zx, esize, Iop_IOR, Zd, Zn, Zm);
}
static inline void yasve_orr_0x05000000 (CTXARG, int Zd, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = 64;
    u64 immv = DecodeBitMasks(esize, (u32)imm);
    perform_IOP_z_imm(zx, esize, Iop_IOR, Zd, Zd, immv);
}
static inline void yasve_orr_0x04180000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_IOR, Zd, Zm, Pg);
}
static inline void yasve_orr_0x25804000 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    //NOTTESTED01();
    int esize = 8;
    bool setflags = false;
    perform_LOP_p(zx, esize, setflags, Iop_IOR, Pd, Pn, Pm, Pg);
}
static inline void yasve_orrs_0x25c04000 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    bool setflags = true;
    perform_LOP_p(zx, esize, setflags, Iop_IOR, Pd, Pn, Pm, Pg);
}
static inline void yasve_orv_0x04182000 (CTXARG, int Vd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    u64 unitv = 0ULL;
    perform_IREDUCE_seq(zx, esize, Iop_IOR, U64EXT, unitv, Vd, Zn, Pg);
}
static inline void yasve_pfalse_0x2518e400 (CTXARG, int Pd) {
    NOTTESTED();
    CheckSVEEnabled();
    zx->p[Pd] = preg_zeros;
}
static inline void yasve_pfirst_0x2558c000 (CTXARG, int Pdn, int Pg, int Pd) {
    NOTTESTED();
    int esize = 8;
    CheckSVEEnabled();
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    preg result = zx->p[Pdn];
    int first = -1;
    for (int e = 0; e < elements; e++) {
	if (ElemP_get(zx, &mask, e, esize) == 1 && first == -1) {
	    first = e;
	}
    }
    if (first >= 0) {
	ElemP_set(zx, &result, first, esize, 1);
    }
    NZCV_set(zx, PredTest(zx, &mask, &result, esize));
    zx->p[Pdn] = result;
}
static inline void yasve_pnext_0x2519c400 (CTXARG, int Pdn, int Pg, int Pd) {
    NOTTESTED();
    int esize = (8 << size);
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    preg operand = zx->p[Pdn];
    preg result = preg_zeros;
    int next = LastActiveElement(zx, &operand, esize) + 1;
    while (next < elements && (ElemP_get(zx, &mask, next, esize) == 0)) {
	next = next + 1;
    }
    result = preg_zeros;
    if (next < elements) {
	ElemP_set(zx, &result, next, esize, 1);
    }
    NZCV_set(zx, PredTest(zx, &mask, &result, esize));
    zx->p[Pdn] = result;
}
static inline void yasve_prfb_0x8400c000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfb_0x84200000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfb_0xc4200000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfb_0xc4608000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfb_0x8400e000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfb_0x85c00000 (CTXARG, int PRFOP, int Pg, ...) {
    /*(scalar plus immediate)*/
    perform_NOP();
}
static inline void yasve_prfb_0xc400e000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfd_0x84206000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfd_0x8580c000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfd_0xc4206000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfd_0xc460e000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfd_0x8580e000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfd_0x85c06000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfd_0xc580e000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfh_0x84202000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfh_0x8480c000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfh_0xc4202000 (CTXARG, int PRFOP, int Pg, ...) {
	perform_NOP();
}
static inline void yasve_prfh_0xc460a000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfh_0x8480e000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfh_0x85c02000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfh_0xc480e000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfw_0x84204000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfw_0x8500c000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfw_0xc4204000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfw_0xc460c000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfw_0x8500e000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfw_0x85c04000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_prfw_0xc500e000 (CTXARG, int PRFOP, int Pg, ...) {
    perform_NOP();
}
static inline void yasve_ptest_0x2550c000 (CTXARG, int Pg, int Pn) {
    //NOTTESTED00();
    int esize = 8;
    CheckSVEEnabled();
    preg mask = zx->p[Pg];
    preg result = zx->p[Pn];
    NZCV_set(zx, PredTest(zx, &mask, &result, esize));
}
static inline void yasve_ptrue_0x2518e000 (CTXARG, int Pd, int pattern) {
    //NOTTESTED00();
    int esize = (8 << size);
    bool setflags = false;
    perform_PTRUE_p(zx, esize, Pd, pattern, setflags);
}
static inline void yasve_ptrues_0x2519e000 (CTXARG, int Pd, int pattern) {
    NOTTESTED();
    int esize = (8 << size);
    bool setflags = true;
    perform_PTRUE_p(zx, esize, Pd, pattern, setflags);
}
static inline void yasve_punpkhi_0x05314000 (CTXARG, int Pd, int Pn) {
    //NOTTESTED00();
    int esize = 16;
    bool hi = true;
    perform_UNPACK_p(zx, esize, hi, Pn, Pd);
}
static inline void yasve_punpklo_0x05304000 (CTXARG, int Pd, int Pn) {
    //NOTTESTED00();
    int esize = 16;
    bool hi = true;
    perform_UNPACK_p(zx, esize, hi, Pn, Pd);
}
static inline void yasve_rbit_0x05278000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_BITREVERSE, Zd, Zn, Pg);
}
static inline void yasve_rdffr_0x2519f000 (CTXARG, int Pd) {
    NOTTESTED();
    CheckSVEEnabled();
    zx->p[Pd] = zx->ffr;
}
static inline void yasve_rdffr_0x2518f000 (CTXARG, int Pd, int Pg) {
    NOTTESTED();
    bool setflags = false;
    CheckSVEEnabled();
    preg mask = zx->p[Pg];
    preg ffr = zx->ffr;
    preg result = preg_and(zx, &ffr, &mask, 8);
    if (setflags) {
	NZCV_set(zx, PredTest(zx, &mask, &result, 8));
    }
    zx->p[Pd] = result;
}
static inline void yasve_rdffrs_0x2558f000 (CTXARG, int Pd, int Pg) {
    NOTTESTED();
    bool setflags = true;
    CheckSVEEnabled();
    preg mask = zx->p[Pg];
    preg ffr = zx->ffr;
    preg result = preg_and(zx, &ffr, &mask, 8);
    if (setflags) {
	NZCV_set(zx, PredTest(zx, &mask, &result, 8));
    }
    zx->p[Pd] = result;
}
static inline void yasve_rdvl_0x04bf5000 (CTXARG, int Rd, s64 imm) {
    NOTTESTED();
    CheckSVEEnabled();
    s64 len = imm * (zx->VL / 8);
    Xreg_set(zx, Rd ,(u64)len);
}
static inline void yasve_rev_0x05344000 (CTXARG, int Pd, int Pn) {
    NOTTESTED();
    int esize = (8 << size);
    perform_VECTOR_REVERSE_p(zx, esize, Pd, Pn);
}
static inline void yasve_rev_0x05383800 (CTXARG, int Zd, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    perform_VECTOR_REVERSE_z(zx, esize, Zd, Zn);
}
static inline void yasve_revb_0x05248000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int swsize = 8;
    if (size == 0) {UnallocatedEncoding();}
    int esize = (8 << size);
    perform_IREVERSE(zx, esize, swsize, Zd, Zn, Pg);
}
static inline void yasve_revh_0x05a58000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int swsize = 16;
    int esize = (32 << sz);
    perform_IREVERSE(zx, esize, swsize, Zd, Zn, Pg);
}
static inline void yasve_revw_0x05e68000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int swsize = 32;
    int esize = 64;
    perform_IREVERSE(zx, esize, swsize, Zd, Zn, Pg);
}
static inline void yasve_sabd_0x040c0000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_SDIFF, Zd, Zm, Pg);
}
static inline void yasve_saddv_0x04002000 (CTXARG, int Vd, int Pg, int Zn) {
    NOTTESTED();
    if (size == 3) {UnallocatedEncoding();}
    int esize = (8 << size);
    u64 unitv = 0ULL;
    perform_IREDUCE_seq(zx, esize, Iop_ADD, S64EXT, unitv, Vd, Zn, Pg);
}
static inline void yasve_scvtf_0x6552a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (16-bit to half-precision) */
    TBD("scvtf");
}
static inline void yasve_scvtf_0x6554a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (32-bit to half-precision) */
    TBD("scvtf");
}
static inline void yasve_scvtf_0x6594a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (32-bit to single-precision) */
    //NOTTESTED00();
    int esize = 32;
    /*int s_esize = 32;*/
    /*int d_esize = 32;*/
    /*bool unsignedp = false;*/
    perform_FOP_z(zx, esize, Fop_CVTF_S32_32, true, Zd, 99, Zn, Pg);
}
static inline void yasve_scvtf_0x65d0a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (32-bit to double-precision) */
    //NOTTESTED00();
    int esize = 64;
    /*int s_esize = 32;*/
    /*int d_esize = 64;*/
    /*bool unsignedp = false;*/
    perform_FOP_z(zx, esize, Fop_CVTF_S32_64, true, Zd, 99, Zn, Pg);
}
static inline void yasve_scvtf_0x6556a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (64-bit to half-precision) */
    TBD("scvtf");
}
static inline void yasve_scvtf_0x65d4a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (64-bit to single-precision) */
    NOTTESTED();
    int esize = 64;
    /*int s_esize = 64;*/
    /*int d_esize = 32;*/
    /*bool unsignedp = false;*/
    perform_FOP_z(zx, esize, Fop_CVTF_S64_32, true, Zd, 99, Zn, Pg);
}
static inline void yasve_scvtf_0x65d6a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (64-bit to double-precision) */
    //NOTTESTED14();
    int esize = 64;
    /*int s_esize = 64;*/
    /*int d_esize = 64;*/
    /*bool unsignedp = false;*/
    perform_FOP_z(zx, esize, Fop_CVTF_S64_64, true, Zd, 99, Zn, Pg);
}
static inline void yasve_sdiv_0x04140000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    /*bool unsignedp = false;*/
    perform_IOP_z_pred(zx, esize, Iop_SDIV, Zd, Zm, Pg);
}
static inline void yasve_sdivr_0x04160000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    /*bool unsignedp = false;*/
    perform_IOP_z_pred(zx, esize, Iop_SDIV_REV, Zd, Zm, Pg);
}

static inline void yasve_sdot_0x44800000 (CTXARG, int Zda, int Zn, int Zm) {
    /* (vectors) */
    /* (SVE_Zd, SVE_Zn, SVE_Zm_16) */
    NOTTESTED();
    if (size == 0) {UnallocatedEncoding();}
    int esize = (8 << size);
    perform_DOTP(zx, esize, false, S64EXT, Zda, Zn, Zm, 0);
}
static inline void yasve_sdot_0x44a00000 (CTXARG, int Zda, int Zn, int Zm) {
    /* (indexed/32-bit) */
    /* (SVE_Zd, SVE_Zn, SVE_Zm3_INDEX) */
    NOTTESTED();
    int esize = 32;
    int index = opr_i2(opc);
    perform_DOTP(zx, esize, true, S64EXT, Zda, Zn, Zm, index);
}
static inline void yasve_sdot_0x44e00000 (CTXARG, int Zda, int Zn, int Zm) {
    /* (indexed/64-bit) */
    /* (SVE_Zd, SVE_Zn, SVE_Zm4_INDEX) */
    NOTTESTED();
    int esize = 64;
    int index = opr_i1(opc);
    perform_DOTP(zx, esize, true, S64EXT, Zda, Zn, Zm, index);
}

static inline void yasve_sel_0x0520c000 (CTXARG, int Zd, int Pg, int Zn, int Zm) {
    //NOTTESTED00();
    int esize = (8 << size);
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg operand1 = zx->z[Zn];
    zreg operand2 = zx->z[Zm];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	u64 element1 = Elem_get(zx, &operand1, e, esize, DONTCARE);
	u64 element2 = Elem_get(zx, &operand2, e, esize, DONTCARE);
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    Elem_set(zx, &result, e, esize, element1);
	} else {
	    Elem_set(zx, &result, e, esize, element2);
	}
    }
    zx->z[Zd] = result;
}
static inline void yasve_sel_0x25004210 (CTXARG, int Pd, int Pg, int Pn, int Pm) {
    NOTTESTED();
    int esize = 8;
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    preg operand1 = zx->p[Pn];
    preg operand2 = zx->p[Pm];
    preg result = preg_zeros;
    for (int e = 0; e < elements; e++) {
	bool element1 = ElemP_get(zx, &operand1, e, esize);
	bool element2 = ElemP_get(zx, &operand2, e, esize);
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    ElemP_set(zx, &result, e, esize, element1);
	} else {
	    ElemP_set(zx, &result, e, esize, element2);
	}
    }
    zx->p[Pd] = result;
}
static inline void yasve_setffr_0x252c9000 (CTXARG, int _0) {
    NOTTESTED();
    CheckSVEEnabled();
    zx->ffr = preg_all_ones;
}
static inline void yasve_smax_0x2528c000 (CTXARG, int Zd, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    /*bool unsignedp = false;*/
    perform_IOP_z_imm(zx, esize, Iop_SMAX, Zd, Zd, (u64)imm);
}
static inline void yasve_smax_0x04080000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    /*bool unsignedp = false;*/
    perform_IOP_z_pred(zx, esize, Iop_SMAX, Zd, Zm, Pg);
}
static inline void yasve_smaxv_0x04082000 (CTXARG, int Vd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    u64 unitv = (0ULL - (1ULL << (esize - 1)));
    perform_IREDUCE_seq(zx, esize, Iop_SMAX, S64EXT, unitv, Vd, Zn, Pg);
}
static inline void yasve_smin_0x252ac000 (CTXARG, int Zd, int _Zd, s64 imm ) {
    NOTTESTED();
    int esize = (8 << size);
    /*bool unsignedp = false;*/
    perform_IOP_z_imm(zx, esize, Iop_SMIN, Zd, Zd, (u64)imm);
}
static inline void yasve_smin_0x040a0000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    /*bool unsignedp = false;*/
    perform_IOP_z_pred(zx, esize, Iop_SMIN, Zd, Zm, Pg);
}
static inline void yasve_sminv_0x040a2000 (CTXARG, int Vd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    u64 unitv = ((1ULL << (esize - 1)) - 1);
    perform_IREDUCE_seq(zx, esize, Iop_SMIN, S64EXT, unitv, Vd, Zn, Pg);
}
static inline void yasve_smulh_0x04120000 (CTXARG, int Zdn, int Pg, int _Zd, int Zm) {
    /* (predicated) */
    //NOTTESTED00();
    int esize = (8 << size);
    bool unsignedp = S64EXT;
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg operand1 = zx->z[Zdn];
    zreg operand2 = zx->z[Zm];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	u64 element1 = Elem_get(zx, &operand1, e, esize, unsignedp);
	u64 element2 = Elem_get(zx, &operand2, e, esize, unsignedp);
	if (ElemP_get(zx, &mask, e, esize) == 1) {
	    u64 product = (element1 * element2) >> esize;
	    Elem_set(zx, &result, e, esize, product);
	} else {
	    u64 v = Elem_get(zx, &operand1, e, esize, DONTCARE);
	    Elem_set(zx, &result, e, esize, v);
	}
    }
    zx->z[Zdn] = result;
}
static inline void yasve_splice_0x052c8000 (CTXARG, int Zdn, int Pg, int Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    preg mask = zx->p[Pg];
    zreg operand1 = zx->z[Zdn];
    zreg operand2 = zx->z[Zm];
    zreg result = zreg_zeros;
    int x = 0;
    bool active = false;
    int lastnum = LastActiveElement(zx, &mask, esize);
    if (lastnum >= 0) {
	for (int e = 0; e < lastnum; e++) {
	    active = (active || ElemP_get(zx, &mask, e, esize) == 1);
	    if (active) {
		u64 v = Elem_get(zx, &operand1, e, esize, DONTCARE);
		Elem_set(zx, &result, x, esize, v);
		x = x + 1;
	    }
	}
    }
    elements = (elements - x - 1);
    for (int e = 0; e < elements; e++) {
	u64 v = Elem_get(zx, &operand2, e, esize, DONTCARE);
	Elem_set(zx, &result, x, esize, v);
	x = x + 1;
    }
    zx->z[Zdn] = result;
}
static inline void yasve_sqadd_0x04201000 (TBDARG) {
    TBD("sqadd");
}
static inline void yasve_sqadd_0x2524c000 (TBDARG) {
    TBD("sqadd");
}
static inline void yasve_sqdecb_0x0430f800 (TBDARG) {
    TBD("sqdecb");
}
static inline void yasve_sqdecb_0x0420f800 (TBDARG) {
    TBD("sqdecb");
}
static inline void yasve_sqdecd_0x04e0c800 (TBDARG) {
    TBD("sqdecd");
}
static inline void yasve_sqdecd_0x04f0f800 (TBDARG) {
    TBD("sqdecd");
}
static inline void yasve_sqdecd_0x04e0f800 (TBDARG) {
    TBD("sqdecd");
}
static inline void yasve_sqdech_0x0460c800 (TBDARG) {
    TBD("sqdech");
}
static inline void yasve_sqdech_0x0470f800 (TBDARG) {
    TBD("sqdech");
}
static inline void yasve_sqdech_0x0460f800 (TBDARG) {
    TBD("sqdech");
}
static inline void yasve_sqdecp_0x252a8000 (TBDARG) {
    TBD("sqdecp");
}
static inline void yasve_sqdecp_0x252a8c00 (TBDARG) {
    TBD("sqdecp");
}
static inline void yasve_sqdecp_0x252a8800 (TBDARG) {
    TBD("sqdecp");
}
static inline void yasve_sqdecw_0x04a0c800 (TBDARG) {
    TBD("sqdecw");
}
static inline void yasve_sqdecw_0x04b0f800 (TBDARG) {
    TBD("sqdecw");
}
static inline void yasve_sqdecw_0x04a0f800 (TBDARG) {
    TBD("sqdecw");
}
static inline void yasve_sqincb_0x0430f000 (TBDARG) {
    TBD("sqincb");
}
static inline void yasve_sqincb_0x0420f000 (TBDARG) {
    TBD("sqincb");
}
static inline void yasve_sqincd_0x04e0c000 (TBDARG) {
    TBD("sqincd");
}
static inline void yasve_sqincd_0x04f0f000 (TBDARG) {
    TBD("sqincd");
}
static inline void yasve_sqincd_0x04e0f000 (TBDARG) {
    TBD("sqincd");
}
static inline void yasve_sqinch_0x0460c000 (TBDARG) {
    TBD("sqinch");
}
static inline void yasve_sqinch_0x0470f000 (TBDARG) {
    TBD("sqinch");
}
static inline void yasve_sqinch_0x0460f000 (TBDARG) {
    TBD("sqinch");
}
static inline void yasve_sqincp_0x25288000 (TBDARG) {
    TBD("sqincp");
}
static inline void yasve_sqincp_0x25288c00 (TBDARG) {
    TBD("sqincp");
}
static inline void yasve_sqincp_0x25288800 (TBDARG) {
    TBD("sqincp");
}
static inline void yasve_sqincw_0x04a0c000 (TBDARG) {
    TBD("sqincw");
}
static inline void yasve_sqincw_0x04b0f000 (TBDARG) {
    TBD("sqincw");
}
static inline void yasve_sqincw_0x04a0f000 (TBDARG) {
    TBD("sqincw");
}
static inline void yasve_sqsub_0x04201800 (TBDARG) {
    TBD("sqsub");
}
static inline void yasve_sqsub_0x2526c000 (TBDARG) {
    TBD("sqsub");
}

static inline void yasve_st1b_0xe4004000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/8-bit element) */
    /* operand: SVE_ADDR_RX=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 8;
    int msize = 8;
    perform_ST1_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_st1b_0xe400e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/8-bit element) */
    /* operand: SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 8;
    int msize = 8;
    perform_ST1_x_imm(zx, esize, msize, Zt, Rn, imm, Pg);
}
static inline void yasve_st1b_0xe4008000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* operand: SVE_ADDR_RZ_XTW_14=(Rn, Zm/32, opr_xs14, LSL=0) */
    assert(mo == OPR_Rn_Zm32_XS14_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1b_0xe400a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* operand: SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1b_0xe440a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int xs, int _1) {
    /* (vector plus immediate/64-bit element) */
    /* operand: SVE_ADDR_ZI_U5=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    s64 offset = imm;
    perform_ST1_z_imm(zx, esize, msize, Zt, Zn, offset, Pg);
}
static inline void yasve_st1b_0xe4604000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/64-bit element) */
    /* operand: SVE_ADDR_RX=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    perform_ST1_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_st1b_0xe460e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/64-bit element) */
    /* operand: SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 8;
    perform_ST1_x_imm(zx, esize, msize, Zt, Rn, imm, Pg);
}
static inline void yasve_st1b_0xe4204000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/16-bit element) */
    /* operand: SVE_ADDR_RX=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 16;
    int msize = 8;
    perform_ST1_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_st1b_0xe420e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/16-bit element) */
    /* operand: SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    int msize = 8;
    perform_ST1_x_imm(zx, esize, msize, Zt, Rn, imm, Pg);
}
static inline void yasve_st1b_0xe4408000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unscaled offset) */
    /* operand: SVE_ADDR_RZ_XTW_14=(Rn, Zm/32, opr_xs14, LSL=0) */
    assert(mo == OPR_Rn_Zm32_XS14_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1b_0xe460a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int _0, int _1) {
    /* (vector plus immediate/32-bit element) */
    /* operand: SVE_ADDR_ZI_U5=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    s64 offset = imm;
    perform_ST1_z_imm(zx, esize, msize, Zt, Zn, offset, Pg);
}
static inline void yasve_st1b_0xe4404000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/32-bit element) */
    /* operand: SVE_ADDR_RX=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    perform_ST1_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_st1b_0xe440e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/32-bit element) */
    /* operand: SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 8;
    perform_ST1_x_imm(zx, esize, msize, Zt, Rn, imm, Pg);
}

static inline void yasve_st1d_0xe5808000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* SVE_ADDR_RZ_XTW_14=(Rn, Zm/32, opr_xs14, LSL=0) */
    assert(mo == OPR_Rn_Zm32_XS14_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1d_0xe580a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int _0, int _1) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    //NOTTESTED21();
    int esize = 64;
    int msize = 64;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1d_0xe5a08000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked scaled offset) */
    /* SVE_ADDR_RZ_XTW3_14=(Rn, Zm/32, opr_xs14, LSL=3) */
    assert(mo == OPR_Rn_Zm32_XS14_LSL3);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 3;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1d_0xe5a0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int _0, int _1) {
    /* (scalar plus vector/64-bit scaled offset) */
    /* SVE_ADDR_RZ_LSL3=(Rn, Zm/64, opr_xs22, LSL=3) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL3);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 3;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1d_0xe5e04000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL3=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL3);
    //NOTTESTED00();
    int esize = 64;
    int msize = 64;
    perform_ST1_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_st1d_0xe5c0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int _0, int _1) {
    /* (vector plus immediate) */
    /* SVE_ADDR_ZI_U5x8=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    s64 offset = imm;
    perform_ST1_z_imm(zx, esize, msize, Zt, Zn, offset, Pg);
}
static inline void yasve_st1d_0xe5e0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    //NOTTESTED18();
    int esize = 64;
    int msize = 64;
    s64 offset = imm;
    perform_ST1_x_imm(zx, esize, msize, Zt, Rn, offset, Pg);
}

static inline void yasve_st1h_0xe4808000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* SVE_ADDR_RZ_XTW_14=(Rn, Zm/32, opr_xs14, LSL=0) */
    assert(mo == OPR_Rn_Zm32_XS14_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1h_0xe480a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int _0, int _1) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1h_0xe4a04000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/16-bit element) */
    /* SVE_ADDR_RX_LSL1=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL1);
    //NOTTESTED00();
    int esize = 16;
    int msize = 16;
    perform_ST1_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_st1h_0xe4a08000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked scaled offset) */
    /* SVE_ADDR_RZ_XTW1_14=(Rn, Zm/32, opr_xs14, LSL=1) */
    assert(mo == OPR_Rn_Zm32_XS14_LSL1);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 1;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1h_0xe4a0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int _0, int _1) {
    /* (scalar plus vector/64-bit scaled offset) */
    /* SVE_ADDR_RZ_LSL1=(Rn, Zm/64, opr_xs22, LSL=1) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL1);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 1;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1h_0xe4c04000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/32-bit element) */
    /* SVE_ADDR_RX_LSL1=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    perform_ST1_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_st1h_0xe4c08000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unscaled offset) */
    /* SVE_ADDR_RZ_XTW_14=(Rn, Zm/32, opr_xs14, LSL=0) */
    assert(mo == OPR_Rn_Zm32_XS14_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1h_0xe4e04000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/64-bit element) */
    /* SVE_ADDR_RX_LSL1=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    perform_ST1_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_st1h_0xe4e08000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit scaled offset) */
    /* SVE_ADDR_RZ_XTW1_14=(Rn, Zm/32, opr_xs14, LSL=1) */
    assert(mo == OPR_Rn_Zm32_XS14_LSL1);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 1;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1h_0xe4a0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/16-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    int msize = 16;
    s64 offset = imm;
    perform_ST1_x_imm(zx, esize, msize, Zt, Rn, offset, Pg);
}
static inline void yasve_st1h_0xe4c0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int _0, int _1) {
    /* (vector plus immediate/64-bit element) */
    /* SVE_ADDR_ZI_U5x2=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    s64 offset = imm;
    perform_ST1_z_imm(zx, esize, msize, Zt, Zn, offset, Pg);
}
static inline void yasve_st1h_0xe4c0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/32-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    s64 offset = imm;
    perform_ST1_x_imm(zx, esize, msize, Zt, Rn, offset, Pg);
}
static inline void yasve_st1h_0xe4e0a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int _0, int _1) {
    /* (vector plus immediate/32-bit element) */
    /* SVE_ADDR_ZI_U5x2=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    s64 offset = imm;
    perform_ST1_z_imm(zx, esize, msize, Zt, Zn, offset, Pg);
}
static inline void yasve_st1h_0xe4e0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/64-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 16;
    perform_ST1_x_imm(zx, esize, msize, Zt, Rn, imm, Pg);
}

static inline void yasve_st1w_0xe5008000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked unscaled offset) */
    /* SVE_ADDR_RZ_XTW_14=(Rn, Zm/32, opr_xs14, LSL=0) */
    assert(mo == OPR_Rn_Zm32_XS14_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1w_0xe500a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int _0, int _1) {
    /* (scalar plus vector/64-bit unscaled offset) */
    /* SVE_ADDR_RZ=(Rn, Zm/64, opr_xs22, LSL=0) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL0);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 0;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1w_0xe5208000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unpacked scaled offset) */
    /* SVE_ADDR_RZ_XTW2_14=(Rn, Zm/32, opr_xs14, LSL=2) */
    assert(mo == OPR_Rn_Zm32_XS14_LSL2);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 2;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1w_0xe520a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int _0, int _1) {
    /* (scalar plus vector/64-bit scaled offset) */
    /* SVE_ADDR_RZ_LSL2=(Rn, Zm/64, opr_xs22, LSL=2) */
    assert(mo == OPR_Rn_Zm64_XS22_LSL2);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    int offs_size = 64;
    bool offs_unsignedp = true;
    int scale = 2;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1w_0xe5404000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/32-bit element) */
    /* SVE_ADDR_RX_LSL2=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL2);
    //NOTTESTED00();
    int esize = 32;
    int msize = 32;
    perform_ST1_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_st1w_0xe5408000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit unscaled offset) */
    /* SVE_ADDR_RZ_XTW_14=(Rn, Zm/32, opr_xs14, LSL=0) */
    assert(mo == OPR_Rn_Zm32_XS14_LSL0);
    NOTTESTED();
    int esize = 32;
    int msize = 32;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 0;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1w_0xe5604000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar/64-bit element) */
    /* SVE_ADDR_RX_LSL2=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL2);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    perform_ST1_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_st1w_0xe5608000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Zm, int xs, int _1) {
    /* (scalar plus vector/32-bit scaled offset) */
    /* SVE_ADDR_RZ_XTW2_14=(Rn, Zm/32, opr_xs14, LSL=2) */
    assert(mo == OPR_Rn_Zm32_XS14_LSL2);
    NOTTESTED();
    int esize = 32;
    int msize = 16;
    int offs_size = 32;
    bool offs_unsignedp = (xs == 0);
    int scale = 2;
    perform_ST1_x_z(zx, esize, msize,
		    offs_size, offs_unsignedp, scale,
		    Zt, Rn, Zm, Pg);
}
static inline void yasve_st1w_0xe540a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int _0, int _1) {
    /* (vector plus immediate/64-bit element) */
    /* SVE_ADDR_ZI_U5x4=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    s64 offset = imm;
    perform_ST1_z_imm(zx, esize, msize, Zt, Zn, offset, Pg);
}
static inline void yasve_st1w_0xe540e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/32-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 32;
    s64 offset = imm;
    perform_ST1_x_imm(zx, esize, msize, Zt, Rn, offset, Pg);
}
static inline void yasve_st1w_0xe560a000 (CTXARG, int Zt, int Pg, svemo_t mo, int Zn, s64 imm, int _0, int _1) {
    /* (vector plus immediate/32-bit element) */
    /* SVE_ADDR_ZI_U5x4=(Zn, opr_uimm5, 0, 0) */
    assert(mo == OPR_ZnSS_IMM);
    NOTTESTED();
    int esize = 32;
    int msize = 32;
    s64 offset = imm;
    perform_ST1_z_imm(zx, esize, msize, Zt, Zn, offset, Pg);
}
static inline void yasve_st1w_0xe560e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate/64-bit element) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    int msize = 32;
    s64 offset = imm;
    perform_ST1_x_imm(zx, esize, msize, Zt, Rn, offset, Pg);
}

static inline void yasve_st2b_0xe4206000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 8;
    int nreg = 2;
    perform_ST234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_st2b_0xe430e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x2xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 8;
    s64 offset = imm;
    int nreg = 2;
    perform_ST234_x_imm(zx, esize, nreg, Zt, Rn, offset, Pg);
}

static inline void yasve_st2d_0xe5a06000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL3=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL3);
    NOTTESTED();
    int esize = 64;
    int nreg = 2;
    perform_ST234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_st2d_0xe5b0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x2xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    s64 offset = imm;
    int nreg = 2;
    perform_ST234_x_imm(zx, esize, nreg, Zt, Rn, offset, Pg);
}

static inline void yasve_st2h_0xe4a06000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL1=(Rn, Rm, 0, lsl) */
    NOTTESTED();
    assert(mo == OPR_Rn_Rm_LSL1);
    int esize = 16;
    int nreg = 2;
    perform_ST234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_st2h_0xe4b0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x2xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    s64 offset = imm;
    int nreg = 2;
    perform_ST234_x_imm(zx, esize, nreg, Zt, Rn, offset, Pg);
}

static inline void yasve_st2w_0xe5206000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL2=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL2);
    NOTTESTED();
    int esize = 32;
    int nreg = 2;
    perform_ST234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_st2w_0xe530e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x2xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    s64 offset = imm;
    int nreg = 2;
    perform_ST234_x_imm(zx, esize, nreg, Zt, Rn, offset, Pg);
}

static inline void yasve_st3b_0xe4406000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 8;
    int nreg = 3;
    perform_ST234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_st3b_0xe450e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x3xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 8;
    s64 offset = imm;
    int nreg = 3;
    perform_ST234_x_imm(zx, esize, nreg, Zt, Rn, offset, Pg);
}

static inline void yasve_st3d_0xe5c06000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL3=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL3);
    NOTTESTED();
    int esize = 64;
    int nreg = 3;
    perform_ST234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_st3d_0xe5d0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x3xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    s64 offset = imm;
    int nreg = 3;
    perform_ST234_x_imm(zx, esize, nreg, Zt, Rn, offset, Pg);
}

static inline void yasve_st3h_0xe4c06000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL1=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 16;
    int nreg = 3;
    perform_ST234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_st3h_0xe4d0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x3xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    s64 offset = imm;
    int nreg = 3;
    perform_ST234_x_imm(zx, esize, nreg, Zt, Rn, offset, Pg);
}

static inline void yasve_st3w_0xe5406000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL2=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL2);
    NOTTESTED();
    int esize = 32;
    int nreg = 3;
    perform_ST234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_st3w_0xe550e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x3xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    s64 offset = imm;
    int nreg = 3;
    perform_ST234_x_imm(zx, esize, nreg, Zt, Rn, offset, Pg);
}

static inline void yasve_st4b_0xe4606000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 8;
    int nreg = 4;
    perform_ST234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_st4b_0xe470e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 8;
    s64 offset = imm;
    int nreg = 4;
    perform_ST234_x_imm(zx, esize, nreg, Zt, Rn, offset, Pg);
}

static inline void yasve_st4d_0xe5e06000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL3=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL3);
    NOTTESTED();
    int esize = 64;
    int nreg = 4;
    perform_ST234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_st4d_0xe5f0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    s64 offset = imm;
    int nreg = 4;
    perform_ST234_x_imm(zx, esize, nreg, Zt, Rn, offset, Pg);
}

static inline void yasve_st4h_0xe4e06000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL1=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 16;
    int nreg = 4;
    perform_ST234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_st4h_0xe4f0e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    s64 offset = imm;
    int nreg = 4;
    perform_ST234_x_imm(zx, esize, nreg, Zt, Rn, offset, Pg);
}

static inline void yasve_st4w_0xe5606000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL2=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL2);
    NOTTESTED();
    int esize = 32;
    int nreg = 4;
    perform_ST234_x_x(zx, esize, nreg, Zt, Rn, Rm, Pg);
}
static inline void yasve_st4w_0xe570e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4x4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    s64 offset = imm;
    int nreg = 4;
    perform_ST234_x_imm(zx, esize, nreg, Zt, Rn, offset, Pg);
}

static inline void yasve_stnt1b_0xe4006000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL0);
    NOTTESTED();
    int esize = 8;
    int msize = 8;
    perform_STNT_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_stnt1b_0xe410e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 8;
    s64 offset = imm;
    perform_STNT_x_imm(zx, esize, Zt, Rn, offset, Pg);
}

static inline void yasve_stnt1d_0xe5806000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL3=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL3);
    NOTTESTED();
    int esize = 64;
    int msize = 64;
    perform_STNT_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_stnt1d_0xe590e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 64;
    s64 offset = imm;
    perform_STNT_x_imm(zx, esize, Zt, Rn, offset, Pg);
}

static inline void yasve_stnt1h_0xe4806000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL1=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL1);
    NOTTESTED();
    int esize = 16;
    int msize = 16;
    perform_STNT_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_stnt1h_0xe490e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 16;
    s64 offset = imm;
    perform_STNT_x_imm(zx, esize, Zt, Rn, offset, Pg);
}

static inline void yasve_stnt1w_0xe5006000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, int Rm, int _0, int _1) {
    /* (scalar plus scalar) */
    /* SVE_ADDR_RX_LSL2=(Rn, Rm, 0, lsl) */
    assert(mo == OPR_Rn_Rm_LSL2);
    NOTTESTED();
    int esize = 32;
    int msize = 32;
    perform_STNT_x_x(zx, esize, msize, Zt, Rn, Rm, Pg);
}
static inline void yasve_stnt1w_0xe510e000 (CTXARG, int Zt, int Pg, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* (scalar plus immediate) */
    /* SVE_ADDR_RI_S4xVL=(Rn, opr_simm4, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    int esize = 32;
    s64 offset = imm;
    perform_STNT_x_imm(zx, esize, Zt, Rn, offset, Pg);
}

static inline void yasve_str_0xe5800000 (CTXARG, int Pt, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* operand: SVE_ADDR_RI_S9xVL=(Rn, opr_simm9, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    perform_STR_p(zx, Pt, Rn, imm);
}
static inline void yasve_str_0xe5804000 (CTXARG, int Zt, svemo_t mo, int Rn, s64 imm, int _0, int _1) {
    /* operand: SVE_ADDR_RI_S9xVL=(Rn, opr_simm9, 0, 0) */
    assert(mo == OPR_Rn_IMM);
    NOTTESTED();
    perform_STR_z(zx, Zt, Rn, imm);
}

static inline void yasve_sub_0x04200400 (CTXARG, int Zd, int Zn, int Zm) {
    //NOTTESTED00();
    int esize = (8 << size);
    perform_IOP_z(zx, esize, Iop_SUB, Zd, Zn, Zm);
}
static inline void yasve_sub_0x2521c000 (CTXARG, int Zd, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    int sh = opr_sh13(opc);
    if (((size << 1) | sh) == 1) {ReservedValue();}
    u64 imm1 = (u64)imm;
    if (sh == 1) {
	imm1 = imm1 << 8;
    }
    perform_IOP_z_imm(zx, esize, Iop_SUB, Zd, Zd, imm1);
}
static inline void yasve_sub_0x04010000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_SUB, Zd, Zm, Pg);
}
static inline void yasve_subr_0x2523c000 (CTXARG, int Zd, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    int sh = opr_sh13(opc);
    if (((size << 1) | sh) == 1) {ReservedValue();}
    u64 immv = (u64)imm;
    if (sh == 1) {
	immv = immv << 8;
    }
    perform_IOP_z_imm(zx, esize, Iop_SUB_REV, Zd, Zd, immv);
}
static inline void yasve_subr_0x04030000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_SUB_REV, Zd, Zm, Pg);
}
static inline void yasve_sunpkhi_0x05313800 (CTXARG, int Zd, int Zn) {
    //NOTTESTED00();
    if (size == 0) {UnallocatedEncoding();}
    int esize = (8 << size);
    bool unsignedp = false;
    bool hi = true;
    perform_UNPACK_z(zx, esize, hi, unsignedp, Zn, Zd);
}
static inline void yasve_sunpklo_0x05303800 (CTXARG, int Zd, int Zn) {
    //NOTTESTED00();
    if (size == 0) {UnallocatedEncoding();}
    int esize = (8 << size);
    bool unsignedp = false;
    bool hi = false;
    perform_UNPACK_z(zx, esize, hi, unsignedp, Zn, Zd);
}
static inline void yasve_sxtb_0x0410a000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    if (size == 0) {UnallocatedEncoding();}
    int esize = (8 << size);
    int s_esize = 8;
    bool unsignedp = false;
    perform_EXTEND(zx, esize, s_esize, unsignedp, Zn, Zd, Pg);
}
static inline void yasve_sxth_0x0412a000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    int s_esize = 16;
    bool unsignedp = false;
    perform_EXTEND(zx, esize, s_esize, unsignedp, Zn, Zd, Pg);
}
static inline void yasve_sxtw_0x04d4a000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int esize = 64;
    int s_esize = 32;
    bool unsignedp = false;
    perform_EXTEND(zx, esize, s_esize, unsignedp, Zn, Zd, Pg);
}
static inline void yasve_tbl_0x05203000 (CTXARG, int Zd, int Zn, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    zreg operand1 = zx->z[Zn];
    zreg operand2 = zx->z[Zm];
    zreg result = zreg_zeros;
    for (int e = 0; e < elements; e++) {
	u64 idx = (u64)Elem_get(zx, &operand2, e, esize, U64EXT);
	u64 v = ((idx < (u64)elements)
		 ? Elem_get(zx, &operand1, (int)idx, esize, DONTCARE)
		 : 0);
	Elem_set(zx, &result, e, esize, v);
    }
    zx->z[Zd] = result;
}
static inline void yasve_trn1_0x05205000 (CTXARG, int Pd, int Pn, int Pm) {
    NOTTESTED();
    int esize = (8 << size);
    int part = 0;
    CheckSVEEnabled();
    int pairs = zx->VL / (esize * 2);
    preg operand1 = zx->p[Pn];
    preg operand2 = zx->p[Pm];
    preg result = preg_zeros;
    for (int p = 0; p < pairs; p++) {
	bool v0 = ElemP_get(zx, &operand1, (2*p+part), esize);
	bool v1 = ElemP_get(zx, &operand2, (2*p+part), esize);
	ElemP_set(zx, &result, 2*p+0, esize, v0);
	ElemP_set(zx, &result, 2*p+1, esize, v1);
    }
    zx->p[Pd] = result;
}
static inline void yasve_trn1_0x05207000 (CTXARG, int Zd, int Zn, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    int part = 0;
    CheckSVEEnabled();
    int pairs = zx->VL / (esize * 2);
    zreg operand1 = zx->z[Zn];
    zreg operand2 = zx->z[Zm];
    zreg result = zreg_zeros;
    for (int p = 0; p < pairs; p++) {
	u64 v0 = Elem_get(zx, &operand1, (2*p+part), esize, DONTCARE);
	u64 v1 = Elem_get(zx, &operand2, (2*p+part), esize, DONTCARE);
	Elem_set(zx, &result, (2*p+0), esize, v0);
	Elem_set(zx, &result, (2*p+1), esize, v1);
    }
    zx->z[Zd] = result;
}
static inline void yasve_trn2_0x05205400 (CTXARG, int Pd, int Pn, int Pm) {
    NOTTESTED();
    int esize = (8 << size);
    int part = 1;
    CheckSVEEnabled();
    int pairs = zx->VL / (esize * 2);
    preg operand1 = zx->p[Pn];
    preg operand2 = zx->p[Pm];
    preg result = preg_zeros;
    for (int p = 0; p < pairs; p++) {
	bool v0 = ElemP_get(zx, &operand1, (2*p+part), esize);
	bool v1 = ElemP_get(zx, &operand2, (2*p+part), esize);
	ElemP_set(zx, &result, 2*p+0, esize, v0);
	ElemP_set(zx, &result, 2*p+1, esize, v1);
    }
    zx->p[Pd] = result;
}
static inline void yasve_trn2_0x05207400 (CTXARG, int Zd, int Zn, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    int part = 1;
    CheckSVEEnabled();
    int pairs = zx->VL / (esize * 2);
    zreg operand1 = zx->z[Zn];
    zreg operand2 = zx->z[Zm];
    zreg result = zreg_zeros;
    for (int p = 0; p < pairs; p++) {
	u64 v0 = Elem_get(zx, &operand1, (2*p+part), esize, DONTCARE);
	u64 v1 = Elem_get(zx, &operand2, (2*p+part), esize, DONTCARE);
	Elem_set(zx, &result, (2*p+0), esize, v0);
	Elem_set(zx, &result, (2*p+1), esize, v1);
    }
    zx->z[Zd] = result;
}
static inline void yasve_uabd_0x040d0000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    perform_IOP_z_pred(zx, esize, Iop_UDIFF, Zd, Zm, Pg);
}
static inline void yasve_uaddv_0x04012000 (CTXARG, int Vd, int Pg, int Zn) {
    //NOTTESTED00();
    int esize = (8 << size);
    u64 unitv = 0ULL;
    perform_IREDUCE_seq(zx, esize, Iop_ADD, U64EXT, unitv, Vd, Zn, Pg);
}
static inline void yasve_ucvtf_0x6553a000 (TBDARG) {
    /* (16-bit to half-precision) */
    /* (SVE_Zd, SVE_Pg3, SVE_Zn) */
    TBD("ucvtf");
}
static inline void yasve_ucvtf_0x6555a000 (TBDARG) {
    /* (32-bit to half-precision) */
    /* (SVE_Zd, SVE_Pg3, SVE_Zn) */
    TBD("ucvtf");
}
static inline void yasve_ucvtf_0x6595a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (32-bit to single-precision) */
    NOTTESTED();
    int esize = 32;
    /*int s_esize = 32;*/
    /*int d_esize = 32;*/
    /*bool unsignedp = true;*/
    perform_FOP_z(zx, esize, Fop_CVTF_U32_32, true, Zd, 99, Zn, Pg);
}
static inline void yasve_ucvtf_0x65d1a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (32-bit to double-precision) */
    NOTTESTED();
    int esize = 64;
    /*int s_esize = 32;*/
    /*int d_esize = 64;*/
    /*bool unsignedp = true;*/
    perform_FOP_z(zx, esize, Fop_CVTF_U32_64, true, Zd, 99, Zn, Pg);
}
static inline void yasve_ucvtf_0x6557a000 (TBDARG) {
    /* (64-bit to half-precision) */
    /* (SVE_Zd, SVE_Pg3, SVE_Zn) */
    TBD("ucvtf");
}
static inline void yasve_ucvtf_0x65d5a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (64-bit to single-precision) */
    NOTTESTED();
    int esize = 64;
    /*int s_esize = 64;*/
    /*int d_esize = 32;*/
    /*bool unsignedp = true;*/
    perform_FOP_z(zx, esize, Fop_CVTF_U64_32, true, Zd, 99, Zn, Pg);
}
static inline void yasve_ucvtf_0x65d7a000 (CTXARG, int Zd, int Pg, int Zn) {
    /* (64-bit to double-precision) */
    NOTTESTED();
    int esize = 64;
    /*int s_esize = 64;*/
    /*int d_esize = 64;*/
    /*bool unsignedp = true;*/
    perform_FOP_z(zx, esize, Fop_CVTF_U64_64, true, Zd, 99, Zn, Pg);
}
static inline void yasve_udiv_0x04950000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (32 << sz);
    /*bool unsignedp = true;*/
    perform_IOP_z_pred(zx, esize, Iop_UDIV, Zd, Zm, Pg);
}
static inline void yasve_udivr_0x04170000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    /*bool unsignedp = true;*/
    perform_IOP_z_pred(zx, esize, Iop_UDIV_REV, Zd, Zm, Pg);
}
static inline void yasve_udot_0x44800400 (CTXARG, int Zda, int Zn, int Zm) {
    /* (vectors) */
    /* (SVE_Zd, SVE_Zn, SVE_Zm_16) */
    NOTTESTED();
    if (size == 0) {UnallocatedEncoding();}
    int esize = (8 << size);
    perform_DOTP(zx, esize, false, U64EXT, Zda, Zn, Zm, 0);
}
static inline void yasve_udot_0x44a00400 (CTXARG, int Zda, int Zn, int Zm) {
    /* (indexed/32-bit) */
    /* (SVE_Zd, SVE_Zn, SVE_Zm3_INDEX) */
    NOTTESTED();
    int esize = 32;
    int index = opr_i2(opc);
    perform_DOTP(zx, esize, true, U64EXT, Zda, Zn, Zm, index);
}
static inline void yasve_udot_0x44e00400 (CTXARG, int Zda, int Zn, int Zm) {
    /* (indexed/64-bit) */
    /* (SVE_Zd, SVE_Zn, SVE_Zm4_INDEX) */
    NOTTESTED();
    int esize = 64;
    int index = opr_i1(opc);
    perform_DOTP(zx, esize, true, U64EXT, Zda, Zn, Zm, index);
}

static inline void yasve_umax_0x2529c000 (CTXARG, int Zd, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    /*bool unsignedp = true;*/
    perform_IOP_z_imm(zx, esize, Iop_UMAX, Zd, Zd, (u64)imm);
}
static inline void yasve_umax_0x04090000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    /*bool unsignedp = true;*/
    perform_IOP_z_pred(zx, esize, Iop_UMAX, Zd, Zm, Pg);
}
static inline void yasve_umaxv_0x04092000 (CTXARG, int Vd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    u64 unitv = 0ULL;
    perform_IREDUCE_seq(zx, esize, Iop_UMAX, U64EXT, unitv, Vd, Zn, Pg);
}
static inline void yasve_umin_0x252bc000 (CTXARG, int Zd, int _Zd, s64 imm) {
    NOTTESTED();
    int esize = (8 << size);
    /*bool unsignedp = true;*/
    perform_IOP_z_imm(zx, esize, Iop_UMIN, Zd, Zd, (u64)imm);
}
static inline void yasve_umin_0x040b0000 (CTXARG, int Zd, int Pg, int _Zd, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    /*bool unsignedp = true;*/
    perform_IOP_z_pred(zx, esize, Iop_UMIN, Zd, Zm, Pg);
}
static inline void yasve_uminv_0x040b2000 (CTXARG, int Vd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    u64 unitv = (~0ULL);
    perform_IREDUCE_seq(zx, esize, Iop_UMIN, U64EXT, unitv, Vd, Zn, Pg);
}
static inline void yasve_umulh_0x04130000 (TBDARG) {
    TBD("umulh");
}
static inline void yasve_uqadd_0x04201400 (TBDARG) {
    TBD("uqadd");
}
static inline void yasve_uqadd_0x2525c000 (TBDARG) {
    TBD("uqadd");
}
static inline void yasve_uqdecb_0x0420fc00 (TBDARG) {
    TBD("uqdecb");
}
static inline void yasve_uqdecb_0x0430fc00 (TBDARG) {
    TBD("uqdecb");
}
static inline void yasve_uqdecd_0x04e0cc00 (CTXARG, int Zdn, int pattern, s64 imm) {
    /* (vector) */
    /* (Zd, SVE_PATTERN_SCALED) */
    NOTTESTED();
    int esize = 64;
    int pat = pattern;
    bool unsignedp = U64EXT;
    CheckSVEEnabled();
    int elements = zx->VL / esize;
    int count = DecodePredCount(zx, pat, esize);
    zreg operand1 = zx->z[Zdn];
    zreg result;
    for (int e = 0; e < elements; e++) {
	s64 element1 = (s64)Elem_get(zx, &operand1, e, esize, unsignedp);
	saturated_t sv = SatQ2(element1, (s64)-(count * imm), esize, unsignedp);
	Elem_set(zx, &result, e, esize, sv.v);
    }
    zx->z[Zdn] = result;
}
static inline void yasve_uqdecd_0x04e0fc00 (CTXARG, int Rdn, int pattern, s64 imm0) {
    /* (scalar/32-bit) */
    /* (Rd, SVE_PATTERN_SCALED) */
    NOTTESTED();
    int esize = 64;
    int pat = pattern;
    s64 imm = imm0 + 1;
    bool unsignedp = true;
    int ssize = 32;
    CheckSVEEnabled();
    int count = DecodePredCount(zx, pat, esize);
    u64 operand1 = Xreg_get(zx, Rdn, X31ZERO);
    s64 element1 = (s64)operand1;
    saturated_t result = SatQ2(element1, (s64)-(count * imm), ssize, unsignedp);
    Xreg_set(zx, Rdn, (u64)result.v);
}
static inline void yasve_uqdecd_0x04f0fc00 (CTXARG, int Rdn, int pattern, s64 imm0) {
    /* (scalar/64-bit) */
    /* (Rd, SVE_PATTERN_SCALED) */
    //NOTTESTED00();
    int esize = 64;
    int pat = pattern;
    s64 imm = imm0 + 1;
    bool unsignedp = true;
    int ssize = 64;
    CheckSVEEnabled();
    int count = DecodePredCount(zx, pat, esize);
    u64 operand1 = Xreg_get(zx, Rdn, X31ZERO);
    s64 element1 = (s64)operand1;
    saturated_t result = SatQ2(element1, (s64)-(count * imm), ssize, unsignedp);
    Xreg_set(zx, Rdn, (u64)result.v);
}
static inline void yasve_uqdech_0x0460cc00 (TBDARG) {
    TBD("uqdech");
}
static inline void yasve_uqdech_0x0460fc00 (TBDARG) {
    TBD("uqdech");
}
static inline void yasve_uqdech_0x0470fc00 (TBDARG) {
    TBD("uqdech");
}
static inline void yasve_uqdecp_0x252b8000 (TBDARG) {
    TBD("uqdecp");
}
static inline void yasve_uqdecp_0x252b8800 (TBDARG) {
    TBD("uqdecp");
}
static inline void yasve_uqdecp_0x252b8c00 (TBDARG) {
    TBD("uqdecp");
}
static inline void yasve_uqdecw_0x04a0cc00 (TBDARG) {
    TBD("uqdecw");
}
static inline void yasve_uqdecw_0x04a0fc00 (TBDARG) {
    TBD("uqdecw");
}
static inline void yasve_uqdecw_0x04b0fc00 (TBDARG) {
    TBD("uqdecw");
}
static inline void yasve_uqincb_0x0420f400 (TBDARG) {
    TBD("uqincb");
}
static inline void yasve_uqincb_0x0430f400 (TBDARG) {
    TBD("uqincb");
}
static inline void yasve_uqincd_0x04e0c400 (TBDARG) {
    TBD("uqincd");
}
static inline void yasve_uqincd_0x04e0f400 (TBDARG) {
    TBD("uqincd");
}
static inline void yasve_uqincd_0x04f0f400 (TBDARG) {
    TBD("uqincd");
}
static inline void yasve_uqinch_0x0460c400 (TBDARG) {
    TBD("uqinch");
}
static inline void yasve_uqinch_0x0460f400 (TBDARG) {
    TBD("uqinch");
}
static inline void yasve_uqinch_0x0470f400 (TBDARG) {
    TBD("uqinch");
}
static inline void yasve_uqincp_0x25298000 (TBDARG) {
    TBD("uqincp");
}
static inline void yasve_uqincp_0x25298800 (TBDARG) {
    TBD("uqincp");
}
static inline void yasve_uqincp_0x25298c00 (TBDARG) {
    TBD("uqincp");
}
static inline void yasve_uqincw_0x04a0c400 (TBDARG) {
    TBD("uqincw");
}
static inline void yasve_uqincw_0x04a0f400 (TBDARG) {
    TBD("uqincw");
}
static inline void yasve_uqincw_0x04b0f400 (TBDARG) {
    TBD("uqincw");
}
static inline void yasve_uqsub_0x04201c00 (TBDARG) {
    TBD("uqsub");
}
static inline void yasve_uqsub_0x2527c000 (TBDARG) {
    TBD("uqsub");
}
static inline void yasve_uunpkhi_0x05333800 (CTXARG, int Zd, int Zn) {
    //NOTTESTED00();
    if (size == 0) {UnallocatedEncoding();}
    int esize = (8 << size);
    bool unsignedp = true;
    bool hi = true;
    perform_UNPACK_z(zx, esize, hi, unsignedp, Zn, Zd);
}
static inline void yasve_uunpklo_0x05323800 (CTXARG, int Zd, int Zn) {
    //NOTTESTED00();
    if (size == 0) {UnallocatedEncoding();}
    int esize = (8 << size);
    bool unsignedp = true;
    bool hi = false;
    perform_UNPACK_z(zx, esize, hi, unsignedp, Zn, Zd);
}
static inline void yasve_uxtb_0x0411a000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    if (size == 0) {UnallocatedEncoding();}
    int esize = (8 << size);
    int s_esize = 8;
    bool unsignedp = true;
    perform_EXTEND(zx, esize, s_esize, unsignedp, Zn, Zd, Pg);
}
static inline void yasve_uxth_0x0413a000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int esize = (8 << size);
    int s_esize = 16;
    bool unsignedp = true;
    perform_EXTEND(zx, esize, s_esize, unsignedp, Zn, Zd, Pg);
}
static inline void yasve_uxtw_0x04d5a000 (CTXARG, int Zd, int Pg, int Zn) {
    NOTTESTED();
    int esize = 64;
    int s_esize = 32;
    bool unsignedp = true;
    perform_EXTEND(zx, esize, s_esize, unsignedp, Zn, Zd, Pg);
}
static inline void yasve_uzp1_0x05204800 (CTXARG, int Pd, int Pn, int Pm) {
    //NOTTESTED00();
    int esize = (8 << size);
    int part = 0;
    perform_UNZIP_p(zx, esize, part, Pm, Pn, Pd);
}
static inline void yasve_uzp1_0x05206800 (CTXARG, int Zd, int Zn, int Zm) {
    //NOTTESTED00();
    int esize = (8 << size);
    int part = 0;
    perform_UNZIP_z(zx, esize, part, Zd, Zn, Zm);
}
static inline void yasve_uzp2_0x05204c00 (CTXARG, int Pd, int Pn, int Pm) {
    NOTTESTED();
    int esize = (8 << size);
    int part = 1;
    perform_UNZIP_p(zx, esize, part, Pm, Pn, Pd);
}
static inline void yasve_uzp2_0x05206c00 (CTXARG, int Zd, int Zn, int Zm) {
    NOTTESTED();
    int esize = (8 << size);
    int part = 1;
    perform_UNZIP_z(zx, esize, part, Zd, Zn, Zm);
}

static inline void yasve_whilele_0x25200410 (CTXARG, int Pd, int Rn, int Rm) {
    NOTTESTED();
    //u32 size = opr_size22;
    int esize = (8 << size);
    int rsize = 32;
    bool unsignedp = false;
    SVECmp op = Cmp_LE;
    perform_WHILE_p(zx, esize, rsize, unsignedp, op, Rn, Rm, Pd);
}
static inline void yasve_whilele_0x25201410 (CTXARG, int Pd, int Rn, int Rm) {
    NOTTESTED();
    //u32 size = opr_size22;
    int esize = (8 << size);
    int rsize = 64;
    bool unsignedp = false;
    SVECmp op = Cmp_LE;
    perform_WHILE_p(zx, esize, rsize, unsignedp, op, Rn, Rm, Pd);
}
static inline void yasve_whilelo_0x25200c00 (CTXARG, int Pd, int Rn, int Rm) {
    NOTTESTED();
    //u32 size = opr_size22;
    int esize = (8 << size);
    int rsize = 32;
    bool unsignedp = true;
    SVECmp op = Cmp_LT;
    perform_WHILE_p(zx, esize, rsize, unsignedp, op, Rn, Rm, Pd);
}
static inline void yasve_whilelo_0x25201c00 (CTXARG, int Pd, int Rn, int Rm) {
    //NOTTESTED00();
    //u32 size = opr_size22;
    int esize = (8 << size);
    int rsize = 64;
    bool unsignedp = true;
    SVECmp op = Cmp_LT;
    perform_WHILE_p(zx, esize, rsize, unsignedp, op, Rn, Rm, Pd);
}
static inline void yasve_whilels_0x25200c10 (CTXARG, int Pd, int Rn, int Rm) {
    NOTTESTED();
    //u32 size = opr_size22;
    int esize = (8 << size);
    int rsize = 32;
    bool unsignedp = true;
    SVECmp op = Cmp_LE;
    perform_WHILE_p(zx, esize, rsize, unsignedp, op, Rn, Rm, Pd);
}
static inline void yasve_whilels_0x25201c10 (CTXARG, int Pd, int Rn, int Rm) {
    NOTTESTED();
    //u32 size = opr_size22;
    int esize = (8 << size);
    int rsize = 64;
    bool unsignedp = true;
    SVECmp op = Cmp_LE;
    perform_WHILE_p(zx, esize, rsize, unsignedp, op, Rn, Rm, Pd);
}
static inline void yasve_whilelt_0x25200400 (CTXARG, int Pd, int Rn, int Rm) {
    NOTTESTED();
    //u32 size = opr_size22;
    int esize = (8 << size);
    int rsize = 32;
    bool unsignedp = false;
    SVECmp op = Cmp_LT;
    perform_WHILE_p(zx, esize, rsize, unsignedp, op, Rn, Rm, Pd);
}
static inline void yasve_whilelt_0x25201400 (CTXARG, int Pd, int Rn, int Rm) {
    NOTTESTED();
    //u32 size = opr_size22;
    int esize = (8 << size);
    int rsize = 64;
    bool unsignedp = false;
    SVECmp op = Cmp_LT;
    perform_WHILE_p(zx, esize, rsize, unsignedp, op, Rn, Rm, Pd);
}

static inline void yasve_wrffr_0x25289000 (CTXARG, int Pn) {
    NOTTESTED();
    CheckSVEEnabled();
    preg operand = zx->p[Pn];
    zx->ffr = operand;
}
static inline void yasve_zip1_0x05204000 (CTXARG, int Pd, int Pn, int Pm) {
    /* (predicates) */
    NOTTESTED();
    int esize = (8 << size);
    int part = 0;
    perform_ZIP_p(zx, esize, part, Pn, Pm, Pd);
}
static inline void yasve_zip1_0x05206000 (CTXARG, int Zd, int Zn, int Zm) {
    /* (vectors) */
    //NOTTESTED00();
    int esize = (8 << size);
    int part = 0;
    perform_ZIP_z(zx, esize, part, Zn, Zm, Zd);
}
static inline void yasve_zip2_0x05204400 (CTXARG, int Pd, int Pn, int Pm) {
    /* (predicates) */
    NOTTESTED();
    int esize = (8 << size);
    int part = 1;
    perform_ZIP_p(zx, esize, part, Pn, Pm, Pd);
}
static inline void yasve_zip2_0x05206400 (CTXARG, int Zd, int Zn, int Zm) {
    /* (vectors) */
    //NOTTESTED00();
    int esize = (8 << size);
    int part = 1;
    perform_ZIP_z(zx, esize, part, Zn, Zm, Zd);
}
