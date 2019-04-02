import bpy
from collections import defaultdict
from contextlib import contextmanager

from . base import DataSocket, BaseNode
from . types import type_infos
from . sockets import OperatorSocket
from . function_tree import FunctionTree
from . utils.graph import iter_connected_components

from . declaration import (
    FixedSocketDecl,
    ListSocketDecl,
    PackListDecl,
    AnyVariadicDecl,
    TreeInterfaceDecl,
)


# Managed Update
#########################################

_managed_update_depth = 0

@contextmanager
def managed_update():
    global _managed_update_depth
    try:
        _managed_update_depth += 1
        yield
    finally:
        _managed_update_depth -= 1

def is_managed_update():
    return _managed_update_depth > 0

def update_function_trees():
    if is_managed_update():
        return

    with managed_update():
        for tree in bpy.data.node_groups:
            if isinstance(tree, FunctionTree):
                update_function_tree(tree)

def update_function_tree(tree):
    run_socket_operators(TreeData(tree))
    inference_decisions(TreeData(tree))
    remove_invalid_links(TreeData(tree))


# Socket Operators
#####################################

def run_socket_operators(tree_data):
    while True:
        for link in tree_data.iter_blinks():
            if isinstance(link.to_socket, OperatorSocket):
                own_node = link.to_node
                own_socket = link.to_socket
                linked_socket = link.from_socket
                connected_sockets = list(tree_data.iter_connected_origins(own_socket))
            elif isinstance(link.from_socket, OperatorSocket):
                own_node = link.from_node
                own_socket = link.from_socket
                linked_socket = link.to_socket
                connected_sockets = list(tree_data.iter_connected_targets(own_socket))
            else:
                continue

            tree_data.tree.links.remove(link)
            decl = own_socket.get_decl(own_node)
            decl.operator_socket_call(own_socket, linked_socket, connected_sockets)
        else:
            return


# Inferencing
#######################################

from collections import namedtuple

DecisionID = namedtuple("DecisionID", ("node", "group", "prop_name"))
LinkSocket = namedtuple("LinkSocket", ("node", "socket"))

def inference_decisions(tree_data):
    decisions = dict()
    list_decisions = make_list_decisions(tree_data)
    decisions.update(list_decisions)
    decisions.update(make_pack_list_decisions(tree_data, list_decisions))

    nodes_to_rebuild = set()

    for decision_id, base_type in decisions.items():
        if getattr(decision_id.group, decision_id.prop_name) != base_type:
            setattr(decision_id.group, decision_id.prop_name, base_type)
            nodes_to_rebuild.add(decision_id.node)

    rebuild_nodes(nodes_to_rebuild)

def rebuild_nodes(nodes):
    for node in nodes:
        node.rebuild_and_try_keep_state()

def make_list_decisions(tree_data):
    decision_users = get_list_decision_ids_with_users(tree_data)
    decision_links = get_list_decision_links(tree_data)

    decisions = dict()

    for component in iter_connected_components(decision_users.keys(), decision_links):
        possible_types = set(iter_possible_list_component_types(
            component, decision_users, tree_data))

        if len(possible_types) == 1:
            base_type = next(iter(possible_types))
            for decision_id in component:
                decisions[decision_id] = base_type

    return decisions

def get_list_decision_ids_with_users(tree_data):
    decision_users = defaultdict(lambda: {"BASE": [], "LIST": []})

    for node in tree_data.iter_nodes():
        for decl, sockets in node.decl_map.iter_decl_with_sockets():
            if isinstance(decl, ListSocketDecl):
                decision_id = DecisionID(node, node, decl.prop_name)
                decision_users[decision_id][decl.list_or_base].append(sockets[0])

    return decision_users

