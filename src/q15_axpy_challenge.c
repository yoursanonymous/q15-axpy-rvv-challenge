#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>
#include <stdbool.h>
#include <riscv_vector.h>

void q15_axpy_ref(const int16_t *a, const int16_t *b,
                  int16_t *y, int n, int16_t alpha)
{
    for (int i = 0; i < n; ++i) {
        int32_t prod = (int32_t)alpha * (int32_t)b[i];
        int32_t acc = prod + ((int32_t)a[i] << 15);
        int32_t rounded = (acc + (1 << 14)) >> 15;
        if (rounded >  32767) rounded =  32767;
        if (rounded < -32768) rounded = -32768;
        y[i] = (int16_t)rounded;
    }
}

#define IMPLEMENT_RVV_WIDENING(LMUL, MLMUL) \
void q15_axpy_rvv_widening_m##LMUL(const int16_t *a, const int16_t *b, \
                          int16_t *y, int n, int16_t alpha) \
{ \
    size_t i = 0; \
    __asm__ __volatile__ ("csrwi vxrm, 0"); \
    while (i < (size_t)n) { \
        size_t vl = __riscv_vsetvl_e16m##LMUL(n - i); \
        vint16m##LMUL##_t va = __riscv_vle16_v_i16m##LMUL(&a[i], vl); \
        vint16m##LMUL##_t vb = __riscv_vle16_v_i16m##LMUL(&b[i], vl); \
        vint32m##MLMUL##_t prod = __riscv_vwmul_vx_i32m##MLMUL(vb, alpha, vl); \
        vint32m##MLMUL##_t va_wide = __riscv_vsext_vf2_i32m##MLMUL(va, vl); \
        vint32m##MLMUL##_t va_shifted = __riscv_vsll_vx_i32m##MLMUL(va_wide, 15, vl); \
        vint32m##MLMUL##_t acc = __riscv_vadd_vv_i32m##MLMUL(prod, va_shifted, vl); \
        vint16m##LMUL##_t vy = __riscv_vnclip_wx_i16m##LMUL(acc, 15, vl); \
        __riscv_vse16_v_i16m##LMUL(&y[i], vy, vl); \
        i += vl; \
    } \
}

IMPLEMENT_RVV_WIDENING(1, 2)
IMPLEMENT_RVV_WIDENING(4, 8)

#define IMPLEMENT_RVV_VSMUL(LMUL) \
void q15_axpy_rvv_vsmul_m##LMUL(const int16_t *a, const int16_t *b, \
                          int16_t *y, int n, int16_t alpha) \
{ \
    size_t i = 0; \
    __asm__ __volatile__ ("csrwi vxrm, 0"); \
    while (i < (size_t)n) { \
        size_t vl = __riscv_vsetvl_e16m##LMUL(n - i); \
        vint16m##LMUL##_t va = __riscv_vle16_v_i16m##LMUL(&a[i], vl); \
        vint16m##LMUL##_t vb = __riscv_vle16_v_i16m##LMUL(&b[i], vl); \
        vint16m##LMUL##_t v_prod = __riscv_vsmul_vx_i16m##LMUL(vb, alpha, vl); \
        vint16m##LMUL##_t v_res = __riscv_vsadd_vv_i16m##LMUL(va, v_prod, vl); \
        __riscv_vse16_v_i16m##LMUL(&y[i], v_res, vl); \
        i += vl; \
    } \
}

IMPLEMENT_RVV_VSMUL(1)
IMPLEMENT_RVV_VSMUL(4)
IMPLEMENT_RVV_VSMUL(8)

#if defined(__riscv)
static inline uint64_t rdcycle(void) {
    uint64_t c;
    __asm__ __volatile__ ("rdcycle %0" : "=r"(c));
    return c;
}
#endif

void run_audit(const int16_t *ref, const int16_t *test, int n, const char *name) {
    int diff_count = 0;
    int32_t max_diff = 0;
    for (int i = 0; i < n; i++) {
        int32_t d = abs((int32_t)ref[i] - (int32_t)test[i]);
        if (d > 0) diff_count++;
        if (d > max_diff) max_diff = d;
    }
    printf("%-20s | Max Diff: %-3d | Bits off: %-5.2f%% | %s\n", 
           name, max_diff, (float)diff_count / n * 100, (diff_count == 0) ? "EXACT" : "APPROX");
}

void bench_suite(int n) {
    void *ptr_a = NULL, *ptr_b = NULL, *ptr_y_ref = NULL, *ptr_y_test = NULL;
    if (posix_memalign(&ptr_a, 128, n * sizeof(int16_t)) != 0 ||
        posix_memalign(&ptr_b, 128, n * sizeof(int16_t)) != 0 ||
        posix_memalign(&ptr_y_ref, 128, n * sizeof(int16_t)) != 0 ||
        posix_memalign(&ptr_y_test, 128, n * sizeof(int16_t)) != 0) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }
    int16_t *a = (int16_t *)ptr_a;
    int16_t *b = (int16_t *)ptr_b;
    int16_t *y_ref = (int16_t *)ptr_y_ref;
    int16_t *y_test = (int16_t *)ptr_y_test;

    for (int i = 0; i < n; i++) {
        a[i] = (rand() % 65536) - 32768;
        b[i] = (rand() % 65536) - 32768;
    }
    int16_t alpha = 16384; 

    printf("\n--- Benchmark Suite: N = %d ---\n", n);
    q15_axpy_ref(a, b, y_ref, n, alpha);
    
#if defined(__riscv)
    #define BENCH(FUNC, NAME) { \
        memset(y_test, 0, n * sizeof(int16_t)); \
        uint64_t t0 = rdcycle(); \
        FUNC(a, b, y_test, n, alpha); \
        uint64_t t1 = rdcycle(); \
        uint64_t cycles = t1 - t0; \
        run_audit(y_ref, y_test, n, NAME); \
        printf("  -> %-20s: %8llu cycles (%6.2f c/e)\n", NAME, (unsigned long long)cycles, (float)cycles/n); \
    }

    BENCH(q15_axpy_ref, "Scalar Reference")
    BENCH(q15_axpy_rvv_widening_m1, "RVV Wide m1")
    BENCH(q15_axpy_rvv_widening_m4, "RVV Wide m4")
    BENCH(q15_axpy_rvv_vsmul_m1, "RVV vsmul m1")
    BENCH(q15_axpy_rvv_vsmul_m4, "RVV vsmul m4")
    BENCH(q15_axpy_rvv_vsmul_m8, "RVV vsmul m8")
#else
    printf("Performance tracking requires RISC-V environment.\n");
#endif

    free(a); free(b); free(y_ref); free(y_test);
}

int main(void) {
    printf("Q15 AXPY RVV Performance & Precision Tool\n");
    printf("===========================================\n");

    int sizes[] = {1024, 4096, 65536};
    for (int i = 0; i < 3; i++) {
        bench_suite(sizes[i]);
    }

    return 0;
}
