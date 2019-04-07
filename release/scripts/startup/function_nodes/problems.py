import bpy
from bpy.props import *
from . ui import NodeSidebarPanel

warnings = []

def report_warning(node, msg):
    warnings.append((node, msg))

class ProblemsPanel(bpy.types.Panel, NodeSidebarPanel):
    bl_idname = "FN_PT_problems"
    bl_label = "Problems"

    def draw(self, context):
        layout = self.layout
        for i, (node, msg) in enumerate(warnings):
            row = layout.row(align=True)
            row.label(text=msg)
            props = row.operator("fn.move_view_to_node", text="Find")
            props.tree_name = node.tree.name
            props.node_name = node.name
            props = row.operator("fn.remove_warning", text="", icon='X')
            props.index = i

class RemoveWarning(bpy.types.Operator):
    bl_idname = "fn.remove_warning"
    bl_label = "Remove Warning"
    bl_options = {'INTERNAL'}

    index: IntProperty()

    def execute(self, context):
        del warnings[self.index]
        return {'FINISHED'}
