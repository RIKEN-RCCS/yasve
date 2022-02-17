/* test00.c */

#include <stdio.h>
#include <assert.h>

/* Replacement of assert. */

#if 0

#define assert(E) \
    ((E) ? (void)(0) : assert_fail(#E, __FILE__, __LINE__, __func__))

static void
assert_fail(char *e, char *f, int l, const char *h)
{
    extern void abort();
    abort();
}

#endif

#define FILL_X(S, T) \
void __attribute__ ((noinline)) \
fill_##S(T * restrict a, int n) \
{ \
    for (int i = 0; i < n; i++) { \
	a[i] = i; \
    } \
}

FILL_X(h, short)
FILL_X(i, int)
FILL_X(l, long)
FILL_X(s, float)
FILL_X(d, double)
FILL_X(uh, unsigned short)
FILL_X(ui, unsigned int)
FILL_X(ul, unsigned long)

#define ADD_X(S, T) \
void __attribute__ ((noinline)) \
add_##S(T * restrict a, T * restrict b, T * restrict c, int n) \
{ \
    for (int i = 0; i < n; i++) { \
	a[i] = b[i] + c[i]; \
    } \
}

ADD_X(h, short)
ADD_X(i, int)
ADD_X(l, long)
ADD_X(s, float)
ADD_X(d, double)
ADD_X(uh, unsigned short)
ADD_X(ui, unsigned int)
ADD_X(ul, unsigned long)

/* (The difference of the type of the counter from int to long changes
   the generated code). */

#define ADD_BY_LONG_COUNTER_X(S, T) \
void __attribute__ ((noinline)) \
add_by_long_counter_##S(T * restrict a, const T * b, const T * c, long n) \
{ \
    for (long i = 0; i < n; ++i) { \
	a[i] = b[i] + c[i]; \
    } \
}

ADD_BY_LONG_COUNTER_X(h, short)
ADD_BY_LONG_COUNTER_X(i, int)
ADD_BY_LONG_COUNTER_X(l, long)
ADD_BY_LONG_COUNTER_X(s, float)
ADD_BY_LONG_COUNTER_X(d, double)
ADD_BY_LONG_COUNTER_X(uh, unsigned short)
ADD_BY_LONG_COUNTER_X(ui, unsigned int)
ADD_BY_LONG_COUNTER_X(ul, unsigned long)

/* Note it receives the constant as an integer. */

#define ADD_CONSTANT_X(S, T) \
void __attribute__ ((noinline)) \
add_constant_##S(T * restrict a, T * restrict b, int c, int n) \
{ \
    for (int i = 0; i < n; i++) { \
	a[i] = b[i] + (T)c; \
    } \
}

ADD_CONSTANT_X(h, short)
ADD_CONSTANT_X(i, int)
ADD_CONSTANT_X(l, long)
ADD_CONSTANT_X(s, float)
ADD_CONSTANT_X(d, double)
ADD_CONSTANT_X(uh, unsigned short)
ADD_CONSTANT_X(ui, unsigned int)
ADD_CONSTANT_X(ul, unsigned long)

#define AXPY_X(S, T) \
void __attribute__ ((noinline)) \
axpy_##S(T * restrict w, T * restrict a, T * restrict x, T * restrict y, int n) \
{ \
    for (int i = 0; i < n; i++) { \
	w[i] = a[i] + (x[i] * y[i]); \
    } \
}

AXPY_X(h, short)
AXPY_X(i, int)
AXPY_X(l, long)
AXPY_X(s, float)
AXPY_X(d, double)
AXPY_X(uh, unsigned short)
AXPY_X(ui, unsigned int)
AXPY_X(ul, unsigned long)

#define ADD_CONDITIONAL_X(S, T) \
void __attribute__ ((noinline)) \
add_conditional_##S(T * restrict a, const T * b, const T * c, \
		    const T * d, long n) \
{ \
    for (long i = 0; i < n; ++i) { \
	if (d[i] > 0) { \
	    a[i] = b[i] + c[i]; \
	} \
    } \
}

ADD_CONDITIONAL_X(h, short)
ADD_CONDITIONAL_X(i, int)
ADD_CONDITIONAL_X(l, long)
ADD_CONDITIONAL_X(s, float)
ADD_CONDITIONAL_X(d, double)
ADD_CONDITIONAL_X(uh, unsigned short)
ADD_CONDITIONAL_X(ui, unsigned int)
ADD_CONDITIONAL_X(ul, unsigned long)

#define ADD_INDEXED_X(S, T) \
void __attribute__ ((noinline)) \
add_indexed_##S(T * restrict a, const T *b, const T *c, \
		  const int *d, long n) \
{ \
    for (long i = 0; i < n; ++i) { \
	a[i] = b[d[i]] + c[i]; \
    } \
}

ADD_INDEXED_X(h, short)
ADD_INDEXED_X(i, int)
ADD_INDEXED_X(l, long)
ADD_INDEXED_X(s, float)
ADD_INDEXED_X(d, double)
ADD_INDEXED_X(uh, unsigned short)
ADD_INDEXED_X(ui, unsigned int)
ADD_INDEXED_X(ul, unsigned long)

#define REDUCE_ADD_X(S, T) \
int __attribute__ ((noinline)) \
reduce_add_##S(T *a, int *b, long n) \
{ \
    int s; \
    s = 0; \
    for (long i = 0; i < n; ++i) { \
	if (b[i]) { \
	    s += a[i]; \
	} \
    } \
    return s; \
}

