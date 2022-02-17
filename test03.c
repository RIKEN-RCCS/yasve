/* test03.c */

/* Simple examples in "Auto-vectorization in GCC"
   [https://gcc.gnu.org/projects/tree-ssa/vectorization.html].  It is
   dated around 2011-10-23.  It is refered to in "Auto-Vectorization
   in LLVM 4.0" [http://llvm.org/docs/Vectorizers.html].  In there,
   there is a link to the benchmark code in C++ by these loops. */

int a[256], b[256], c[256];

void __attribute__ ((noinline))
example1()
{
    for (int i = 0; i < 256; i++) {
	a[i] = b[i] + c[i];
    }
}

/* feature: support for unknown loop bound */
/* feature: support for loop invariants */

void __attribute__ ((noinline))
example2a(int n, int x)
{
    for (int i = 0; i < n; i++) {
	b[i] = x;
    }
}

/* feature: general loop exit condition */
/* feature: support for bitwise operations */

void __attribute__ ((noinline))
example2b(int n, int x)
{
    while (n--) {
	a[i] = b[i]&c[i];
	i++;
    }
}

typedef int aint __attribute__ ((__aligned__(16)));

/* feature: support for (aligned) pointer accesses.  */

void __attribute__ ((noinline))
example3(int n, aint * restrict p, aint * restrict q)
{
    while (n--) {
	*p++ = *q++;
    }
}

typedef int aint __attribute__ ((__aligned__(16)));

/* feature: support for (aligned) pointer accesses */
/* feature: support for constants */

void __attribute__ ((noinline))
example4a(int n, aint * restrict p, aint * restrict q)
{
    while (n--) {
	*p++ = *q++ + 5;
    }
}

/* feature: support for read accesses with a compile time known
   misalignment */

void __attribute__ ((noinline))
example4b(int n)
{
    for (int i = 0; i < n; i++) {
	a[i] = b[i+1] + c[i+3];
    }
}

/* feature: support for if-conversion */

void __attribute__ ((noinline))
example4c(int n)
{
    int MAX = 100;
    for (int i = 0; i < n; i++) {
	int j = a[i];
	b[i] = (j > MAX ? MAX : 0);
    }
}

/* feature: support for alignable struct access */

#define N 256
struct a {
    int ca[N];
} s;

void __attribute__ ((noinline))
example5()
{
    for (int i = 0; i < N; i++) {
	s.ca[i] = 5;
    }
}

/* Example6 is in Fortran. */

/* feature: support for read accesses with an unknown misalignment */

void __attribute__ ((noinline))
example7(int x)
{
    for (int i = 0; i < N; i++) {
	a[i] = b[i+x];
    }
}

#define M 256
int a8[M][N];

/* feature: support for multidimensional arrays */

void __attribute__ ((noinline))
example8(int x)
{
    for (int i = 0; i < M; i++) {
	for (int j = 0; j < N; j++) {
	    a8[i][j] = x;
	}
    }
}

unsigned int ub[N], uc[N];

/* feature: support summation reduction. */
/* note: in case of floats use -funsafe-math-optimizations */

unsigned int __attribute__ ((noinline))
example9()
{
    unsigned int udiff;
    udiff = 0;
    for (int i = 0; i < N; i++) {
	udiff += (ub[i] - uc[i]);
    }
    return udiff;
}

/* feature: support data-types of different sizes.
   Currently only a single vector-size per target is supported;
   it can accommodate n elements such that n = vector-size/element-size
   (e.g, 4 ints, 8 shorts, or 16 chars for a vector of size 16 bytes).
   A combination of data-types of different sizes in the same loop
   requires special handling. This support is now present in mainline,
   and also includes support for type conversions.  */

void __attribute__ ((noinline))
example10a(short * restrict sa, const short *sb, const short *sc,
	   int * restrict ia, const int *ib, const int *ic)
{
    for (int i = 0; i < N; i++) {
	ia[i] = ib[i] + ic[i];
	sa[i] = sb[i] + sc[i];
    }
}

void __attribute__ ((noinline))
example10b(const short *sb, int * restrict ia)
{
    for (int i = 0; i < N; i++) {
	ia[i] = (int) sb[i];
    }
}

/* feature: support strided accesses - the data elements
   that are to be operated upon in parallel are not consecutive - they
   are accessed with a stride > 1 (in the example, the stride is 2) */

void __attribute__ ((noinline))
example11()
{
    for (int i = 0; i < N/2; i++) {
	a[i] = b[2*i+1] * c[2*i+1] - b[2*i] * c[2*i];
	d[i] = b[2*i] * c[2*i+1] + b[2*i+1] * c[2*i];
    }
}

/* induction */

void __attribute__ ((noinline))
example12()
{
    for (int i = 0; i < N; i++) {
	a[i] = i;
    }
}

int out[256];

#define M 256

/* outer-loop */

