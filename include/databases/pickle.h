/* pickle.h -- (de-)serialising associative arrays */

/**
 * \file pickle.h
 *
 * Storage of associative arrays.
 *
 * Its name borrowed from Python, this module provides pickle_pickle() and
 * pickle_unpickle() which respectively serialise or deserialise an
 * associative array to a file of the form:
 *
 *   #<comments>
 *   <version>
 *   <key><separator><value>  (zero or more)
 *
 * When serialising, keys and values are read from through an abstract
 * pickle_reader_methods interface. They are then transformed into savable
 * strings using the methods given in the pickle_format_methods interface.
 *
 * For deserialising, the reverse is true.
 */

// FUTURE
//
// * Use streams for output.

#ifndef DATABASES_PICKLE_H
#define DATABASES_PICKLE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>

#include "base/result.h"

/* ----------------------------------------------------------------------- */

#define result_PICKLE_END               (result_BASE_PICKLE + 0)
#define result_PICKLE_SKIP              (result_BASE_PICKLE + 1)
#define result_PICKLE_INCOMPATIBLE      (result_BASE_PICKLE + 2)
#define result_PICKLE_COULDNT_OPEN_FILE (result_BASE_PICKLE + 3)
#define result_PICKLE_SYNTAX_ERROR      (result_BASE_PICKLE + 4)

/* ----------------------------------------------------------------------- */

/**
 * Interface used when pickle begins reading from the associative array.
 *
 * \param[in]   assocarr  Associative array.
 * \param[in]   opaque    The opaque pointer passed into pickle_pickle().
 * \param[out]  state     Pointer to hold reader state, if required.
 *
 * \return Error indication.
 */
typedef result_t (pickle_reader_start_t)(const void *assocarr,
                                         void       *opaque,
                                         void      **state);

/**
 * Interface used when pickle stops reading from the associative array.
 *
 * \param[in]   state     Reader state.
 * \param[in]   opaque    The opaque pointer passed into pickle_pickle().
 */
typedef void (pickle_reader_stop_t)(void *state, void *opaque);

/**
 * Interface used by pickle to read the next array entry.
 *
 * \param[in]   state     Reader state.
 * \param[out]  key       Returned key.
 * \param[out]  value     Returned value.
 * \param[in]   opaque    The opaque pointer passed into pickle_pickle().
 *
 * \return result_OK if entry returned.
 * \return result_PICKLE_END if end reached.
 */
typedef result_t (pickle_reader_next_t)(void        *state,
                                        const void **key,
                                        const void **value,
                                        void        *opaque);

/**
 * Interfaces for reading from an associative array.
 *
 * Start and stop pointers may be NULL if not required.
 */
typedef struct pickle_reader_methods
{
  pickle_reader_start_t *start;
  pickle_reader_stop_t  *stop;
  pickle_reader_next_t  *next;
}
pickle_reader_methods_t;

/* ----------------------------------------------------------------------- */

/**
 * Interface used by pickle to format a key into text.
 *
 * \param[in]   key       Key pointer as returned from pickle_reader_next.
 * \param[in]   buf       Buffer to write into.
 * \param[in]   len       Length of buffer.
 * \param[in]   opaque    The opaque pointer passed into pickle_pickle().
 *
 * \return result_OK if entry returned.
 * \return result_PICKLE_SKIP if this key:value pair ought to be skipped.
 */
typedef result_t (pickle_format_key_t)(const void *key,
                                       char       *buf,
                                       size_t      len,
                                       void       *opaque);

/**
 * Interface used by pickle to format a value into text.
 *
 * \param[in]   value     Value pointer as returned from pickle_reader_next.
 * \param[in]   buf       Buffer to write into.
 * \param[in]   len       Length of buffer.
 * \param[in]   opaque    The opaque pointer passed into pickle_pickle().
 *
 * \return result_OK if entry returned.
 * \return result_PICKLE_SKIP if this key:value pair ought to be skipped.
 */
typedef result_t (pickle_format_value_t)(const void *value,
                                         char       *buf,
                                         size_t      len,
                                         void       *opaque);

/**
 * Interfaces used to turn input keys and values into text.
 */
typedef struct pickle_format_methods
{
  const char            *comments;    /**< Initial comment string to write. */
  size_t                 commentslen; /**< Length of above. */
  const char            *split;       /**< String inbetween key and value. */
  size_t                 splitlen;    /**< Length of above. */
  pickle_format_key_t   *key;
  pickle_format_value_t *value;
}
pickle_format_methods_t;

/* ----------------------------------------------------------------------- */

