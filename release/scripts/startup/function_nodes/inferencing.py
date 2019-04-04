from collections import namedtuple, defaultdict
from . utils.graph import iter_connected_components
from . types import type_infos

from . declaration import (
    FixedSocketDecl,
    ListSocketDecl,
    PackListDecl,
    AnyVariadicDecl,
    TreeInterfaceDecl,
)

DecisionID = namedtuple("DecisionID", ("node", "group", "prop_name"))

def get_inferencing_decisions(tree_data):
    decisions = dict()
    list_decisions = make_list_decisions(tree_data)
    decisions.update(list_decisions)
    decisions.update(make_pack_list_decisions(tree_data, list_decisions))
    return decisions

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