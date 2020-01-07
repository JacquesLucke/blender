import bpy
from bpy.props import *
from . base import SocketDeclBase
from .. sockets import OperatorSocket, ExecuteSocket

class SimulationObjectsSocketDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, display_name: str):
        self.node = node
        self.identifier = identifier
        self.display_name = display_name

    def build(self, node_sockets):
        socket = node_sockets.new("fn_SimulationObjectsSocket", self.display_name, identifier=self.identifier)
        socket.display_shape = 'DIAMOND'
        return [socket]

    def validate(self, sockets):
        if len(sockets) != 1:
            return False
        socket = sockets[0]
        if socket.bl_idname != "fn_SimulationObjectsSocket":
            return False
        if socket.name != self.display_name:
            return False
        return True

    def amount(self):
        return 1


class SimulationSolverSocketDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, display_name: str):
        self.node = node
        self.identifier = identifier
        self.display_name = display_name

    def build(self, node_sockets):
        socket = node_sockets.new("fn_SimulationSolverSocket", self.display_name, identifier=self.identifier)
        socket.display_shape = 'SQUARE'
        return [socket]

    def validate(self, sockets):
        if len(sockets) != 1:
            return False
        socket = sockets[0]
        if socket.bl_idname != "fn_SimulationSolverSocket":
            return False
        if socket.name != self.display_name:
            return False
        return True

    def amount(self):
        return 1
