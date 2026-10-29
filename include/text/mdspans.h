/* text/mdspans.h -- minimal Markdown to plain text plus styled spans */

/**
 * \file mdspans.h
 *
 * Strips a small subset of Markdown down to plain text, recording where the
 * styled runs lie in the result.
 *
 * ATX headers ("#" to "######" then a space, at a line start) style their
 * whole line. Elsewhere "**bold**", "*italic*", "_italic_" and "`code`"
 * style their contents. Inline markers must close on the line they open on,
 * else they are kept as literal text; within a header they are stripped but
 * not styled.
 *
 * Nothing else (lists, links, nesting) is interpreted.
 */

#ifndef TEXT_MDSPANS_H
#define TEXT_MDSPANS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "base/result.h"

/* ----------------------------------------------------------------------- */

/** The style of one span. */
typedef enum mdspans_style
{
  mdspans_HEADER,
  mdspans_BOLD,
  mdspans_ITALIC,
  mdspans_CODE
}
mdspans_style_t;

/** One styled run: bytes [start, start + len) of the plain text. */
typedef struct mdspan
{
  int             start;
  int             len;
  mdspans_style_t style;
}
mdspan_t;

/* ----------------------------------------------------------------------- */

/**
 * Parse Markdown into plain text and its styled spans.
 *
 * Spans never overlap and are sorted by start. Spans beyond "maxspans" are
 * dropped; their text is still stripped of its markers.
 *
 * \param[in]  md       Markdown source.
 * \param[in]  mdlen    Length of "md" in bytes.
 * \param[in]  maxspans Most spans to record.
 * \param[out] text     Newly malloc'd, NUL-terminated plain text. Free it.
 * \param[out] spans    Newly malloc'd array of spans. Free it.
 * \param[out] nspans   Number of spans recorded.
 *
 * \return result_OK or result_OOM, when all outputs are zeroed.
 */
result_t mdspans_parse(const char *md,
                       int         mdlen,
                       int         maxspans,
                       char      **text,
                       mdspan_t  **spans,
                       int        *nspans);

/* ----------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* TEXT_MDSPANS_H */
