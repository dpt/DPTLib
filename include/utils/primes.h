/* primes.h -- cache of prime numbers */

#ifndef UTILS_PRIMES_H
#define UTILS_PRIMES_H

/* Returns the nearest prime number to 'x' from a greatly reduced range.
 * Intended as a cache of values for use when sizing data structures. */
int prime_nearest(int x);

#endif /* UTILS_PRIMES_H */
