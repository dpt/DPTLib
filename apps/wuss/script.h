/* wuss/script.h -- scripted input and state dumps for the Wuss demo */

#ifndef WUSS_SCRIPT_H
#define WUSS_SCRIPT_H

#ifdef WUSS_APP

#include <stdbool.h>

#include "base/result.h"
#include "framebuf/bitmap.h"
#include "wuss/wuss.h"

#include "frontend.h"

/* A script (--script FILE) drives the demo without a human: one command per
 * line, '#' starts a comment. Points are screen pixels, or content-relative
 * to the front-most window with the given title when prefixed with one:
 *
 *   move [TITLE] X Y            pointer to X,Y
 *   down|up|click BUTTON [TITLE] X Y
 *                               BUTTON is select, menu or adjust; click is
 *                               a down then an up
 *   key [MOD+...]KEY            KEY is a printable or return, escape, tab,
 *                               backspace, delete, insert, up, down, left,
 *                               right, home, end, pageup, pagedown, f1..f12;
 *                               MOD is shift, ctrl or alt
 *   type TEXT                   a key per character (quote to keep spaces)
 *   wait N                      let N frames pass
 *   dump                        print every window, front to back, to stdout
 *   png FILE                    save the whole framebuffer as a PNG
 *   quit                        exit (also at end of file)
 *
 * dump and png see the screen as redrawn after all preceding input. */

result_t script_open(const char *filename);

/* Called first each frame, until it returns false: fills *ev with the next
 * scripted input. false ends the script's part of this frame (a wait, or a
 * dump/png held over until input already sent has been redrawn). */
bool script_poll(wuss_t *wuss, const bitmap_t *bm, wuss_input_t *ev);

/* Returns result_OK if the script ran to its end without error. */
result_t script_close(void);

#endif /* WUSS_APP */

#endif /* WUSS_SCRIPT_H */
