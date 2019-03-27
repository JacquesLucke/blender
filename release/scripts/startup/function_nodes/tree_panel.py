import bpy
from . ui import NodeSidebarPanel
from . function_tree import FunctionTree

class TreePanel(bpy.types.Panel, NodeSidebarPanel):
    bl_idname = "FN_PT_tree_panel"
    bl_label = "Functions Tree"

    @classmethod
    def poll(self, context):
        try: return isinstance(context.space_data.edit_tree, FunctionTree)
        except: return False

    def draw(self, context):
        layout = self.layout