def get_list_decision_links(tree_data):
    linked_decisions = defaultdict(set)

    for from_socket, to_socket in tree_data.iter_connections():
        from_node = tree_data.get_node(from_socket)
        to_node = tree_data.get_node(to_socket)
        from_decl = from_socket.get_decl(from_node)
        to_decl = to_socket.get_decl(to_node)
        if isinstance(from_decl, ListSocketDecl) and isinstance(to_decl, ListSocketDecl):
            if from_decl.list_or_base == to_decl.list_or_base:
                from_decision_id = DecisionID(from_node, from_node, from_decl.prop_name)
                to_decision_id = DecisionID(to_node, to_node, to_decl.prop_name)
                linked_decisions[from_decision_id].add(to_decision_id)
                linked_decisions[to_decision_id].add(from_decision_id)

    return linked_decisions

def iter_possible_list_component_types(component, decision_users, tree_data):
    for decision_id in component:
        for socket in decision_users[decision_id]["LIST"]:
            for other_node, other_socket in tree_data.iter_connected_sockets_with_nodes(socket):
                other_decl = other_socket.get_decl(other_node)
                if data_sockets_are_static(other_decl):
                    data_type = other_socket.data_type
                    if type_infos.is_list(data_type):
                        yield type_infos.to_base(data_type)
                elif isinstance(other_decl, PackListDecl):
                    yield other_decl.base_type
        for socket in decision_users[decision_id]["BASE"]:
            for other_node, other_socket in tree_data.iter_connected_sockets_with_nodes(socket):
                other_decl = other_socket.get_decl(other_node)
                if data_sockets_are_static(other_decl):
                    data_type = other_socket.data_type
                    if type_infos.is_base(data_type):
                        yield data_type
                elif isinstance(other_decl, PackListDecl):
                    yield other_decl.base_type

def make_pack_list_decisions(tree_data, list_decisions):
    decisions = dict()

    for decision_id, decl, socket in iter_pack_list_sockets(tree_data):
        assert not socket.is_output

        origin_node, origin_socket = tree_data.try_get_origin_with_node(socket)
        if origin_socket is None:
            decisions[decision_id] = "BASE"
            continue

        origin_decl = origin_socket.get_decl(origin_node)
        if data_sockets_are_static(origin_decl):
            data_type = origin_socket.data_type
            if data_type == decl.base_type:
                decisions[decision_id] = "BASE"
            elif data_type == decl.list_type:
                decisions[decision_id] = "LIST"
            else:
                decisions[decision_id] = "BASE"
        elif isinstance(origin_decl, ListSocketDecl):
            list_decision_id = DecisionID(origin_node, origin_node, origin_decl.prop_name)
            if list_decision_id in list_decisions:
                other_base_type = list_decisions[list_decision_id]
                if other_base_type == decl.base_type:
                    decisions[decision_id] = origin_decl.list_or_base
                else:
                    decisions[decision_id] = "BASE"
            else:
                old_origin_type = origin_socket.data_type
                if old_origin_type == decl.list_type:
                    decisions[decision_id] = "LIST"
                else:
                    decisions[decision_id] = "BASE"
        else:
            decisions[decision_id] = "BASE"

    return decisions

def data_sockets_are_static(decl):
    return isinstance(decl, (FixedSocketDecl, AnyVariadicDecl, TreeInterfaceDecl))

def iter_pack_list_sockets(tree_data):
    for node in tree_data.iter_nodes():
        for decl, sockets in node.decl_map.iter_decl_with_sockets():
            if isinstance(decl, PackListDecl):
                collection = decl.get_collection()
                for i, socket in enumerate(sockets[:-1]):
                    decision_id = DecisionID(node, collection[i], "state")
                    yield decision_id, decl, socket


# Remove Invalid Links
################################

def remove_invalid_links(tree_data):
    links_to_remove = set()
    for from_socket, to_socket in tree_data.iter_connections():
        if not is_link_valid(tree_data, from_socket, to_socket):
            links_to_remove.update(tree_data.iter_incident_links(to_socket))

    tree = tree_data.tree
    for link in links_to_remove:
        tree.links.remove(link)

def is_link_valid(tree_data, from_socket, to_socket):
    is_data_src = isinstance(from_socket, DataSocket)
    is_data_dst = isinstance(to_socket, DataSocket)

    if is_data_src != is_data_dst:
        return False

    if is_data_src and is_data_dst:
        from_type = from_socket.data_type
        to_type = to_socket.data_type
        return type_infos.is_link_allowed(from_type, to_type)

    return True


