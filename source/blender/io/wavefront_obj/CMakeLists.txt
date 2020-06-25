# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../depsgraph
  ../../editors/include
  ../../imbuf
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc
  ../../ikplugin
  ../../../../intern/iksolver/extern
  ../../bmesh
)

set(INC_SYS

)

set(SRC
  intern/wavefront_obj.cc
  intern/wavefront_obj_exporter.cc
  intern/wavefront_obj_exporter_mesh.cc
  intern/wavefront_obj_exporter_nurbs.cc
  intern/wavefront_obj_file_handler.cc
  intern/wavefront_obj_importer.cc

  IO_wavefront_obj.h
  intern/wavefront_obj.hh
  intern/wavefront_obj_exporter.hh
  intern/wavefront_obj_exporter_mesh.hh
  intern/wavefront_obj_exporter_nurbs.hh
  intern/wavefront_obj_file_handler.hh
  intern/wavefront_obj_importer.hh
)

set(LIB

)

blender_add_lib(bf_wavefront_obj "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
