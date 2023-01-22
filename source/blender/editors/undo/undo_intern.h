/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edundo
 */

#pragma once

/* internal exports only */

struct UndoType;

#ifdef __cplusplus
extern "C" {
#endif

/* memfile_undo.c */

/** Export for ED_undo_sys. */
void ED_memfile_undosys_type(struct UndoType *ut);

#ifdef __cplusplus
}
#endif
