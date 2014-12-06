/* bytesex.h -- reversing bytesex */

#ifndef UTILS_BYTESEX_H
#define UTILS_BYTESEX_H

#include <stddef.h>

/* Reverse bytesex.
 *
 * 's'hort or 'l'ong
 *
 * _m -> from memory (takes a pointer, reads as chars)
 * _pair -> two packed arguments
 * _block -> array of things to swap
 */

unsigned short int rev_s(unsigned short int);
unsigned short int rev_s_m(const unsigned char *);

unsigned int rev_s_pair(unsigned int);
unsigned int rev_s_pair_m(const unsigned char *);

unsigned int rev_l(unsigned int);
unsigned int rev_l_m(const unsigned char *);

void rev_s_block(unsigned short int *array, size_t nelems);
void rev_l_block(unsigned int       *array, size_t nelems);

#endif /* UTILS_BYTESEX_H */
