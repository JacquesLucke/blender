import bpy
from bpy.props import *
from . utils.generic import iter_subclasses_recursive
import itertools
from collections import defaultdict

class BaseTree:
    show_mockups: BoolProperty(
        name="Show Mockups",
        default=False,
    )

    def new_link(self, a, b):
        if a.is_output:
            self.links.new(a, b)
        else:
            self.links.new(b, a)

    def update(self):
        self.sync()

    def sync(self):
        from . sync import sync_trees_and_dependent_trees
        sync_trees_and_dependent_trees({self})


class SocketValueStates:
    def __init__(self, node):
        self.node = node
        self.input_value_storage = dict()

    def store_current(self):
        for socket in self.node.inputs:
            if not isinstance(socket, DataSocket):
                continue
            storage_id = (socket.data_type, socket.identifier)
            self.input_value_storage[storage_id] = socket.get_state()

    def try_load(self):
        for socket in self.node.inputs:
            if not isinstance(socket, DataSocket):
                continue
            storage_id = (socket.data_type, socket.identifier)
            if storage_id in self.input_value_storage:
                socket.restore_state(self.input_value_storage[storage_id])


def get_new_node_identifier():
    import uuid
    return str(uuid.uuid4())


class BaseNode:
    search_terms = tuple()
    search_terms_only = False

    identifier: StringProperty()

    def init(self, context):
        self.identifier = get_new_node_identifier()

        from . sync import skip_syncing
        with skip_syncing():
            builder = self.get_node_builder()
            builder.initialize_decls()
            builder.build()

    @classmethod
    def get_search_terms(cls):
        if not cls.search_terms_only:
            yield (cls.bl_label, dict())
        yield from cls.search_terms

    def sync_tree(self, context=None):
        self.tree.sync()

    def rebuild(self):
        from . sync import skip_syncing
        with skip_syncing():
            self.socket_value_states.store_current()
            linkage_state = LinkageState(self)

            self.rebuild_fast()

            self.socket_value_states.try_load()
            linkage_state.try_restore()

    def rebuild_fast(self):
        from . sync import skip_syncing
        with skip_syncing():
            builder = self.get_node_builder()
            builder.build()
            _decl_map_per_node[self] = builder.get_sockets_decl_map()

    @property
    def tree(self):
        return self.id_data

    def get_node_builder(self):
        from . node_builder import NodeBuilder
        builder = NodeBuilder(self)
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

    def draw_socket(self, layout, socket, text, decl, index):
        decl.draw_socket(layout, socket, index)

    def draw_label(self):
        if self.hide:
            return self.draw_closed_label()
        else:
            return self.bl_label

    def draw_closed_label(self):
        return self.bl_label

    def iter_directly_used_trees(self):
        return
        yield

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

    def invoke_group_selector(self,
            layout, function_name, text,
            *, icon="NONE", settings=tuple()):
        assert isinstance(settings, tuple)
        props = layout.operator("fn.node_group_selector", text=text, icon=icon)
        self._set_common_invoke_props(props, function_name, settings)

    def _set_common_invoke_props(self, props, function_name, settings):
        props.tree_name = self.id_data.name
        props.node_name = self.name
        props.function_name = function_name
        props.settings_repr = repr(settings)

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
    def decl_map(self):
        if self not in _decl_map_per_node:
            builder = self.get_node_builder()
            _decl_map_per_node[self] = builder.get_sockets_decl_map()
        return _decl_map_per_node[self]

    @property
    def socket_value_states(self):
        if self not in _socket_value_states_per_node:
            _socket_value_states_per_node[self] = SocketValueStates(self)
        return _socket_value_states_per_node[self]

    def free(self):
        if self in _decl_map_per_node:
            del _decl_map_per_node[self]

    def copy(self, src_node):
        self.identifier = get_new_node_identifier()


class BaseSocket:
    color = (0, 0, 0, 0)

    def draw_color(self, context, node):
        return self.color

    def draw(self, context, layout, node, text):
        decl, index = self.get_decl_with_index(node)
        node.draw_socket(layout, self, text, decl, index)

    def draw_self(self, layout, node, text):
        layout.label(text=text)

    def get_index(self, node):
        if self.is_output:
            return tuple(node.outputs).index(self)
        else:
            return tuple(node.inputs).index(self)

    def to_id(self, node):
        return (node, self.is_output, self.identifier)

    def get_decl(self, node):
        return node.decl_map.get_decl_by_socket(self)

    def get_decl_with_index(self, node):
        decl_map = node.decl_map
        decl = decl_map.get_decl_by_socket(self)
        index = decl_map.get_socket_index_in_decl(self)
        return decl, index

class FunctionNode(BaseNode):
    pass

class SimulationNode(BaseNode):
    pass

class DataSocket(BaseSocket):
    def draw_self(self, layout, node, text):
        if not (self.is_linked or self.is_output) and hasattr(self, "draw_property"):
            self.draw_property(layout, node, text)
        else:
            layout.label(text=text)

    def get_state(self):
        return None

    def restore_state(self, state):
        pass

class LinkageState:
    def __init__(self, node):
        self.node = node
        self.tree = node.tree
        self.links_per_input = defaultdict(set)
        self.links_per_output = defaultdict(set)

        for link in self.tree.links:
            if link.from_node == node:
                self.links_per_output[link.from_socket.identifier].add(link.to_socket)
            if link.to_node == node:
                self.links_per_input[link.to_socket.identifier].add(link.from_socket)

    def try_restore(self):
        tree = self.tree
        for socket in self.node.inputs:
            for from_socket in self.links_per_input[socket.identifier]:
                tree.links.new(socket, from_socket)
        for socket in self.node.outputs:
            for to_socket in self.links_per_output[socket.identifier]:
                tree.links.new(to_socket, socket)


_decl_map_per_node = {}
_socket_value_states_per_node = {}

@bpy.app.handlers.persistent
def clear_cached_node_states(_):
    _decl_map_per_node.clear()
    _socket_value_states_per_node.clear()

def register():
    bpy.app.handlers.load_pre.append(clear_cached_node_states)
