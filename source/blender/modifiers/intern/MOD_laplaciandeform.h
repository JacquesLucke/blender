/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */


#ifndef __MOD_LAPLACIANDEFORM_H__
#define __MOD_LAPLACIANDEFORM_H__

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"

int MOD_LaplacianDeform_Bind(Object *ob, Mesh *mesh, LaplacianDeformModifierData *modifier);
void MOD_LaplacianDeform_Unbind(LaplacianDeformModifierData *modifier);

#endif /* __MOD_LAPLACIANDEFORM_H__ */