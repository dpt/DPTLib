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

#include "base/utils.h"

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
static INLINE void rng_seed(rng_t *s, uint32_t seed)
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
static INLINE uint32_t rng_xorshift32(rng_t *s)
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
static INLINE uint32_t rng_lcg32(rng_t *s)
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
static INLINE int rng_range(rng_t *s, int n)
{
  return (int) (rng_xorshift32(s) % (uint32_t) n);
}

/**
 * Return the low nbits of an xorshift32 step.
 *
 * \param[in,out] s     Generator state.
 * \param[in]     nbits Number of bits wanted (> 0). 32 or more returns the
 *                      whole word.
 * \return A value in [0, 2^nbits).
 */
static INLINE uint32_t rng_bits(rng_t *s, int nbits)
{
  uint32_t r;

  r = rng_xorshift32(s);
  return (nbits >= 32) ? r : r & ((1u << nbits) - 1);
}

/* ----------------------------------------------------------------------- */

/**
 * Hash an integer key under a seed to a well-mixed 32-bit value.
 *
 * Stateless: the same seed and key always give the same value, so values can
 * be looked up in any order, unlike the step functions above.
 *
 * \param[in] seed Seed value.
 * \param[in] k    Key, e.g. a position along a line.
 * \return The hashed value.
 */
static INLINE uint32_t rng_hash32(uint32_t seed, int k)
{
  uint32_t h;

  h  = seed ^ ((uint32_t) k * 0x9E3779B9u);
  h ^= h >> 16;
  h *= 0x7FEB352Du;
  h ^= h >> 15;
  h *= 0x846CA68Bu;
  h ^= h >> 16;

  return h;
}

/**
 * Smooth one-dimensional value noise.
 *
 * A random knot sits every "period" steps; between knots the value is eased
 * from one to the next with a smoothstep curve.
 *
 * \param[in] seed   Seed value.
 * \param[in] s      Step (>= 0).
 * \param[in] period Steps between knots (> 0).
 * \return A value in [-256, 256).
 */
static INLINE int rng_noise1d(uint32_t seed, int s, int period)
{
  int k, f, v0, v1, t;

  k  = s / period;
  f  = s % period;
  v0 = (int) (rng_hash32(seed, k) & 511) - 256;
  v1 = (int) (rng_hash32(seed, k + 1) & 511) - 256;
  t  = 256 * f * f * (3 * period - 2 * f) / (period * period * period);

  return v0 + (v1 - v0) * t / 256;
}

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* UTILS_RNG_H */
