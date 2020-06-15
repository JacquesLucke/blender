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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup obj
 */

#include "IO_wavefront_obj.h"
#include <chrono>

#include "wavefront_obj.hh"
#include "wavefront_obj_exporter.hh"

/**
 * Called from io_obj.c. Calls internal functions in IO:OBJ.
 * When more preferences are there, will be used to set appropriate flags.
 */
void OBJ_export(bContext *C, const OBJExportParams *export_params)
{
  std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
  io::obj::exporter_main(C, export_params);
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  std::cout << "--------\nExport Time = "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]"
            << std::endl;
}
/**
 * Called from io_obj.c. Currently not implemented.
 */
void OBJ_import(bContext *C, const OBJImportParams *import_params)
{
}
