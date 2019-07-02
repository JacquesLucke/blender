import bpy
from . base import BaseTree
from . function_tree import TreeWithFunctionNodes

class BParticlesTree(bpy.types.NodeTree, BaseTree, TreeWithFunctionNodes):
    bl_idname = "BParticlesTree"
    bl_icon = "PARTICLES"
    bl_label = "BParticles"