/**
 * Interface used when pickle begins writing to the associative array.
 *
 * \param[in]   assocarr  Associative array.
 * \param[out]  state     Pointer to hold reader state, if required.
 * \param[in]   opaque    The opaque pointer passed into pickle_unpickle().
 *
 * \return Error indication.
 */
typedef result_t (pickle_writer_start_t)(void  *assocarr,
                                         void **state,
                                         void  *opaque);

/**
 * Interface used when pickle stops writing to the associative array.
 *
 * \param[in]   state     Reader state.
 * \param[in]   opaque    The opaque pointer passed into pickle_unpickle().
 */
typedef void (pickle_writer_stop_t)(void *state, void *opaque);

/**
 * Interface used by pickle when a key:value pair is ready to be inserted.
 *
 * \param[in]   state     Reader state.
 * \param[out]  key       Key, formatted for insertion.
 * \param[out]  value     Value, formatted for insertion.
 * \param[in]   opaque    The opaque pointer passed into pickle_unpickle().
 *
 * \return Error indication.
 */
typedef result_t (pickle_writer_next_t)(void *state,
                                        void *key,
                                        void *value,
                                        void *opaque);

/**
 * Interfaces for writing to an associative array.
 *
 * Start and stop pointers may be NULL if not required.
 */
typedef struct pickle_writer_methods
{
  pickle_writer_start_t *start;
  pickle_writer_stop_t  *stop;
  pickle_writer_next_t  *next;
}
pickle_writer_methods_t;

/* ----------------------------------------------------------------------- */

/**
 * Interface used by pickle_unpickle to parse a key from the file.
 *
 * \param[in]   buf       Buffer to parse.
 * \param[in]   len       Length of buffer.
 * \param[out]  key       Pointer to parsed key data.
 * \param[in]   opaque    The opaque pointer passed into pickle_unpickle().
 *
 * \return result_OK if entry returned.
 */
typedef result_t (*pickle_unformat_key_t)(const char *buf,
                                          size_t      len,
                                          void      **key,
                                          void       *opaque);

/**
 * Interface used by pickle_unpickle to parse a value from the file.
 *
 * \param[in]   buf       Buffer to parse.
 * \param[in]   len       Length of buffer.
 * \param[out]  value     Pointer to parsed value data.
 * \param[in]   opaque    The opaque pointer passed into pickle_unpickle().
 *
 * \return result_OK if entry returned.
 */
typedef result_t (*pickle_unformat_value_t)(const char *buf,
                                            size_t      len,
                                            void      **value,
                                            void       *opaque);

/**
 * Interfaces used by pickle_unpickle to parse input keys and values.
 */
typedef struct pickle_unformat_methods
{
  const char             *split;    /**< Separator used between key and value. */
  size_t                  splitlen; /**< Length of above. */
  pickle_unformat_key_t   key;
  pickle_unformat_value_t value;
}
pickle_unformat_methods_t;

/* ----------------------------------------------------------------------- */

/**
 * Serialise associative array 'assocarr' to the file 'filename'. Interpret
 * the contents of the associative array using the methods in 'reader'.
 * Format the keys and values for storage using the methods in 'format'.
 *
 * \param[in]   filename    Filename to save to.
 * \param[in]   assocarr    Associative array to pickle.
 * \param[in]   reader      Interfaces for reading from the associative array.
 * \param[in]   format      Interfaces for formatting the retrieved values.
 * \param[in]   opaque      Opaque pointer passed into interfaces.
 *
 * \return Error indication.
 */
result_t pickle_pickle(const char                    *filename,
                       void                          *assocarr,
                       const pickle_reader_methods_t *reader,
                       const pickle_format_methods_t *format,
                       void                          *opaque);

/**
 * Populate associative array 'assocarr' from the file 'filename'. Insert
 * into the associative array using the methods in 'writer'. Parse
 * the keys and values from storage using the methods in 'unformat'.
 *
 * \param[in]   filename    Filename to read from.
 * \param[in]   assocarr    Associative array to pickle.
 * \param[in]   writer      Interfaces for writing to the associative array.
 * \param[in]   unformat    Interfaces for parsing the retrieved values.
 * \param[in]   opaque      Opaque pointer passed into interfaces.
 *
 * \return Error indication.
 */
result_t pickle_unpickle(const char                      *filename,
                         void                            *assocarr,
                         const pickle_writer_methods_t   *writer,
                         const pickle_unformat_methods_t *unformat,
                         void                            *opaque);

/**
 * Delete the pickle file 'filename'.
 *
 * \param[in]   filename    Filename to delete.
 */
void pickle_delete(const char *filename);

/* ----------------------------------------------------------------------- */

/* FIXME: This ought to be in a private impl.h header file. */
#define PICKLE_SIGNATURE "1"

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* DATABASES_PICKLE_H */
