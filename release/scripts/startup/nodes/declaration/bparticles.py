from . base import SocketDeclBase

MAX_LINK_LIMIT = 4095

class EmitterSocketDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, display_name: str):
        self.node = node
        self.identifier = identifier
        self.display_name = display_name

    def build(self, node_sockets):
        socket = node_sockets.new("bp_EmitterSocket", self.display_name, identifier=self.identifier)
        socket.link_limit = MAX_LINK_LIMIT
        return [socket]

    def validate(self, sockets):
        if len(sockets) != 1:
            return False
        socket = sockets[0]
        if socket.bl_idname != "bp_EmitterSocket":
            return False
        if socket.name != self.display_name:
            return False
        if socket.link_limit != MAX_LINK_LIMIT:
            return False
        return True

    def amount(self):
        return 1

class EventSocketDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, display_name: str):
        self.node = node
        self.identifier = identifier
        self.display_name = display_name

    def build(self, node_sockets):
        socket = node_sockets.new("bp_EventSocket", self.display_name, identifier=self.identifier)
        socket.link_limit = MAX_LINK_LIMIT
        return [socket]

    def validate(self, sockets):
        if len(sockets) != 1:
            return False
        socket = sockets[0]
        if socket.bl_idname != "bp_EventSocket":
            return False
        if socket.name != self.display_name:
            return False
        if socket.link_limit != MAX_LINK_LIMIT:
            return False
        return True

    def amount(self):
        return 1

class ControlFlowSocketDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, display_name: str):
        self.node = node
        self.identifier = identifier
        self.display_name = display_name

    def build(self, node_sockets):
        socket = node_sockets.new("bp_ControlFlowSocket", self.display_name, identifier=self.identifier)
        socket.link_limit = self.get_desired_link_limit(socket)
        return [socket]

    def get_desired_link_limit(self, socket):
        if socket.is_output:
            return 1
        else:
            return MAX_LINK_LIMIT

    def validate(self, sockets):
        if len(sockets) != 1:
            return False
        socket = sockets[0]
        if socket.bl_idname != "bp_ControlFlowSocket":
            return False
        if socket.name != self.display_name:
            return False
        if socket.link_limit != self.get_desired_link_limit(socket):
            return False
        return True

    def amount(self):
        return 1

class ParticleEffectorSocketDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, display_name: str):
        self.node = node
        self.identifier = identifier
        self.display_name = display_name

    def build(self, node_sockets):
        socket = node_sockets.new("bp_ParticleEffectorSocket", self.display_name, identifier=self.identifier)
        socket.link_limit = MAX_LINK_LIMIT
        return [socket]

    def validate(self, sockets):
        if len(sockets) != 1:
            return False
        socket = sockets[0]
        if socket.bl_idname != "bp_ParticleEffectorSocket":
            return False
        if socket.name != self.display_name:
            return False
        if socket.link_limit != MAX_LINK_LIMIT:
            return False
        return True

    def amount(self):
        return 1
