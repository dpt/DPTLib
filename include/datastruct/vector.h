/* vector.h -- flexible arrays */

/**
 * \file vector.h
 *
 * Vector is an abstracted array which can be resized by both length and
 * element width.
 *
 * Elements are of a fixed size, stored contiguously and are addressed by
 * index.
 *
 * \warning If the vector is altered then pointers into the vector may be
 * invalidated (should the block move when reallocated).
 */

#ifndef DATASTRUCT_VECTOR_H
#define DATASTRUCT_VECTOR_H

#include <stddef.h>

#include "base/result.h"

/**
 * A vector.
 */
typedef struct vector vector_t;

/* ----------------------------------------------------------------------- */

/**
 * Create a new vector.
 *
 * \param width Byte width of each element.
 *
 * \return New vector, or NULL if out of memory.
 */
vector_t *vector_create(size_t width);

/**
 * Destroy an existing vector.
 *
 * \param doomed Vector to destroy.
 */
void vector_destroy(vector_t *doomed);

/* ----------------------------------------------------------------------- */

/**
 * Clear the specified vector.
 *
 * \param vector Vector.
 */
void vector_clear(vector_t *vector);

/* ----------------------------------------------------------------------- */

/**
 * Return the number of elements stored in the specified vector.
 *
 * \param vector Vector.
 *
 * \return Number of elements stored.
 */
int vector_length(const vector_t *vector);

/**
 * Change the number of elements stored in the specified vector.
 *
 * Truncates the vector if smaller than the present size.
 *
 * \param vector Vector.
 * \param length New length.
 *
 * \return Error indication.
 */
result_t vector_set_length(vector_t *vector, size_t length);

/* ----------------------------------------------------------------------- */

/**
 * Ensure that at least the specified number of elements can be stored in
 * the specified vector.
 *
 * \param vector Vector.
 * \param need   Required length.
 *
 * \return Error indication.
 */
result_t vector_ensure(vector_t *vector, size_t need);

/* ----------------------------------------------------------------------- */

/**
 * Return the byte width of element stored in the specified vector.
 *
 * \param vector Vector.
 *
 * \return Byte width of stored elements.
 */
int vector_width(const vector_t *vector);

/**
 * Change the byte width of element stored in the specified vector.
 *
 * If the element width is reduced then any extra bytes are lost. If
 * increased, then zeroes are inserted.
 *
 * \param vector Vector.
 * \param width New element width.
 *
 * \return Error indication.
 */
result_t vector_set_width(vector_t *vector, size_t width);

/* ----------------------------------------------------------------------- */

/**
 * Retrieve an element of the vector by index.
 *
 * \param vector Vector.
 * \param index Index of element wanted.
 *
 * \return Pointer to element.
 */
void *vector_get(vector_t *vector, int index);

/* ----------------------------------------------------------------------- */

/**
 * Insert an element, allocating memory if required.
 *
 * \param vector  Vector.
 * \param element Element to insert.
 *
 * \return Error indication.
 */
result_t vector_insert(vector_t *vector, void *element);

/* ----------------------------------------------------------------------- */

#endif /* DATASTRUCT_VECTOR_H */
