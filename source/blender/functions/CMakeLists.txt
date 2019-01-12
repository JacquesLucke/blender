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
# The Original Code is Copyright (C) 2018, Blender Foundation
# All rights reserved.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	../../../intern/nodecompiler/src
)

set(INC_SYS
	/usr/lib/llvm-6.0/include
)

set(SRC
	function_nodes/nodes/nodes.hpp
	function_nodes/nodes/scalar_math.cpp
	function_nodes/nodes/number_input.cpp
	function_nodes/nodes/vector_math.cpp
	function_nodes/nodes/generic.cpp
	function_nodes/nodes/convert.cpp

	function_nodes/types/types.hpp
	function_nodes/types/number_types.cpp
)

blender_add_lib(bf_functions "${SRC}" "${INC}" "${INC_SYS}")
