/* test02.c */

/* Code examples in "A sneak peek into SVE and VLA programming", by
   Francesco Petrogalli, November 2016. */

/* Replacement of assert. */

#define assert(E) \
    ((E) ? (void)(0) : assert_fail(#E, __FILE__, __LINE__, __func__))

static void
assert_fail(char *e, char *f, int l, const char *h)
{
    extern void abort();
    abort();
}

volatile int one;
#define SERIAL() assert(one == 1);

/* Listing 1.1 (Simple C loop processing integers).  example01(). */

void __attribute__ ((noinline))
add_i(int * restrict a, const int * b, const int * c, long N)
{
    long i;
    for (i = 0; i < N; ++i) {
	a[i] = b[i] + c[i];
    }
}

void __attribute__ ((noinline))
serial_add_i(int * restrict a, const int * b, const int * c, long N)
{
    long i;
    for (i = 0; i < N; ++i) {
	SERIAL();
	a[i] = b[i] + c[i];
    }
}

/* Listing 1.7 (A loop with conditional execution).  example02(). */

void __attribute__ ((noinline))
add_i_conditional(int * restrict a, const int * b, const int * c, long N,
		  const int * d)
{
    long i;
    for (i = 0; i < N; ++i) {
	if (d[i] > 0) {
	    a[i] = b[i] + c[i];
	}
    }
}

void __attribute__ ((noinline))
serial_add_i_conditional(int * restrict a, const int * b, const int * c, long N,
		  const int * d)
{
    long i;
    for (i = 0; i < N; ++i) {
	SERIAL();
	if (d[i] > 0) {
	    a[i] = b[i] + c[i];
	}
    }
}

/* Listing 1.9 (A reduction).  example02(). */

int __attribute__ ((noinline))
reduce_add_i(int *a, int *b, long N)
{
    long i;
    int s = 0;
    for (i = 0; i < N; ++i) {
	if (b[i]) {
	    s += a[i];
	}
    }
    return s;
}

int __attribute__ ((noinline))
serial_reduce_add_i(int *a, int *b, long N)
{
    long i;
    int s = 0;
    for (i = 0; i < N; ++i) {
	SERIAL();
	if (b[i]) {
	    s += a[i];
	}
    }
    return s;
}

/* Listing 1.11 (Loading data from an array of addresses).  example03(). */

void __attribute__ ((noinline))
add_indexed(int * restrict a, const int *b, const int *c,
	    long N, const int *d)
{
    long i;
    for (i = 0; i < N; ++i) {
	a[i] = b[d[i]] + c[i];
    }
}

void __attribute__ ((noinline))
serial_add_indexed(int * restrict a, const int *b, const int *c,
	    long N, const int *d)
{
    long i;
    for (i = 0; i < N; ++i) {
	a[i] = b[d[i]] + c[i];
    }
}

/* Listing 2.1 (Custom strcpy-like routine). */

void __attribute__ ((noinline))
strcpy0(char *restrict dst, const char *src)
{
    while (1) {
	*dst = *src;
	if (*src == '\0') {
	    break;
	}
	src++;
	dst++;
    }
}

void __attribute__ ((noinline))
serail_strcpy0(char *restrict dst, const char *src)
{
    while (1) {
	SERIAL();
	*dst = *src;
	if (*src == '\0') {
	    break;
	}
	src++;
	dst++;
    }
}

/* Listing 2.3 (An implementation of strcmp). */

int __attribute__ ((noinline))
strcmp0(const char *lhs, const char *rhs)
{
    while (*lhs == *rhs && *lhs != 0) {
	lhs++, rhs++;
    }
    return (*lhs - *rhs);
}

int __attribute__ ((noinline))
serial_strcmp0(const char *lhs, const char *rhs)
{
    while (*lhs == *rhs && *lhs != 0) {
	SERIAL();
	lhs++, rhs++;
    }
    return (*lhs - *rhs);
}

#define N 1025
int a0[N], a1[N], b[N], c[N], d[N];

int
main(int argc, char **argv)
{
    one = 1;

    for (long i = 0; i < N; i++) {
	a0[i] = i % N;
    }
    for (long i = 0; i < N; i++) {
	a1[i] = i % N;
    }
    for (long i = 0; i < N; i++) {
	b[i] = i % N;
    }
    for (long i = 0; i < N; i++) {
	c[i] = i % N;
    }
    for (long i = 0; i < N; i++) {
	d[i] = i % N;
    }

    add_i(a0, b, c, N);
    serial_add_i(a1, b, c, N);
    for (long i = 0; i < N; i++) {
	assert(a0[i] == a1[i]);
    }
    
    add_i_conditional(a0, b, c, N, d);
    serial_add_i_conditional(a1, b, c, N, d);
    for (long i = 0; i < N; i++) {
	assert(a0[i] == a1[i]);
    }

    int s0 = reduce_add_i(a0, b, N);
    int s1 = serial_reduce_add_i(a0, b, N);
    assert(s0 == s1);

    add_indexed(a0, b, c, N, d);
    serial_add_indexed(a1, b, c, N, d);
    for (long i = 0; i < N; i++) {
	assert(a0[i] == a1[i]);
    }

    char ss0[] = "0123456789012345678901234567890123456789";
    char dd0[2560];
    strcpy0(dd0, ss0);
    char ss1[] = "0123456789012345678901234567890123456789";
    strcmp0(ss0, ss1);
    return 0;
}
