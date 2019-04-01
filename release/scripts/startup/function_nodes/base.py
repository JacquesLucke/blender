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


class NodeStorage:
    def __init__(self, node):
        self.node = node
        builder = node.get_socket_builder()
        self.socket_decl_map = builder.get_sockets_decl_map()
        self.input_value_storage = dict()

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
    search_terms = tuple()
    search_terms_only = False

    def init(self, context):
        from . update import managed_update
        with managed_update():
            builder = self.get_socket_builder()
            builder.initialize_decls()
            builder.build()

    @classmethod
    def get_search_terms(cls):
        if not cls.search_terms_only:
            yield (cls.bl_label, dict())
        yield from cls.search_terms

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
            builder = self.get_socket_builder()
            builder.build()

        self.storage.socket_decl_map = builder.get_sockets_decl_map()
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

    def get_socket_builder(self):
        from . socket_builder import SocketBuilder
        builder = SocketBuilder(self)
        self.declaration(builder)
        return builder

    def declaration(self, builder):
        raise NotImplementedError()

    def draw_buttons(self, context, layout):
        self.draw(layout)
        for decl in self.decl_map.iter_decls():
            decl.draw_node(layout)

    def draw_buttons_ext(self, context, layout):
        self.draw_advanced(layout)

    def draw(self, layout):
        pass

    def draw_advanced(self, layout):
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
        decl_map = self.decl_map
        decl = decl_map.get_decl_by_socket(socket)
        index = decl_map.get_socket_index_in_decl(socket)
        decl.draw_socket(layout, socket, index)

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

    @property
    def decl_map(self):
        return self.storage.socket_decl_map



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
        return node.decl_map.get_decl_by_socket(self)

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

    def draw_color(self, context, node):
        from . types import type_infos
        return type_infos.get_socket_color(self.data_type)
