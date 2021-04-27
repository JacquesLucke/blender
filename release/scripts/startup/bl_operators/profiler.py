# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

from __future__ import annotations

import bpy


class PROFILE_OT_enable(bpy.types.Operator):
    bl_idname = "profile.enable"
    bl_label = "Enable Profiling"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(cls, context):
        return context.area.type == 'PROFILER'

    def execute(self, context):
        sprofiler = context.space_data
        sprofiler.profile_enable()
        return {'FINISHED'}

class PROFILE_OT_disable(bpy.types.Operator):
    bl_idname = "profile.disable"
    bl_label = "Disable Profiling"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(cls, context):
        return context.area.type == 'PROFILER'

    def execute(self, context):
        sprofiler = context.space_data
        sprofiler.profile_disable()
        return {'FINISHED'}

class PROFILE_OT_clear(bpy.types.Operator):
    bl_idname = "profile.clear"
    bl_label = "Clear Profile Data"
    bl_options = {'REGISTER'}

    @classmethod
    def poll(cls, context):
        return context.area.type == 'PROFILER'

    def execute(self, context):
        sprofiler = context.space_data
        sprofiler.profile_clear()
        return {'FINISHED'}

classes = (
    PROFILE_OT_enable,
    PROFILE_OT_disable,
    PROFILE_OT_clear,
)
