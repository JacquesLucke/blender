import bpy
from bpy.props import *
from . utils.generic import iter_subclasses_recursive
import itertools
from collections import defaultdict

class BaseTree:
    def new_link(self, a, b):
        if a.is_output:
            self.links.new(a, b)
        else:
            self.links.new(b, a)

    def update(self):
        from . update import update_function_trees
        update_function_trees()


class FunctionNodeTree(bpy.types.NodeTree, BaseTree):
    bl_idname = "FunctionNodeTree"
    bl_icon = "MOD_DATA_TRANSFER"
    bl_label = "Function Nodes"


class NodeStorage:
    def __init__(self, node):
        self.node = node
        self.set_current_declaration(*node.get_sockets())
        self.input_value_storage = dict()

    def set_current_declaration(self, inputs, outputs):
        self.inputs_decl = inputs
        self.outputs_decl = outputs

        self.inputs_per_decl = {}
        sockets = iter(self.node.inputs)
        for decl in self.inputs_decl:
            group = tuple(itertools.islice(sockets, decl.amount(self.node)))
            self.inputs_per_decl[decl] = group

        self.outputs_per_decl = {}
        sockets = iter(self.node.outputs)
        for decl in self.outputs_decl:
            group = tuple(itertools.islice(sockets, decl.amount(self.node)))
            self.outputs_per_decl[decl] = group

        self.sockets_per_decl = {}
        self.sockets_per_decl.update(self.inputs_per_decl)
        self.sockets_per_decl.update(self.outputs_per_decl)

        self.decl_per_socket = {}
        self.decl_index_per_socket = {}
        for decl, sockets in self.sockets_per_decl.items():
            for i, socket in enumerate(sockets):
                self.decl_per_socket[socket] = decl
                self.decl_index_per_socket[socket] = i

    def store_socket_states(self):
        for socket in self.node.inputs:
            if not isinstance(socket, DataSocket):
                continue
            storage_id = (socket.data_type, socket.identifier)
            self.input_value_storage[storage_id] = socket.get_state()

    def try_restore_socket_states(self):
        for socket in self.node.inputs:
            if not isinstance(socket, DataSocket):
                continue
            storage_id = (socket.data_type, socket.identifier)
            if storage_id in self.input_value_storage:
                socket.restore_state(self.input_value_storage[storage_id])


_storage_per_node = {}

class BaseNode:
    def init(self, context):
        from . update import managed_update
        with managed_update():
            inputs, outputs = self.get_sockets()
            for decl in inputs:
                decl.build(self, self.inputs)
            for decl in outputs:
                decl.build(self, self.outputs)

    def refresh(self, context=None):
        from . update import update_function_trees
        self.rebuild_and_try_keep_state()
        update_function_trees()

    def rebuild_and_try_keep_state(self):
        state = self._get_state()
        self.rebuild()
        self._try_set_state(state)

    def rebuild(self):
        from . update import managed_update

        self.storage.store_socket_states()

        with managed_update():
            self.inputs.clear()
            self.outputs.clear()

            inputs, outputs = self.get_sockets()
            for decl in inputs:
                decl.build(self, self.inputs)
            for decl in outputs:
                decl.build(self, self.outputs)

        self.storage.set_current_declaration(inputs, outputs)
        self.storage.try_restore_socket_states()

    def _get_state(self):
        links_per_input = defaultdict(set)
        links_per_output = defaultdict(set)

        for link in self.tree.links:
            if link.from_node == self:
                links_per_output[link.from_socket.identifier].add(link.to_socket)
            if link.to_node == self:
                links_per_input[link.to_socket.identifier].add(link.from_socket)

        return (links_per_input, links_per_output)

    def _try_set_state(self, state):
        tree = self.tree
        for socket in self.inputs:
            for from_socket in state[0][socket.identifier]:
                tree.links.new(socket, from_socket)
        for socket in self.outputs:
            for to_socket in state[1][socket.identifier]:
                tree.links.new(to_socket, socket)

    @property
    def tree(self):
        return self.id_data

    def get_sockets():
        return [], []

    def draw_buttons(self, context, layout):
        self.draw(layout)
        for decl in self.storage.sockets_per_decl.keys():
            decl.draw_node(layout, self)

    def draw(self, layout):
        pass

    def invoke_function(self,
            layout, function_name, text,
            *, icon="NONE", settings=tuple()):
        assert isinstance(settings, tuple)
        props = layout.operator("fn.node_operator", text=text, icon=icon)
        self._set_common_invoke_props(props, function_name, settings)

    def invoke_type_selection(self,
            layout, function_name, text,
            *, mode="ALL", icon="NONE", settings=tuple()):
        assert isinstance(settings, tuple)
        props = layout.operator("fn.node_data_type_selector", text=text, icon=icon)
        self._set_common_invoke_props(props, function_name, settings)
        props.mode = mode

    def _set_common_invoke_props(self, props, function_name, settings):
        props.tree_name = self.id_data.name
        props.node_name = self.name
        props.function_name = function_name
        props.settings_repr = repr(settings)

    def draw_socket(self, socket, layout):
        storage = self.storage
        decl = storage.decl_per_socket[socket]
        index = storage.decl_index_per_socket[socket]
        decl.draw_socket(layout, self, socket, index)

    @classmethod
    def iter_final_subclasses(cls):
        yield from filter(lambda x: issubclass(x, bpy.types.Node), iter_subclasses_recursive(cls))

    def find_input(self, identifier):
        for socket in self.inputs:
            if socket.identifier == identifier:
                return socket
        else:
            return None

    def find_output(self, identifier):
        for socket in self.outputs:
            if socket.identifier == identifier:
                return socket
        else:
            return None

    def find_socket(self, identifier, is_output):
        if is_output:
            return self.find_output(identifier)
        else:
            return self.find_input(identifier)

    def iter_sockets(self):
        yield from self.inputs
        yield from self.outputs

    # Storage
    #########################

    @property
    def storage(self) -> NodeStorage:
        if self not in _storage_per_node:
            _storage_per_node[self] = NodeStorage(self)
        return _storage_per_node[self]



class BaseSocket:
    color = (0, 0, 0, 0)

    def draw_color(self, context, node):
        return self.color

    def draw(self, context, layout, node, text):
        node.draw_socket(self, layout)

    def draw_self(self, layout, node):
        layout.label(text=self.name)

    def get_index(self, node):
        if self.is_output:
            return tuple(node.outputs).index(self)
        else:
            return tuple(node.inputs).index(self)

    def to_id(self, node):
        return (node, self.is_output, self.identifier)

    def get_decl(self, node):
        return node.storage.decl_per_socket[self]

class FunctionNode(BaseNode):
    pass

class DataSocket(BaseSocket):
    data_type: StringProperty(
        maxlen=64)

    def draw_self(self, layout, node):
        text = self.name
        if not (self.is_linked or self.is_output) and hasattr(self, "draw_property"):
            self.draw_property(layout, node, text)
        else:
            layout.label(text=text)

    def get_state(self):
        return None

    def restore_state(self, state):
        pass

