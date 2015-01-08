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
 * \param[in] width Byte width of each element.
 *
 * \return New vector, or NULL if out of memory.
 */
vector_t *vector_create(size_t width);

/**
 * Destroy an existing vector.
 *
 * \param[in] doomed Vector to destroy.
 */
void vector_destroy(vector_t *doomed);

/* ----------------------------------------------------------------------- */

/**
 * Clear the specified vector.
 *
 * \param[in] vector Vector to change.
 */
void vector_clear(vector_t *vector);

/* ----------------------------------------------------------------------- */

/**
 * Return the number of elements stored in the specified vector.
 *
 * \param[in] vector Vector to query.
 *
 * \return Number of elements stored.
 */
size_t vector_length(const vector_t *vector);

/**
 * Change the number of elements stored in the specified vector.
 *
 * Truncates the vector if smaller than the present size.
 *
 * \param[in] vector Vector to change.
 * \param[in] length New length.
 *
 * \return result_OK or result_OOM.
 */
result_t vector_set_length(vector_t *vector, size_t length);

/* ----------------------------------------------------------------------- */

/**
 * Reserve space for at least the specified number of elements in the
 * specified vector.
 *
 * \param[in] vector Vector to change.
 * \param[in] need   Required length.
 *
 * \return result_OK or result_OOM.
 */
result_t vector_ensure(vector_t *vector, size_t need);

/* ----------------------------------------------------------------------- */

/**
 * Return the byte width of element stored in the specified vector.
 *
 * \param[in] vector Vector to query.
 *
 * \return Byte width of stored elements.
 */
size_t vector_width(const vector_t *vector);

/**
 * Change the byte width of element stored in the specified vector.
 *
 * If the element width is reduced then any extra bytes are lost. If
 * increased, then zeroes are inserted.
 *
 * \param[in] vector Vector to change.
 * \param[in] width  New element width.
 *
 * \return result_OK, result_OOM or result_BAD_ARG.
 */
result_t vector_set_width(vector_t *vector, size_t width);

/* ----------------------------------------------------------------------- */

/**
 * Retrieve a pointer to an element in the vector by index.
 *
 * \param[in] vector Vector to query.
 * \param[in] index Index of element wanted.
 *
 * \return Pointer to element.
 */
void *vector_get(const vector_t *vector, int index);

/* ----------------------------------------------------------------------- */

/**
 * Assign an element of the vector by index.
 *
 * \warning Will fail silently if index is out of bounds.
 *
 * \param[in] vector Vector to change.
 * \param[in] index  Index of element to assign.
 * \param[in] value  Value to assign.
 */
void vector_set(vector_t *vector, int index, const void *value);

/* ----------------------------------------------------------------------- */

/**
 * Insert an element at the end of the vector, allocating memory if required.
 *
 * \param[in] vector Vector to change.
 * \param[in] value  Value to insert.
 *
 * \return result_OK or result_OOM.
 */
result_t vector_insert(vector_t *vector, const void *value);

/* ----------------------------------------------------------------------- */

#endif /* DATASTRUCT_VECTOR_H */
