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
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup obj
 */

#ifndef __WAVEFRONT_OBJ_HH__
#define __WAVEFRONT_OBJ_HH__

#include "IO_wavefront_obj.h"

namespace blender {
namespace io {
namespace obj {

/**
 * Called from io_obj.c. Calls internal functions in IO:OBJ.
 * When more preferences are there, will be used to set appropriate flags.
 */
void OBJ_export(bContext *C, const OBJExportParams *export_params);
/**
 * Called from io_obj.c. Currently not implemented.
 */
void OBJ_import(bContext *C, const OBJImportParams *import_params);

}  // namespace obj
}  // namespace io
}  // namespace blender

#endif /* __WAVEFRONT_OBJ_HH__ */
