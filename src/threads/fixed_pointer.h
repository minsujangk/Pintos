#include <stdint.h>

static int f = 1 << 14;

#define int_to_fp(n) n * f
#define fp_to_int_down(x) x/f
#define fp_to_int_nearest(x) (x>=0) ? (x + f/2) / f : (x - f/2) / f
#define fadd(x, y) x+y
#define fsub(x, y) x-y
#define fadd_int(x, n) x+n*f
#define fsub_int(x, n) x-n*f
#define fmul(x,y) ((int64_t) x) * y / f
#define fmul_int(x,n) x*n
#define fdiv(x,y) ((int64_t) x) * f / y
#define fdiv_int(x,n) x/n