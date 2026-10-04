/* appsfiler.h -- permanent icon bar "Apps" folder of launchable tasks */

#ifndef WUSS_APP_APPSFILER_H
#define WUSS_APP_APPSFILER_H

#ifdef WUSS_ICONBAR

#include "base/result.h"
#include "wuss/wuss.h"

/* Create the AppsFiler task: a permanent "Apps" icon on the icon bar.
 * Clicking it opens a Filer-style window of one folder per task category
 * (see tasks_category_count); double-clicking a folder opens a window of
 * that category's tasks, one window per category, never duplicated;
 * double-clicking a task spawns it. There is no way to quit it short of
 * destroying the wuss instance. Call once at startup, after g_tasks.wuss
 * and g_tasks.menu_task are set. */
result_t appsfiler_create(wuss_t *wuss);

#endif /* WUSS_ICONBAR */

#endif /* WUSS_APP_APPSFILER_H */
