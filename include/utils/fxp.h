/* fxp.h -- fixed point helpers */

#ifndef UTILS_FXP_H
#define UTILS_FXP_H

#ifdef __cplusplus
extern "C"
{
#endif

/* -------------------------------------------------------------------------- */

typedef int fix8_t;  /* e.g. 24.8 fixed point */

#define FIX8_SHIFT            (8)
#define FIX8_ONE              (1 << FIX8_SHIFT)
#define FIX8_FLOOR_TO_INT(f)  ((f) >> FIX8_SHIFT) // round down, like floor()
#define FIX8_ROUND_TO_INT(f)  (((f) + FIX8_ONE / 2) >> FIX8_SHIFT) // round to nearest, like round()
#define INT_TO_FIX8(f)        ((f) << FIX8_SHIFT)
#define FLOAT_TO_FIX8(f)      ((f) * FIX8_ONE)
#define FIX8_FRAC(f)          ((f) & (FIX8_ONE - 1))
#define FIX8_FLOOR(f)         ((f) - FIX8_FRAC(f))

/* -------------------------------------------------------------------------- */

typedef int fix16_t; /* e.g. 16.16 fixed point */

#define FIX16_SHIFT           (16)
#define FIX16_ONE             (1 << FIX16_SHIFT)
#define FIX16_FLOOR_TO_INT(f) ((f) >> FIX16_SHIFT) // round down, like floor()
#define FIX16_ROUND_TO_INT(f) (((f) + FIX16_ONE / 2) >> FIX16_SHIFT) // round to nearest, like round()
#define INT_TO_FIX16(f)       ((f) << FIX16_SHIFT)
#define FLOAT_TO_FIX16(f)     ((f) * FIX16_ONE)
#define FIX16_FRAC(f)         ((f) & (FIX16_ONE - 1))
#define FIX16_FLOOR(f)        ((f) - FIX16_FRAC(f))

/* -------------------------------------------------------------------------- */

/* Signed multiply of two 16.16 fixed-point numbers. */
int smull_fxp16(int x, int y);

/* Unsigned multiply of two 16.16 fixed-point numbers. */
unsigned int umull_fxp16(unsigned int x, unsigned int y);

/* -------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* UTILS_FXP_H */
