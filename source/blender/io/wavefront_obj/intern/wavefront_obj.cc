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

#include "wavefront_obj.h"
#include "wavefront_obj_exporter.h"

bool OBJ_export(bContext *C, OBJExportParams *export_params)
{
  if (export_params->print_name) {
    printf("\n OP");
  }
  if (export_params->number) {
    printf("\n%f\n", export_params->number);
  }
  exporter_main(C, export_params);
  return true;
}

bool OBJ_import(bContext *C, const char *filepath, OBJImportParams *import_params)
{
  return true;
}
