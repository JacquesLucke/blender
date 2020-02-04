import bpy
from bpy.props import *
from . base import SocketDeclBase

class MockupSocketDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, display_name: str, idname: str):
        self.node = node
        self.identifier = identifier
        self.display_name = display_name
        self.idname = idname

    def build(self, node_sockets):
        socket = node_sockets.new(self.idname, self.display_name, identifier=self.identifier)
        socket.display_shape = socket.default_shape
        socket.link_limit = 1000
        return [socket]

    def validate(self, sockets):
        if len(sockets) != 1:
            return False
        socket = sockets[0]
        if socket.bl_idname != self.idname:
            return False
        if socket.name != self.display_name:
            return False
        return True

    def amount(self):
        return 1
