import bpy
from collections import defaultdict
from contextlib import contextmanager

from . types import type_infos
from . sockets import OperatorSocket
from . base import DataSocket, FunctionTree
from . utils.graph import iter_connected_components

from . socket_decl import (
    FixedSocketDecl,
    ListSocketDecl,
    PackListDecl,
    AnyVariadicDecl,
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
    run_socket_operators(tree)
    inference_decisions(tree)
    remove_invalid_links(tree)


# Socket Operators
#####################################

def run_socket_operators(tree):
    while True:
        for link in tree.links:
            if isinstance(link.to_socket, OperatorSocket):
                node = link.to_node
                own_socket = link.to_socket
                other_socket = link.from_socket
            elif isinstance(link.from_socket, OperatorSocket):
                node = link.from_node
                own_socket = link.from_socket
                other_socket = link.to_socket
            else:
                continue

            tree.links.remove(link)
            decl = node.storage.decl_per_socket[own_socket]
            decl.operator_socket_call(node, own_socket, other_socket)
        else:
            return


# Inferencing
#######################################

from collections import namedtuple

DecisionID = namedtuple("DecisionID", ("node", "group", "prop_name"))
LinkSocket = namedtuple("LinkSocket", ("node", "socket"))

def inference_decisions(tree):
    linked_sockets = get_linked_sockets_dict(tree)

    decisions = dict()
    list_decisions = make_list_decisions(tree, linked_sockets)
    decisions.update(list_decisions)
    decisions.update(make_pack_list_decisions(tree, linked_sockets, list_decisions))

    nodes_to_rebuild = set()

    for decision_id, base_type in decisions.items():
        if getattr(decision_id.group, decision_id.prop_name) != base_type:
            setattr(decision_id.group, decision_id.prop_name, base_type)
            nodes_to_rebuild.add(decision_id.node)

    rebuild_nodes(nodes_to_rebuild)

def rebuild_nodes(nodes):
    for node in nodes:
        node.rebuild_and_try_keep_state()

def get_linked_sockets_dict(tree):
    linked_sockets = defaultdict(set)
    for link in tree.links:
        origin = LinkSocket(link.from_node, link.from_socket)
        target = LinkSocket(link.to_node, link.to_socket)
        linked_sockets[link.from_socket].add(target)
        linked_sockets[link.to_socket].add(origin)
    return linked_sockets

def make_list_decisions(tree, linked_sockets):
    decision_users = get_list_decision_ids_with_users(tree)
    decision_links = get_list_decision_links(tree)

    decisions = dict()

    for component in iter_connected_components(decision_users.keys(), decision_links):
        possible_types = set(iter_possible_list_component_types(
            component, decision_users, linked_sockets))

        if len(possible_types) == 1:
            base_type = next(iter(possible_types))
            for decision_id in component:
                decisions[decision_id] = base_type

    return decisions

def get_list_decision_ids_with_users(tree):
    decision_users = defaultdict(lambda: {"BASE": [], "LIST": []})

    for node in tree.nodes:
        for decl, sockets in node.storage.sockets_per_decl.items():
            if isinstance(decl, ListSocketDecl):
                decision_id = DecisionID(node, node, decl.prop_name)
                decision_users[decision_id][decl.list_or_base].append(sockets[0])

    return decision_users

def get_list_decision_links(tree):
    linked_decisions = defaultdict(set)

    for link in tree.links:
        from_decl = link.from_socket.get_decl(link.from_node)
        to_decl = link.to_socket.get_decl(link.to_node)
        if isinstance(from_decl, ListSocketDecl) and isinstance(to_decl, ListSocketDecl):
            if from_decl.list_or_base == to_decl.list_or_base:
                from_decision_id = DecisionID(link.from_node, link.from_node, from_decl.prop_name)
                to_decision_id = DecisionID(link.to_node, link.to_node, to_decl.prop_name)
                linked_decisions[from_decision_id].add(to_decision_id)
                linked_decisions[to_decision_id].add(from_decision_id)

    return linked_decisions

def iter_possible_list_component_types(component, decision_users, linked_sockets):
    for decision_id in component:
        for socket in decision_users[decision_id]["LIST"]:
            for other_node, other_socket in linked_sockets[socket]:
                other_decl = other_socket.get_decl(other_node)
                if isinstance(other_decl, (FixedSocketDecl, AnyVariadicDecl)):
                    data_type = other_socket.data_type
                    if type_infos.is_list(data_type):
                        yield type_infos.to_base(data_type)
                elif isinstance(other_decl, PackListDecl):
                    yield other_decl.base_type
        for socket in decision_users[decision_id]["BASE"]:
            for other_node, other_socket in linked_sockets[socket]:
                other_decl = other_socket.get_decl(other_node)
                if isinstance(other_decl, (FixedSocketDecl, AnyVariadicDecl)):
                    data_type = other_socket.data_type
                    if type_infos.is_base(data_type):
                        yield data_type
                elif isinstance(other_decl, PackListDecl):
                    yield other_decl.base_type

def make_pack_list_decisions(tree, linked_sockets, list_decisions):
    decisions = dict()

    for decision_id, decl, socket in iter_pack_list_sockets(tree):
        assert not socket.is_output

        if len(linked_sockets[socket]) == 0:
            decisions[decision_id] = "BASE"
            continue

        assert len(linked_sockets[socket]) == 1
        origin_node, origin_socket = next(iter(linked_sockets[socket]))
        origin_decl = origin_socket.get_decl(origin_node)
        if isinstance(origin_decl, (FixedSocketDecl, AnyVariadicDecl)):
            data_type = origin_socket.data_type
            if data_type == decl.base_type:
                decisions[decision_id] = "BASE"
            elif data_type == decl.list_type:
                decisions[decision_id] = "LIST"
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

def iter_pack_list_sockets(tree):
    for node in tree.nodes:
        for decl, sockets in node.storage.sockets_per_decl.items():
            if isinstance(decl, PackListDecl):
                collection = decl.get_collection(node)
                for i, socket in enumerate(sockets[:-1]):
                    decision_id = DecisionID(node, collection[i], "state")
                    yield decision_id, decl, socket


# Remove Invalid Links
################################

def remove_invalid_links(tree):
    for link in list(tree.links):
        if not is_link_valid(link):
            tree.links.remove(link)

def is_link_valid(link):
    is_data_src = isinstance(link.from_socket, DataSocket)
    is_data_dst = isinstance(link.to_socket, DataSocket)

    if is_data_src != is_data_dst:
        return False

    if is_data_src and is_data_dst:
        from_type = link.from_socket.data_type
        to_type = link.to_socket.data_type
        return type_infos.is_link_allowed(from_type, to_type)

    return True
