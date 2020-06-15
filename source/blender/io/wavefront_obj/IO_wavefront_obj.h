/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup obj
 */

#ifndef __IO_WAVEFRONT_OBJ_H__
#define __IO_WAVEFRONT_OBJ_H__

#include "BKE_context.h"
#include <stdio.h> /* For FILENAME_MAX. */

#ifdef __cplusplus
extern "C" {
#endif

struct OBJExportParams {
  /** Full path to the destination OBJ file to export. */
  char filepath[FILENAME_MAX];
  /** Whether mutiple frames are to be exported or not. */
  bool export_animation;
  /** The first frame to be exported. */
  int start_frame;
  /** The last frame to be exported. */
  int end_frame;
};

struct OBJImportParams {
  /** Full path to the source OBJ file to import. */
  char filepath[FILENAME_MAX];
};

void OBJ_import(bContext *C, const struct OBJImportParams *import_params);

void OBJ_export(bContext *C, const struct OBJExportParams *export_params);

#ifdef __cplusplus
}
#endif

#endif /* __IO_WAVEFRONT_OBJ_H__ */
