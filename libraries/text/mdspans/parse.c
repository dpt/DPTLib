/* text/mdspans/parse.c -- minimal Markdown to plain text plus styled spans */

#include <stdlib.h>
#include <string.h>

#ifdef FORTIFY
#include "fortify/fortify.h"
#endif

#include "base/result.h"
#include "text/mdspans.h"

/* ----------------------------------------------------------------------- */

/* which kind of line the cursor is in. LINE_START lasts only for the header
 * prefix check on a line's first character. */
typedef enum mdspans_state
{
  mdspans_STATE_LINE_START,
  mdspans_STATE_HEADER,
  mdspans_STATE_BODY
}
mdspans_state_t;

typedef struct mdspans_out
{
  char     *text;
  int       textlen;
  mdspan_t *spans;
  int       nspans;
  int       maxspans;
}
mdspans_out_t;

/* ----------------------------------------------------------------------- */

static void add_span(mdspans_out_t  *out,
                     int             start,
                     int             len,
                     mdspans_style_t style)
{
  if (out->nspans >= out->maxspans)
    return;

  out->spans[out->nspans].start = start;
  out->spans[out->nspans].len   = len;
  out->spans[out->nspans].style = style;
  out->nspans++;
}

/* *p is at a line start's "#": if it opens an ATX header, step past the "# "
 * prefix and return non-zero */
static int header_prefix(const char **p, const char *end)
{
  const char *q;

  q = *p;
  while (q < end && *q == '#')
    q++;
  if (q == *p || q - *p > 6 || q >= end || *q != ' ')
    return 0;

  *p = q + 1;
  return 1;
}

/* *p is at "**", "*", "_" or "`": if it closes on the same line, append the
 * run's text, add its span if "styled", step past the closing marker and
 * return non-zero. Else leave *p unchanged and return zero. */
static int emphasis(const char   **p,
                    const char    *end,
                    int            styled,
                    mdspans_out_t *out)
{
  const char     *start;
  int             markerlen;
  mdspans_style_t style;
  const char     *run;
  const char     *close;
  int             runlen;

  start = *p;
  if (start + 1 < end && start[0] == '*' && start[1] == '*')
  {
    markerlen = 2;
    style     = mdspans_BOLD;
  }
  else if (start[0] == '*' || start[0] == '_')
  {
    markerlen = 1;
    style     = mdspans_ITALIC;
  }
  else
  {
    markerlen = 1;
    style     = mdspans_CODE;
  }
  run = start + markerlen;

  for (close = run; ; close++)
  {
    if (close + markerlen > end || *close == '\n')
      return 0;
    if (memcmp(close, start, (size_t) markerlen) == 0)
      break;
  }

  runlen = (int) (close - run);
  memcpy(out->text + out->textlen, run, (size_t) runlen);
  if (styled)
    add_span(out, out->textlen, runlen, style);
  out->textlen += runlen;
  *p = close + markerlen;
  return 1;
}

/* ----------------------------------------------------------------------- */

result_t mdspans_parse(const char *md,
                       int         mdlen,
                       int         maxspans,
                       char      **text,
                       mdspan_t  **spans,
                       int        *nspans)
{
  mdspans_out_t   out;
  const char     *p;
  const char     *end;
  mdspans_state_t state;
  int             line_start;

  out.text     = malloc((size_t) mdlen + 1);
  out.textlen  = 0;
  out.spans    = malloc((size_t) (maxspans > 0 ? maxspans : 1) *
                        sizeof(*out.spans));
  out.nspans   = 0;
  out.maxspans = maxspans;
  if (out.text == NULL || out.spans == NULL)
  {
    free(out.text);
    free(out.spans);
    *text   = NULL;
    *spans  = NULL;
    *nspans = 0;
    return result_OOM;
  }

  p          = md;
  end        = md + mdlen;
  state      = mdspans_STATE_LINE_START;
  line_start = 0;

  while (p < end)
  {
    if (state == mdspans_STATE_LINE_START)
    {
      state = (*p == '#' && header_prefix(&p, end)) ? mdspans_STATE_HEADER
                                                    : mdspans_STATE_BODY;
    }
    else if (*p == '\n')
    {
      if (state == mdspans_STATE_HEADER && line_start < out.textlen)
        add_span(&out, line_start, out.textlen - line_start, mdspans_HEADER);
      out.text[out.textlen++] = *p++;
      line_start = out.textlen;
      state      = mdspans_STATE_LINE_START;
    }
    else if ((*p == '*' || *p == '_' || *p == '`') &&
             emphasis(&p, end, state == mdspans_STATE_BODY, &out))
    {
      /* state unchanged. a header's line span covers its inline runs, so
       * they are stripped but not styled: spans must not overlap */
    }
    else
    {
      out.text[out.textlen++] = *p++;
    }
  }

  if (state == mdspans_STATE_HEADER && line_start < out.textlen)
    add_span(&out, line_start, out.textlen - line_start, mdspans_HEADER);

  out.text[out.textlen] = '\0';

  *text   = out.text;
  *spans  = out.spans;
  *nspans = out.nspans;
  return result_OK;
}
