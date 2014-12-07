/* result.h -- generic function return values */

#ifndef BASE_RESULT_H
#define BASE_RESULT_H

typedef int result_t;

/* ----------------------------------------------------------------------- */

/* DPTLib bases */
#define result_BASE_GENERIC                     0x0000
#define result_BASE_STREAM                      0x0100
#define result_BASE_ATOM                        0x0200
#define result_BASE_HASH                        0x0300
#define result_BASE_PICKLE                      0x0400
#define result_BASE_TAGDB                       0x0500
#define result_BASE_FILENAMEDB                  0x0600

/* Non-DPTLib bases */
#define result_BASE_MMPLAYER                    0xF000

/* ----------------------------------------------------------------------- */

#define result_OK                               (result_BASE_GENERIC     + 0)
#define result_OOM                              (result_BASE_GENERIC     + 1) /* OOM = Out Of Memory */
#define result_FILE_NOT_FOUND                   (result_BASE_GENERIC     + 2)
#define result_BAD_ARG                          (result_BASE_GENERIC     + 3)
#define result_BUFFER_OVERFLOW                  (result_BASE_GENERIC     + 4)
#define result_STOP_WALK                        (result_BASE_GENERIC     + 5)

#define result_STREAM_UNKNOWN_OP                (result_BASE_STREAM      + 0)
#define result_STREAM_CANT_SEEK                 (result_BASE_STREAM      + 1)
#define result_STREAM_BAD_SEEK                  (result_BASE_STREAM      + 2)

#define result_ATOM_SET_EMPTY                   (result_BASE_ATOM        + 0)
#define result_ATOM_NAME_EXISTS                 (result_BASE_ATOM        + 1)
#define result_ATOM_OUT_OF_RANGE                (result_BASE_ATOM        + 2)

#define result_HASH_END                         (result_BASE_HASH        + 0)
#define result_HASH_BAD_CONT                    (result_BASE_HASH        + 1)

#define result_PICKLE_END                       (result_BASE_PICKLE      + 0)
#define result_PICKLE_SKIP                      (result_BASE_PICKLE      + 1)
#define result_PICKLE_INCOMPATIBLE              (result_BASE_PICKLE      + 2)
#define result_PICKLE_COULDNT_OPEN_FILE         (result_BASE_PICKLE      + 3)
#define result_PICKLE_SYNTAX_ERROR              (result_BASE_PICKLE      + 4)

#define result_TAGDB_INCOMPATIBLE               (result_BASE_TAGDB       + 0)
#define result_TAGDB_COULDNT_OPEN_FILE          (result_BASE_TAGDB       + 1)
#define result_TAGDB_SYNTAX_ERROR               (result_BASE_TAGDB       + 2)
#define result_TAGDB_UNKNOWN_ID                 (result_BASE_TAGDB       + 3)
#define result_TAGDB_BUFF_OVERFLOW              (result_BASE_TAGDB       + 4)
#define result_TAGDB_UNKNOWN_TAG                (result_BASE_TAGDB       + 5)

#define result_FILENAMEDB_INCOMPATIBLE          (result_BASE_FILENAMEDB  + 0)
#define result_FILENAMEDB_COULDNT_OPEN_FILE     (result_BASE_FILENAMEDB  + 1)
#define result_FILENAMEDB_SYNTAX_ERROR          (result_BASE_FILENAMEDB  + 2)
#define result_FILENAMEDB_BUFF_OVERFLOW         (result_BASE_FILENAMEDB  + 3)

/* ----------------------------------------------------------------------- */

#endif /* BASE_RESULT_H */
