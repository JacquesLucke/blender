import bpy
from . base import BaseTree

class BParticlesTree(bpy.types.NodeTree, BaseTree):
    bl_idname = "BParticlesTree"
    bl_icon = "PARTICLES"
    bl_label = "BParticles"
