/* wuss/tasks/effecting-edit.h -- effecting task's editing dialogues */

#ifndef TASKS_EFFECTING_EDIT_H
#define TASKS_EFFECTING_EDIT_H

#ifdef WUSS_APP

#include "effecting.h"

/* the dialogues, in the order effecting_edit_find reports them */
typedef enum effecting_edit_which
{
  EFFECTING_EDIT_BLUR,
  EFFECTING_EDIT_TONE,
  EFFECTING_EDIT_LEVEL,
  EFFECTING_EDIT__LIMIT
}
effecting_edit_which_t;

typedef struct effecting_edit effecting_edit_t;

/* effecting.c, for the dialogues: the selected element (NULL if none; index
 * returned through index if non-NULL), and re-run the stack after a change
 * (non-zero rechain runs the effects too, else only the final blend) */
effecting_element_t *effecting_selected(effecting_task_t *ec, int *index);
void effecting_update(effecting_task_t *ec, int rechain);

/* make the dialogues, hidden */
result_t effecting_edit_create(effecting_task_t *ec, effecting_edit_t **out);

/* free the dialogues; safe to pass NULL */
void effecting_edit_destroy(effecting_edit_t *edit);

/* the dialogue's window, for a menu row to open */
wuss_window_t *effecting_edit_window(const effecting_edit_t *edit,
                                     effecting_edit_which_t  which);

/* which dialogue owns window, or -1 */
int effecting_edit_find(const effecting_edit_t *edit,
                        const wuss_window_t    *window);

/* a wuss_EVENT_CLOSE on window: forget it if it is one of the dialogues', so
 * effecting_edit_destroy does not close it again */
void effecting_edit_closed(effecting_edit_t *edit, wuss_window_t *window);

/* a wuss_EVENT_ICON on the dialogue's window */
result_t effecting_edit_icon(effecting_edit_t      *edit,
                             effecting_edit_which_t which,
                             const wuss_event_t    *event);

/* a wuss_EVENT_PRE_SHOW for the dialogue's window: refills it from the
 * selected element */
result_t effecting_edit_pre_show(effecting_edit_t      *edit,
                                 effecting_edit_which_t which);

#endif /* WUSS_APP */

#endif /* TASKS_EFFECTING_EDIT_H */
