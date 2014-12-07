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

/* Stream result codes are in io/stream.h */

/* Atom result codes are in datastruct/atom.h */

/* Hash result codes are in datastruct/hash.h */

/* Pickle result codes are in databases/pickle.h */

/* TagDB result codes are in databases/tag-db.h */

/* FilenameDB result codes are in databases/filename-db.h */

/* ----------------------------------------------------------------------- */

#endif /* BASE_RESULT_H */