REDUCE_ADD_X(h, short)
REDUCE_ADD_X(i, int)
REDUCE_ADD_X(l, long)
REDUCE_ADD_X(s, float)
REDUCE_ADD_X(d, double)
REDUCE_ADD_X(uh, unsigned short)
REDUCE_ADD_X(ui, unsigned int)
REDUCE_ADD_X(ul, unsigned long)

#define N 1024

short a_h[N], b_h[N], c_h[N], w_h[N];
int a_i[N], b_i[N], c_i[N], w_i[N];
long a_l[N], b_l[N], c_l[N], w_l[N];
float a_s[N], b_s[N], c_s[N], w_s[N];
double a_d[N], b_d[N], c_d[N], w_d[N];
unsigned short a_uh[N], b_uh[N], c_uh[N], w_uh[N];
unsigned int a_ui[N], b_ui[N], c_ui[N], w_ui[N];
unsigned long a_ul[N], b_ul[N], c_ul[N], w_ul[N];
int g_i[N];

volatile int one = 1;

int
main(int argc, char **argv)
{
#define TEST_X(S, T, CV, UNSIGNEDTYPE) \
    { \
	fill_##S(a_##S, N); \
	for (int i = 0; i < N; i++) { \
	    printf("%d a=%ld\n", i, (long)a_##S[i]); \
	    assert(a_##S[i] == i); \
	} \
	\
	for (int i = 0; i < N; i++) { \
	    /*assert(one);*/ \
	    b_##S[i] = i; \
	} \
	add_constant_##S(a_##S, b_##S, CV, N); \
	for (int i = 0; i < N; i++) { \
	    printf("%d a=%ld\n", i, (long)a_##S[i]); \
	    assert(a_##S[i] == (T)(i + CV)); \
	} \
	\
	for (int i = 0; i < N; i++) { \
	    /*assert(one);*/ \
	    b_##S[i] = 2 * i; \
	    c_##S[i] = 3 * i; \
	} \
	add_##S(a_##S, b_##S, c_##S, N); \
	for (int i = 0; i < N; i++) { \
	    assert(a_##S[i] == ((2 * i) + (3 * i))); \
	} \
	\
	add_by_long_counter_##S(a_##S, b_##S, c_##S, N); \
	for (int i = 0; i < N; i++) { \
	    assert(a_##S[i] == ((2 * i) + (3 * i))); \
	} \
	\
	for (int i = 0; i < N; i++) { \
	    /*assert(one);*/ \
	    a_##S[i] = 2 * i; \
	    b_##S[i] = 3 * i; \
	    c_##S[i] = 5 * i; \
	} \
	axpy_##S(w_##S, a_##S, b_##S, c_##S, N); \
	for (int i = 0; i < N; i++) { \
	    T axpy = (T)((T)(2 * i) + ((T)(3 * i) * (T)(5 * i))); \
	    assert(w_##S[i] == axpy); \
	} \
	\
	for (int i = 0; i < N; i++) { \
	    /*assert(one);*/ \
	    a_##S[i] = 2 * i; \
	    b_##S[i] = 3 * i; \
	    c_##S[i] = 5 * i; \
	    if (!UNSIGNEDTYPE) { \
		w_##S[i] = (((i % 2) == 0) ? 1 : -1); \
	    } else { \
		w_##S[i] = (((i % 2) == 0) ? 1 : 0); \
	    } \
	} \
	add_conditional_##S(a_##S, b_##S, c_##S, w_##S, N); \
	for (int i = 0; i < N; i++) { \
	    T conditional = (((i % 2) == 0) ? ((3 * i) + (5 * i)) : (2 * i)); \
	    assert(a_##S[i] == conditional); \
	} \
	\
	for (int i = 0; i < N; i++) { \
	    /*assert(one);*/ \
	    a_##S[i] = 2 * i; \
	    b_##S[i] = 3 * i; \
	    c_##S[i] = 5 * i; \
	    g_i[i] = (i % 13); \
	} \
	add_indexed_##S(a_##S, b_##S, c_##S, g_i, N); \
	for (int i = 0; i < N; i++) { \
	    assert(a_##S[i] == ((3 * (i % 13)) + (5 * i))); \
	} \
	\
	for (int i = 0; i < N; i++) { \
	    /*assert(one);*/ \
	    a_##S[i] = 2 * i; \
	    g_i[i] = (((i % 2) == 0) ? 1 : 0); \
	} \
	T r = reduce_add_##S(a_##S, g_i, N); \
	T s; \
	s = 0; \
	for (int i = 0; i < N; i++) { \
	    /*assert(one);*/ \
	    if ((i % 2) == 0) { \
		s += (2 * i); \
	    } \
	} \
	assert(s == r); \
    }

    TEST_X(h, short, 30, 0);
    TEST_X(i, int, 0xffff000fU, 0);
    TEST_X(l, long, 0xff0fU, 0);
    TEST_X(uh, unsigned short, 30, 1);
    TEST_X(ui, unsigned int, 0xffff000fU, 1);
    TEST_X(ul, unsigned long, 0xff0fU, 1);
    TEST_X(s, float, 5, 0);
    TEST_X(d, double, 10, 0);

    return 0;
}
