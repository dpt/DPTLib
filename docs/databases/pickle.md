# [DPTLib](https://github.com/dpt/DPTLib) > databases > pickle

"pickle" is a sub-library of DPTLib for serialising and deserialising associative arrays to and from disk storage. Named after Python's pickle module, it provides a flexible framework for saving key-value data structures in a human-readable text format.

The pickle system uses an abstract interface approach, allowing it to work with any associative array implementation through pluggable reader, writer, and formatting methods. This makes it particularly useful for creating persistent storage for hash tables, dictionaries, and other key-value data structures.

- It supports customisable text-based file formats with comments and separators.
- It provides abstract interfaces for different associative array types.
- It offers flexible key and value formatting through callback methods.
- Its file format should be reasonably human-readable and editable.

## File Format

The pickle file format is a simple text-based structure:

```
#<comments>
<version>
<key><separator><value>  (zero or more)
```

- Comments begin with `#` and can span multiple lines
- Version information ensures compatibility
- Each key-value pair is on its own line with a configurable separator
- Keys and values are formatted as text strings

## Architecture

The pickle system uses several interface layers:

- **Reader interfaces** - Extract key-value pairs from associative arrays
- **Writer interfaces** - Insert key-value pairs into associative arrays
- **Format interfaces** - Convert keys/values to/from text representations
- **Unformat interfaces** - Parse text back into key/value objects

This layered approach allows pickle to work with different data structures whilst maintaining format flexibility.

## Setup

#### Include the pickle header:

```C
#include "databases/pickle.h"
```

No special initialisation is required - pickle functions can be called directly.

## Serialising (Pickling)

Use `pickle_pickle()` to save associative arrays to disk:

```C
result_t pickle_pickle(const char                    *filename,
                       void                          *assocarr,
                       const pickle_reader_methods_t *reader,
                       const pickle_format_methods_t *format,
                       void                          *opaque);
```

It requires:

- A filename for the output file
- The associative array to serialise
- Reader methods to extract data from the array
- Format methods to convert data to text
- An opaque pointer passed to all callback methods

### Reader Methods

The reader interface extracts key-value pairs from your associative array:

```C
typedef struct pickle_reader_methods
{
  pickle_reader_start_t *start;  // Optional: initialisation
  pickle_reader_stop_t  *stop;   // Optional: cleanup
  pickle_reader_next_t  *next;   // Required: get next key-value pair
}
pickle_reader_methods_t;
```

The `next` method should return `result_PICKLE_END` when no more entries exist.

### Format Methods

The format interface converts keys and values to text:

```C
typedef struct pickle_format_methods
{
  const char            *comments;    // Initial comment string
  size_t                 commentslen; // Length of comments
  const char            *split;       // Separator between key and value
  size_t                 splitlen;    // Length of separator
  pickle_format_key_t   *key;         // Format key to text
  pickle_format_value_t *value;       // Format value to text
}
pickle_format_methods_t;
```

Format methods can return `result_PICKLE_SKIP` to exclude specific key-value pairs from serialisation.

## Deserialising (Unpickling)

Use `pickle_unpickle()` to load associative arrays from disk:

```C
result_t pickle_unpickle(const char                      *filename,
                         void                            *assocarr,
                         const pickle_writer_methods_t   *writer,
                         const pickle_unformat_methods_t *unformat,
                         void                            *opaque);
```

It requires:

- A filename for the input file
- The associative array to populate
- Writer methods to insert data into the array
- Unformat methods to parse text back to objects
- An opaque pointer passed to all callback methods

### Writer Methods

The writer interface inserts key-value pairs into your associative array:

```C
typedef struct pickle_writer_methods
{
  pickle_writer_start_t *start; // Optional: initialisation
  pickle_writer_stop_t  *stop;  // Optional: cleanup
  pickle_writer_next_t  *next;  // Required: insert key-value pair
}
pickle_writer_methods_t;
```

### Unformat Methods

The unformat interface parses text back into key and value objects:

```C
typedef struct pickle_unformat_methods
{
  const char             *split;    // Expected separator
  size_t                  splitlen; // Length of separator
  pickle_unformat_key_t   key;      // Parse key from text
  pickle_unformat_value_t value;    // Parse value from text
}
pickle_unformat_methods_t;
```

## Provided Glue Methods

DPTLib includes pre-built interfaces for hash tables:

- `pickle-reader-hash.h` - Reader methods for hash table serialisation
- `pickle-writer-hash.h` - Writer methods for hash table deserialisation

These provide ready-to-use implementations for the common case of pickling hash tables.

## File Management

Use `pickle_delete()` to remove pickle files:

```C
void pickle_delete(const char *filename);
```

This is equivalent to unlinking the file but provides a consistent interface.

## Error Handling

The pickle system can return these specific result codes:

- `result_PICKLE_END` - End of data reached during enumeration
- `result_PICKLE_SKIP` - Skip this key-value pair during processing
- `result_PICKLE_INCOMPATIBLE` - File version incompatibility
- `result_PICKLE_COULDNT_OPEN_FILE` - Unable to open the specified file
- `result_PICKLE_SYNTAX_ERROR` - File format parsing error

## Example Usage Pattern

A typical pickle workflow:

1. Define reader/writer methods for your associative array type
2. Define format/unformat methods for your key and value types
3. Call `pickle_pickle()` to save data
4. Call `pickle_unpickle()` to restore data
5. Use the same format/unformat methods for consistency

## Limitations

- The file format is line-based - keys and values cannot contain newlines without special encoding.
- No built-in support for complex data types - formatting methods must handle serialisation.
- The interface requires careful memory management in unformat methods.
- No built-in compression or encryption - files are stored as plain text.
- Version compatibility is basic - only simple signature checking is performed.
