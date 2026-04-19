#ifndef GPU_UTILS_H
#define GPU_UTILS_H

/* Bit manipulation macros */
#define BIT(n)              (1U << (n))
#define BIT_MASK(n)         (BIT(n) - 1U)

/* Get bit n from x (returns 0 or 1) */
#define GET_BIT(x, n)       (((x) >> (n)) & 1U)

/* Check if bit n is set in x (returns non-zero if set) */
#define IS_BIT_SET(x, n)    ((x) & BIT(n))

/* Align x up to the nearest multiple of 'align' (align must be power of 2) */
#define ALIGN_UP(x, align)  (((x) + ((align) - 1)) & ~((align) - 1))

/* Standard Min/Max */
#ifndef MIN
#define MIN(a, b)           (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b)           (((a) > (b)) ? (a) : (b))
#endif

#endif /* GPU_UTILS_H */
