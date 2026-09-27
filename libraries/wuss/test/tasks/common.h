/* wuss/test/tasks/common.h -- helpers shared by the wuss demo tasks */

#ifndef TASKS_COMMON_H
#define TASKS_COMMON_H

/* braced initialiser for a wuss_proginfo_desc_t, filling in the author and
 * version lines every demo task shares */
#define TASK_PROGINFO_DESC(name, purpose) \
  { name, purpose, "© DPTLib contributors", "1.0 (" __DATE__ ")" }

#endif /* TASKS_COMMON_H */