class TreeData:
    def __init__(self, tree):
        self.tree = tree
        self.links_mapping = find_direct_links_mapping(tree)
        self.node_by_socket = get_node_by_socket_mapping(tree)
        self.connections_mapping = find_links_following_reroutes(self.links_mapping, self.node_by_socket)
        self.link_by_sockets = get_link_by_sockets_mapping(tree)

    def iter_nodes(self):
        for node in self.tree.nodes:
            if isinstance(node, BaseNode):
                yield node

    def iter_blinks(self):
        yield from self.tree.links

    def iter_connections(self):
        for socket, others in self.connections_mapping.items():
            if socket.is_output:
                continue
            for other in others:
                yield other, socket

    def get_node(self, socket):
        return self.node_by_socket[socket]

    def iter_connected_origins(self, socket):
        node = self.get_node(socket)
        if is_reroute(node):
            socket = node.inputs[0]
            for other_socket in self.links_mapping[socket]:
                yield from self.iter_connected_origins(other_socket)
        else:
            if socket.is_output:
                yield socket
            else:
                yield from self.iter_connected_sockets(socket)

    def iter_connected_targets(self, socket):
        node = self.get_node(socket)
        if is_reroute(node):
            socket = node.outputs[0]
            for other_socket in self.links_mapping[socket]:
                yield from self.iter_connected_targets(other_socket)
        else:
            if socket.is_output:
                yield from self.iter_connected_sockets(socket)
            else:
                yield socket

    def iter_connected_sockets(self, socket):
        yield from self.connections_mapping[socket]

    def iter_connected_sockets_with_nodes(self, socket):
        for other_socket in self.iter_connected_sockets(socket):
            other_node = self.get_node(other_socket)
            yield other_node, other_socket

    def try_get_origin_with_node(self, socket):
        linked_sockets = self.connections_mapping[socket]
        amount = len(linked_sockets)
        if amount == 0:
            return None, None
        elif amount == 1:
            origin_socket = next(iter(linked_sockets))
            origin_node = self.get_node(origin_socket)
            return origin_node, origin_socket
        else:
            assert False

    def iter_incident_links(self, socket):
        if socket.is_output:
            for other_socket in self.links_mapping[socket]:
                yield self.link_by_sockets[(socket, other_socket)]
        else:
            for other_socket in self.links_mapping[socket]:
                yield self.link_by_sockets[(other_socket, socket)]

def find_direct_links_mapping(tree):
    direct_links = defaultdict(set)
    for link in tree.links:
        direct_links[link.from_socket].add(link.to_socket)
        direct_links[link.to_socket].add(link.from_socket)
    return dict(direct_links)

def get_node_by_socket_mapping(tree):
    node_by_socket = dict()
    for node in tree.nodes:
        for socket in node.inputs:
            node_by_socket[socket] = node
        for socket in node.outputs:
            node_by_socket[socket] = node
    return node_by_socket

def get_link_by_sockets_mapping(tree):
    link_by_sockets = dict()
    for link in tree.links:
        link_by_sockets[(link.from_socket, link.to_socket)] = link
    return link_by_sockets

def find_links_following_reroutes(direct_links, node_by_socket):
    links = defaultdict(set)
    for socket, direct_linked_sockets in direct_links.items():
        node = node_by_socket[socket]
        if socket.is_output:
            # handle every link only once
            continue
        if is_reroute(node):
            continue

        for other_socket in direct_linked_sockets:
            for origin_socket in iter_non_reroute_outputs(direct_links, node_by_socket, other_socket):
                links[socket].add(origin_socket)
                links[origin_socket].add(socket)
    return links

def iter_non_reroute_outputs(direct_links, node_by_socket, socket):
    assert socket.is_output

    node = node_by_socket[socket]
    if is_reroute(node):
        input_socket = node.inputs[0]
        if input_socket in direct_links:
            for origin_socket in direct_links[input_socket]:
                yield from iter_non_reroute_outputs(direct_links, node_by_socket, origin_socket)
    else:
        yield socket

def is_reroute(node):
    return node.bl_idname == "NodeReroute"