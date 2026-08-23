# [DPTLib](https://github.com/dpt/DPTLib) > databases > tag-db

"tag-db" is a sub-library of DPTLib for managing a persistent tag database. It provides an associative mapping system that allows you to assign multiple tags to keys (such as filenames or digests) with automatic storage and retrieval from disk.

The tag database maintains relationships between identifiers and descriptive tags, making it useful for categorising, labelling, and organising data. Tags are stored as compact integer identifiers internally whilst presenting human-readable names to users.

- It supports many-to-many relationships between identifiers and tags.
- It provides persistent storage with automatic disk synchronisation.
- It offers efficient enumeration and querying capabilities.
- Its performance should be reasonable for most tagging scenarios.

PrivateEye uses this to label images with descriptive metadata.

## Database Architecture

The tag database uses an associative array structure that maps string identifiers to sets of tags. Tags themselves are stored as integer identifiers with human-readable names, allowing for efficient storage and fast lookups.

The database maintains two primary relationships:

- Tag names to tag identifiers (for managing tag vocabulary)
- Identifiers to tag sets (for the actual tagging relationships)

All data is automatically persisted to disk, with explicit commit operations available for ensuring data consistency.

## Setup

#### Initialise the tag database system:

1. `tagdb_init()` to initialise the tag database subsystem.
2. `tagdb_open()` to open or create a tag database file.

```C
result_t tagdb_init(void);
result_t tagdb_open(const char *filename, tagdb_t **db);
```

The system must be initialised before opening any databases. Opening will create a new database file if one doesn't exist.

## Tag Management

Use `tagdb_add()` to create new tags:

```C
result_t tagdb_add(tagdb_t             *db,
                   const unsigned char *name,
                   tagdb_tag_t         *tag);
```

It requires a database handle and tag name. It returns the tag identifier for the newly created tag (optional - pass `NULL` if not required).

Use `tagdb_remove()` to delete tags:

```C
void tagdb_remove(tagdb_t *db, tagdb_tag_t tag);
```

Use `tagdb_rename()` to change tag names:

```C
result_t tagdb_rename(tagdb_t             *db,
                      tagdb_tag_t          tag,
                      const unsigned char *name);
```

Use `tagdb_tagtoname()` to convert tag identifiers back to names:

```C
result_t tagdb_tagtoname(tagdb_t       *db,
                         tagdb_tag_t    tag,
                         unsigned char *buf,
                         size_t        *length,
                         size_t         bufsz);
```

It requires a database handle, tag identifier, and output buffer. It returns the actual length of the tag name (optional - pass `NULL` if not required). Pass `NULL` for `buf` with `bufsz` of 0 to query the required buffer size.

## Tagging Operations

Use `tagdb_tagid()` to apply tags to identifiers:

```C
result_t tagdb_tagid(tagdb_t             *db,
                     const unsigned char *id,
                     tagdb_tag_t          tag);
```

It requires a database handle, identifier string, and tag to apply.

Use `tagdb_untagid()` to remove tags from identifiers:

```C
result_t tagdb_untagid(tagdb_t             *db,
                       const unsigned char *id,
                       tagdb_tag_t          tag);
```

Use `tagdb_forget()` to remove all knowledge of an identifier:

```C
void tagdb_forget(tagdb_t *db, const unsigned char *id);
```

This removes all tag associations for the specified identifier.

## Querying and Enumeration

Use `tagdb_get_tags_for_id()` to retrieve tags for a specific identifier:

```C
result_t tagdb_get_tags_for_id(tagdb_t             *db,
                               const unsigned char *id,
                               int                 *continuation,
                               tagdb_tag_t         *tag);
```

Use `tagdb_enumerate_tags()` to iterate through all tags with usage counts:

```C
result_t tagdb_enumerate_tags(tagdb_t     *db,
                              int         *continuation,
                              tagdb_tag_t *tag,
                              int         *count);
```

Use `tagdb_enumerate_ids()` to iterate through all identifiers:

```C
result_t tagdb_enumerate_ids(tagdb_t       *db,
                             int           *continuation,
                             unsigned char *buf,
                             size_t         bufsz);
```

Use `tagdb_enumerate_ids_by_tag()` to find identifiers with a specific tag:

```C
result_t tagdb_enumerate_ids_by_tag(tagdb_t       *db,
                                    tagdb_tag_t    tag,
                                    int           *continuation,
                                    unsigned char *buf,
                                    size_t         bufsz);
```

Use `tagdb_enumerate_ids_by_tags()` to find identifiers matching all specified tags:

```C
result_t tagdb_enumerate_ids_by_tags(tagdb_t           *db,
                                     const tagdb_tag_t *tags,
                                     int                ntags,
                                     int               *continuation,
                                     unsigned char     *buf,
                                     size_t             bufsz);
```

### Continuation Pattern

All enumeration functions use a continuation pattern for iterating through results. Set the continuation value to zero to begin enumeration - the functions will update this value and return zero when no more results are available.

## Data Persistence

Use `tagdb_commit()` to force pending changes to disk:

```C
result_t tagdb_commit(tagdb_t *db);
```

The database automatically manages persistence, but explicit commits ensure data consistency at specific points.

## Error Handling

The tag database can return these specific result codes:

- `result_TAGDB_INCOMPATIBLE` - Database format version incompatibility
- `result_TAGDB_COULDNT_OPEN_FILE` - Unable to open the database file
- `result_TAGDB_SYNTAX_ERROR` - Database file format error
- `result_TAGDB_UNKNOWN_ID` - Specified identifier not found
- `result_TAGDB_BUFF_OVERFLOW` - Output buffer too small
- `result_TAGDB_UNKNOWN_TAG` - Specified tag not found

## Cleanup

Use `tagdb_close()` to close a database when finished:

```C
void tagdb_close(tagdb_t *db);
```

Use `tagdb_fin()` to clean up the tag database subsystem:

```C
void tagdb_fin(void);
```

Call `tagdb_fin()` after closing all databases to properly clean up system resources.

## Limitations

- Tag names are limited to byte strings - no Unicode normalisation is performed.
- The database is not thread-safe - external synchronisation is required for concurrent access.
- No built-in support for hierarchical tags or tag relationships.
- Enumeration results are not sorted - applications must sort if needed.
