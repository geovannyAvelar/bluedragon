/* SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright (C) 2026 Giovani Avelar
 */
#pragma once

#include "bd-window.h"

G_BEGIN_DECLS

/* Developer aid: renders every page and the first action menu to <dir>/<name>.png, then quits.
 * Enabled by running with M711_GUI_SNAPSHOT_DIR=<dir>. */
void bd_snapshot_start(BdWindow *win, const char *dir);

G_END_DECLS
