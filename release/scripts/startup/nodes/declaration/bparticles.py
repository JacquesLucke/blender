import bpy
from bpy.props import *
from . base import SocketDeclBase
from .. sockets import OperatorSocket

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
        return [socket]

    def validate(self, sockets):
        if len(sockets) != 1:
            return False
        socket = sockets[0]
        if socket.bl_idname != "bp_ControlFlowSocket":
            return False
        if socket.name != self.display_name:
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

class ExecuteInputListDecl(SocketDeclBase):
    def __init__(self, node, identifier: str, prop_name: str, display_name: str):
        self.node = node
        self.identifier_prefix = identifier
        self.display_name = display_name
        self.prop_name = prop_name

    def build(self, node_sockets):
        return list(self._build(node_sockets))

    def _build(self, node_sockets):
        for i in range(self.get_execute_amount()):
            yield node_sockets.new(
                "bp_ControlFlowSocket",
                self.display_name,
                identifier=self.identifier_prefix + str(i))
        yield node_sockets.new("fn_OperatorSocket", "Operator")

    def amount(self):
        return self.get_execute_amount() + 1

    def get_execute_amount(self):
        return getattr(self.node, self.prop_name)

    def validate(self, sockets):
        if len(sockets) != self.amount():
            return False

        for i, socket in enumerate(sockets[:-1]):
            expected_identifier = self.identifier_prefix + str(i)
            if socket.identifier != expected_identifier:
                return False
            elif socket.bl_idname != "bp_ControlFlowSocket":
                return False

        if not isinstance(sockets[-1], OperatorSocket):
            return False

        return True

    def draw_socket(self, layout, socket, index):
        if isinstance(socket, OperatorSocket):
            props = layout.operator("bp.append_execute_socket", text=self.display_name, emboss=False)
            props.tree_name = self.node.tree.name
            props.node_name = self.node.name
            props.prop_name = self.prop_name
        else:
            layout.label(text=f"{self.display_name} ({index + 1})")

    def operator_socket_call(self, own_socket, linked_socket, connected_sockets):
        old_amount = self.get_execute_amount()
        new_amount = old_amount + 1
        new_identifier = self.identifier_prefix + str(old_amount)
        setattr(self.node, self.prop_name, new_amount)

        self.node.rebuild()

        new_socket = self.node.find_socket(new_identifier, False)
        self.node.tree.new_link(linked_socket, new_socket)

    @classmethod
    def Property(cls):
        return IntProperty(default=0)

class AppendAnyVariadicOperator(bpy.types.Operator):
    bl_idname = "bp.append_execute_socket"
    bl_label = "Append Execute Socket"
    bl_options = {'INTERNAL'}

    tree_name: StringProperty()
    node_name: StringProperty()
    prop_name: StringProperty()

    def execute(self, context):
        tree = bpy.data.node_groups[self.tree_name]
        node = tree.nodes[self.node_name]

        old_amount = getattr(node, self.prop_name)
        new_amount = old_amount + 1
        setattr(node, self.prop_name, new_amount)

        tree.sync()
        return {'FINISHED'}
