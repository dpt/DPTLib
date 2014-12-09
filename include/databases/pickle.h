/* --------------------------------------------------------------------------
 *    Name: pickle.h
 * Purpose: (De-)Serialising associative arrays
 * ----------------------------------------------------------------------- */

/**
 * \file pickle.h
 *
 * Storage of associative arrays.
 *
 * Its name borrowed from Python, this provides pickle_pickle() and
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

#ifndef DATABASES_PICKLE_H
#define DATABASES_PICKLE_H

#include <stdlib.h>

#include "base/result.h"

/* ----------------------------------------------------------------------- */

#define result_PICKLE_END               (result_BASE_PICKLE + 0)
#define result_PICKLE_SKIP              (result_BASE_PICKLE + 1)
#define result_PICKLE_INCOMPATIBLE      (result_BASE_PICKLE + 2)
#define result_PICKLE_COULDNT_OPEN_FILE (result_BASE_PICKLE + 3)
#define result_PICKLE_SYNTAX_ERROR      (result_BASE_PICKLE + 4)

/* ----------------------------------------------------------------------- */

/* methods for reading from an associative array */

typedef result_t (pickle_reader_start)(const void *assocarr,
                                       void       *opaque,
                                       void      **state);

typedef void  (pickle_reader_stop)(void *state, void *opaque);

typedef result_t (pickle_reader_next)(void        *state,
                                      const void **key,
                                      const void **value,
                                      void        *opaque);

/**
 * Methods used by pickle_pickle to read elements to serialise.
 *
 * Methods may be NULL if not required.
 */
typedef struct pickle_reader_methods
{
  pickle_reader_start *start;
  pickle_reader_stop  *stop;
  pickle_reader_next  *next;
}
pickle_reader_methods;

/* ----------------------------------------------------------------------- */

/* methods for formatting keys and values for output */

// nb. same prototypes...
typedef result_t (pickle_format_key)(const void *key,
                                     char       *buf,
                                     size_t      len,
                                     void       *opaque);

typedef result_t (pickle_format_value)(const void *value,
                                       char       *buf,
                                       size_t      len,
                                       void       *opaque);

/**
 * Methods used by pickle_pickle to transform elements for storing.
 */
typedef struct pickle_format_methods
{
  const char          *comments;    /* initial comment string to write */
  size_t               commentslen; /* length of above */
  const char          *split;       /* string inbetween key and value */
  size_t               splitlen;    /* length of above */
  pickle_format_key   *key;
  pickle_format_value *value;
}
pickle_format_methods;

/* ----------------------------------------------------------------------- */

/* methods for writing to an associative array */

typedef result_t (pickle_writer_start)(void  *assocarr,
                                       void **state,
                                       void  *opaque);

typedef void  (pickle_writer_stop)(void *state, void *opaque);

typedef result_t (pickle_writer_next)(void *state,
                                      void *key,
                                      void *value,
                                      void *opaque);

typedef struct pickle_writer_methods
{
  pickle_writer_start *start;
  pickle_writer_stop  *stop;
  pickle_writer_next  *next;
}
pickle_writer_methods;

/* ----------------------------------------------------------------------- */

/* methods for formatting keys and values for input */

// nb. same prototypes...
typedef result_t (*pickle_unformat_key)(const char *buf,
                                        size_t      len,
                                        void      **key,
                                        void       *opaque);

typedef result_t (*pickle_unformat_value)(const char *buf,
                                          size_t      len,
                                          void      **value,
                                          void       *opaque);

typedef struct pickle_unformat_methods
{
  const char           *split; /* string inbetween key and value */
  size_t                splitlen;
  pickle_unformat_key   key;
  pickle_unformat_value value;
}
pickle_unformat_methods;

/* ----------------------------------------------------------------------- */

// use streams for output?

/**
 * Store associative array 'assocarr' to the file 'filename'. Interpret the
 * contents of the associative array using the methods in 'reader'. Format
 * the keys and values for storage using the methods in 'format'.
 */
result_t pickle_pickle(const char                  *filename,
                       void                        *assocarr,
                       const pickle_reader_methods *reader,
                       const pickle_format_methods *format,
                       void                        *opaque);

/**
 * Populate associative array 'assocarr' from the file 'filename'. Insert
 * into the associative array using the methods in 'writer'. Parse
 * the keys and values from storage using the methods in 'unformat'.
 */
result_t pickle_unpickle(const char                    *filename,
                         void                          *assocarr,
                         const pickle_writer_methods   *writer,
                         const pickle_unformat_methods *unformat,
                         void                          *opaque);

void pickle_delete(const char *filename);

/* ----------------------------------------------------------------------- */

/* Ought to be in a private impl.h header file. */
#define PICKLE_SIGNATURE "1"

/* ----------------------------------------------------------------------- */

#endif /* DATABASES_PICKLE_H */