void __attribute__ ((noinline))
example13()
{
    for (int i = 0; i < M; i++) {
	int diff;
	diff = 0;
	for (int j = 0; j < N; j+=8) {
	    diff += (a[i][j] - b[i][j]);
	}
	out[i] = diff;
    }
}

#define K 256

/* double reduction */

void __attribute__ ((noinline))
example14()
{
    for (int k = 0; k < K; k++) {
	int sum;
	sum = 0;
	for (int j = 0; j < M; j++) {
	    for (int i = 0; i < N; i++) {
		sum += in[i+k][j] * coeff[i][j];
	    }
	}
	out[k] = sum;
    }
}

int x_in[256];
int x_out[256];

/* condition in nested loop */

void __attribute__ ((noinline))
example15()
{
    int i,j;
    for (int j = 0; j < M; j++) {
	int curr_a;
	int x = x_in[j];
	curr_a = a[0];

	for (int i = 0; i < N; i++) {
	    int next_a = a[i+1];
	    curr_a = x > c[i] ? curr_a : next_a;
        }
	x_out[j] = curr_a;
    }
}

int M00 = 1, M01 = 2, M02 = 3;
int M10 = 4, M11 = 5, M12 = 6;
int M20 = 7, M21 = 9, M22 = 9;

/* load permutation in loop-aware SLP */

void __attribute__ ((noinline))
example16(int * restrict pOutput, const int * pInput)
{
    for (int i = 0; i < N; i++) {
	int a = *pInput++;
	int b = *pInput++;
	int c = *pInput++;
	*pOutput++ = M00 * a + M01 * b + M02 * c;
	*pOutput++ = M10 * a + M11 * b + M12 * c;
	*pOutput++ = M20 * a + M21 * b + M22 * c;
    }
}

unsigned int in[256n];
unsigned int out[256];

/* basic block SLP */

void __attribute__ ((noinline))
example17()
{
    unsigned int *pin = &in[0];
    unsigned int *pout = &out[0];
    *pout++ = *pin++;
    *pout++ = *pin++;
    *pout++ = *pin++;
    *pout++ = *pin++;
}

int sum1;
int sum2;
int a[128];

/* Simple reduction in SLP */

void __attribute__ ((noinline))
example18()
{
    for (int i = 0; i < 64; i++) {
	sum1 += a[2*i];
	sum2 += a[2*i+1];
    }
}

/* Reduction chain in SLP */

int sum;
int a[128];

void __attribute__ ((noinline))
example19(void)
{
    for (int i = 0; i < 64; i++) {
	sum += a[2*i];
	sum += a[2*i+1];
    }
}

/* Basic block SLP with multiple types, loads with different offsets,
   misaligned load, and not-affine accesses */

void __attribute__ ((noinline))
example20(int * __restrict__ dst, short * __restrict__ src,
	  int h, int stride, short A, short B)
{
    for (int i = 0; i < h; i++) {
	dst[0] += A*src[0] + B*src[1];
	dst[1] += A*src[1] + B*src[2];
	dst[2] += A*src[2] + B*src[3];
	dst[3] += A*src[3] + B*src[4];
	dst[4] += A*src[4] + B*src[5];
	dst[5] += A*src[5] + B*src[6];
	dst[6] += A*src[6] + B*src[7];
	dst[7] += A*src[7] + B*src[8];
	dst += stride;
	src += stride;
    }
}

/* Backward access */

int __attribute__ ((noinline))
example21(int *b, int n)
{
    int a;
    a = 0;
    for (int i = n-1; i >= 0; i--) {
	a += b[i];
    }
    return a;
}

/* Alignment hints */

void __attribute__ ((noinline))
example22(int *out1, int *in1, int *in2, int n)
{
    out1 = __builtin_assume_aligned (out1, 32, 16);
    in1 = __builtin_assume_aligned (in1, 32, 16);
    in2 = __builtin_assume_aligned (in2, 32, 0);
    for (int i = 0; i < n; i++) {
	out1[i] = in1[i] * in2[i];
    }
}

/* Widening shift */

void __attribute__ ((noinline))
example23(unsigned short * restrict src, unsigned int * restrict dst)
{
    for (int i = 0; i < 256; i++) {
	*dst++ = *src++ << 7;
    }
}

/* Condition with mixed types */

#define N24 1024
float a24[N24], b24[N24];
int c24[N24];

void __attribute__ ((noinline))
example24()(short x, short y)
{
    for (int i = 0; i < N; i++) {
	c24[i] = a24[i] < b24[i] ? x : y;
    }
}

/* Loop with bool */

#define N25 1024
float a25[N25], b25[N25], c25[N25], d25[N25];
int j25[N25];

void __attribute__ ((noinline))
example25()
{
    _Bool x, y;
    for (int i = 0; i < N25; i++) {
	x = (a25[i] < b25[i]);
	y = (c25[i] < d25[i]);
	j25[i] = x & y;
    }
}
