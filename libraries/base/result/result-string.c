/* result-string.c -- map result_t codes to human-readable strings */

#include "base/result.h"

#include "databases/filename-db.h"
#include "databases/pickle.h"
#include "databases/tag-db.h"
#include "datastruct/atom.h"
#include "datastruct/bitfifo.h"
#include "datastruct/hash.h"
#include "geom/layout.h"
#include "geom/packer.h"
#include "io/stream.h"
#include "wuss/wuss.h"

const char *result_string(result_t err)
{
  switch (err)
  {
  case result_OK:                        return "OK";
  case result_OOM:                       return "Out of memory";
  case result_FILE_NOT_FOUND:            return "File not found";
  case result_BAD_ARG:                   return "Bad argument";
  case result_BUFFER_OVERFLOW:           return "Buffer overflow";
  case result_STOP_WALK:                 return "Walk cancelled";
  case result_PARSE_ERROR:               return "Parse error";
  case result_TOO_BIG:                   return "Value too big";
  case result_NOT_IMPLEMENTED:           return "Not implemented";
  case result_NOT_FOUND:                 return "Not found";
  case result_EXISTS:                    return "Already exists";
  case result_CLASHES:                   return "Key clashes with existing one";
  case result_NULL_ARG:                  return "NULL argument";
  case result_NOT_SUPPORTED:             return "Not supported";
  case result_INCOMPATIBLE:              return "Incompatible argument";
  case result_FOPEN_FAILED:              return "fopen() failed";

  case result_STREAM_BAD_SEEK:           return "Stream: bad seek";
  case result_STREAM_CANT_SEEK:          return "Stream: cannot seek";
  case result_STREAM_UNKNOWN_OP:         return "Stream: unknown operation";

  case result_ATOM_SET_EMPTY:            return "Atom: set is empty";
  case result_ATOM_NAME_EXISTS:          return "Atom: name already exists";
  case result_ATOM_OUT_OF_RANGE:         return "Atom: index out of range";

  case result_HASH_END:                  return "Hash: end of iteration";
  case result_HASH_BAD_CONT:             return "Hash: invalid continuation value";

  case result_PICKLE_END:                return "Pickle: end of data";
  case result_PICKLE_SKIP:               return "Pickle: skip entry";
  case result_PICKLE_INCOMPATIBLE:       return "Pickle: incompatible format";
  case result_PICKLE_COULDNT_OPEN_FILE:  return "Pickle: could not open file";
  case result_PICKLE_SYNTAX_ERROR:       return "Pickle: syntax error";

  case result_TAGDB_INCOMPATIBLE:        return "TagDB: incompatible format";
  case result_TAGDB_COULDNT_OPEN_FILE:   return "TagDB: could not open file";
  case result_TAGDB_SYNTAX_ERROR:        return "TagDB: syntax error";
  case result_TAGDB_UNKNOWN_ID:          return "TagDB: unknown id";
  case result_TAGDB_BUFF_OVERFLOW:       return "TagDB: buffer overflow";
  case result_TAGDB_UNKNOWN_TAG:         return "TagDB: unknown tag";

  case result_FILENAMEDB_INCOMPATIBLE:      return "FilenameDB: incompatible format";
  case result_FILENAMEDB_COULDNT_OPEN_FILE: return "FilenameDB: could not open file";
  case result_FILENAMEDB_SYNTAX_ERROR:      return "FilenameDB: syntax error";
  case result_FILENAMEDB_BUFF_OVERFLOW:     return "FilenameDB: buffer overflow";

  case result_TEST_PASSED:               return "Test passed";
  case result_TEST_FAILED:               return "Test failed";

  case result_PACKER_DIDNT_FIT:          return "Packer: did not fit";
  case result_PACKER_EMPTY:              return "Packer: empty";

  case result_LAYOUT_BUFFER_FULL:        return "Layout: buffer full";

  case result_BITFIFO_EMPTY:             return "BitFIFO: empty";
  case result_BITFIFO_FULL:              return "BitFIFO: full";
  case result_BITFIFO_INSUFFICIENT:      return "BitFIFO: insufficient bits";

  case result_WUSS_TOO_SMALL:            return "Wuss: dimensions too small";
  case result_WUSS_BAD_COLOUR:           return "Wuss: palette index out of range";
  case result_WUSS_BAD_ICON:             return "Wuss: malformed icon spec";

  default:                               return "Unknown error";
  }
}
