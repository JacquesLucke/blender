import bpy
from . ui import NodeSidebarPanel
from . base import BaseTree

class TreePanel(bpy.types.Panel, NodeSidebarPanel):
    bl_idname = "FN_PT_tree_panel"
    bl_label = "Tree"

    @classmethod
    def poll(self, context):
        try: return isinstance(context.space_data.edit_tree, BaseTree)
        except: return False

    def draw(self, context):
        layout = self.layout

        tree = context.space_data.edit_tree
