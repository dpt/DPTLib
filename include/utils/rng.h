/* utils/rng.h -- tiny deterministic pseudo-random number generators */

/**
 * \file rng.h
 *
 * Small, seedable, header-only PRNGs for reproducible procedural content
 * (dither, scatter, wireframe jitter, test fixtures).
 *
 * Every generator here is a pure function of its state word, so re-seeding
 * with the same value always replays the same sequence -- unlike the C
 * library's rand(), which shares one hidden global. State is a single
 * uint32_t the caller owns.
 *
 * Two step functions are provided. rng_xorshift32 is Marsaglia's xorshift32:
 * good bit avalanche, period 2^32 - 1, never returns 0 and must not be
 * seeded 0. rng_lcg32 is the Numerical Recipes linear congruential
 * generator: full period 2^32, but its low bits are weak, so take from the
 * top (rng_range below already does).
 *
 * Prefer rng_xorshift32 unless you specifically need to match an existing
 * LCG stream.
 */

#ifndef UTILS_RNG_H
#define UTILS_RNG_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

/* ----------------------------------------------------------------------- */

/** Generator state. A single 32-bit word the caller stores. */
typedef uint32_t rng_t;

/**
 * Seed a generator.
 *
 * A zero seed is remapped to 1 so rng_xorshift32 (which cannot use a zero
 * state) is always safe; this leaves rng_lcg32 unable to produce the single
 * state value 0 from a fresh seed, which is harmless.
 *
 * \param[out] s    Generator state.
 * \param[in]  seed Seed value.
 */
static __inline__ void rng_seed(rng_t *s, uint32_t seed)
{
  *s = seed ? seed : 1u;
}

/**
 * Advance the state one xorshift32 step and return it.
 *
 * \param[in,out] s Generator state (must be non-zero; rng_seed guarantees
 *                  this).
 * \return The new state word.
 */
static __inline__ uint32_t rng_xorshift32(rng_t *s)
{
  uint32_t x;

  x   = *s;
  x  ^= x << 13;
  x  ^= x >> 17;
  x  ^= x << 5;
  *s  = x;

  return x;
}

/**
 * Advance the state one LCG step and return it.
 *
 * Constants are those from Numerical Recipes. The low-order bits cycle
 * short; consumers wanting a bounded value should use rng_range, which
 * samples the high bits.
 *
 * \param[in,out] s Generator state.
 * \return The new state word.
 */
static __inline__ uint32_t rng_lcg32(rng_t *s)
{
  *s = *s * 1664525u + 1013904223u;

  return *s;
}

/**
 * Return a value in [0, n) using an xorshift32 step.
 *
 * Uses a plain modulo, so the distribution is very slightly biased for n
 * that do not divide 2^32; negligible for the small n typical of grid and
 * palette indexing. n must be positive.
 *
 * \param[in,out] s Generator state.
 * \param[in]     n Exclusive upper bound (> 0).
 * \return A value in [0, n).
 */
static __inline__ int rng_range(rng_t *s, int n)
{
  return (int) (rng_xorshift32(s) % (uint32_t) n);
}

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* UTILS_RNG_H */
