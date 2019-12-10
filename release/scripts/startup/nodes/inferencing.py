from collections import namedtuple, defaultdict
from . utils.graph import iter_connected_components
from . types import type_infos
from . tree_data import TreeData

from . declaration import (
    FixedSocketDecl,
    ListSocketDecl,
    BaseListVariadic,
    AnyVariadicDecl,
    VectorizedInputDecl,
    VectorizedOutputDecl,
)

DecisionID = namedtuple("DecisionID", ("node", "prop_name"))

def get_inferencing_decisions(tree_data: TreeData):
    list_decisions = make_list_decisions(tree_data)
    vector_decisions = make_vector_decisions(tree_data, list_decisions)
    base_list_variadic_decisions = make_base_list_variadic_decisions(tree_data, list_decisions, vector_decisions)

    decisions = dict()
    decisions.update(list_decisions)
    decisions.update(vector_decisions)
    decisions.update(base_list_variadic_decisions)
    return decisions


# Inference list type decisions
#################################################

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
                decision_id = DecisionID(node, decl.prop_name)
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
                from_decision_id = DecisionID(from_node, from_decl.prop_name)
                to_decision_id = DecisionID(to_node, to_decl.prop_name)
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
                elif isinstance(other_decl, BaseListVariadic):
                    yield other_decl.base_type
                elif isinstance(other_decl, VectorizedInputDecl):
                    yield other_decl.base_type
                elif isinstance(other_decl, VectorizedOutputDecl):
                    yield other_decl.base_type
        for socket in decision_users[decision_id]["BASE"]:
            for other_node, other_socket in tree_data.iter_connected_sockets_with_nodes(socket):
                other_decl = other_socket.get_decl(other_node)
                if data_sockets_are_static(other_decl):
                    data_type = other_socket.data_type
                    if type_infos.is_base(data_type):
                        yield data_type
                elif isinstance(other_decl, BaseListVariadic):
                    yield other_decl.base_type
                elif isinstance(other_decl, VectorizedInputDecl):
                    yield other_decl.base_type
                elif isinstance(other_decl, VectorizedOutputDecl):
                    yield other_decl.base_type


# Inference vectorization decisions
########################################

def make_vector_decisions(tree_data, list_decisions):
    graph, input_sockets, output_sockets = get_vector_decisions_graph(tree_data)

    decisions = dict()
    decision_ids_with_collision = set()

    for initial_decision_id, decision in iter_obligatory_vector_decisions(graph, input_sockets, output_sockets, tree_data, list_decisions):
        for decision_id in graph.reachable(initial_decision_id):
            if decision_id in decisions:
                if decisions[decision_id] != decision:
                    decision_ids_with_collision.add(decision_id)
            else:
                decisions[decision_id] = decision

    for decision_id in graph.V:
        decisions.setdefault(decision_id, "BASE")

    while len(decision_ids_with_collision) > 0:
        collision_decision_id = decision_ids_with_collision.pop()
        connected_decision_ids = graph.connected(collision_decision_id)
        for decision_id in connected_decision_ids:
            decisions.pop(decision_id, None)
            decision_ids_with_collision.discard(decision_id)

    return decisions

def get_vector_decisions_graph(tree_data):
    '''
    Builds a directed graph.
    Vertices in that graph are decision IDs.
    A directed edge (A, B) means: If A is a list, then B has to be a list.
    '''
    from . graph import DirectedGraphBuilder
    builder = DirectedGraphBuilder()
    input_sockets = set()
    output_sockets = set()

    for node in tree_data.iter_nodes():
        for decl, sockets in node.decl_map.iter_decl_with_sockets():
            if isinstance(decl, VectorizedInputDecl):
                decision_id = DecisionID(node, decl.prop_name)
                builder.add_vertex(decision_id)
                input_sockets.add(sockets[0])
            elif isinstance(decl, VectorizedOutputDecl):
                output_sockets.add(sockets[0])

    for from_socket, to_socket in tree_data.iter_connections():
        from_node = tree_data.get_node(from_socket)
        to_node = tree_data.get_node(to_socket)

        from_decl = from_socket.get_decl(from_node)
        to_decl = to_socket.get_decl(to_node)

        if isinstance(from_decl, VectorizedOutputDecl) and isinstance(to_decl, VectorizedInputDecl):
            for prop_name in from_decl.input_prop_names:
                from_decision_id = DecisionID(from_node, prop_name)
                to_decision_id = DecisionID(to_node, to_decl.prop_name)
                builder.add_directed_edge(from_decision_id, to_decision_id)

    return builder.build(), input_sockets, output_sockets

def iter_obligatory_vector_decisions(graph, input_sockets, output_sockets, tree_data, list_decisions):
    for socket in input_sockets:
        other_node, other_socket = tree_data.try_get_origin_with_node(socket)
        if other_node is None:
            continue

        node = tree_data.get_node(socket)
        decl = socket.get_decl(node)
        decision_id = DecisionID(node, decl.prop_name)

        other_decl = other_socket.get_decl(other_node)
        if data_sockets_are_static(other_decl):
            other_data_type = other_socket.data_type
            if type_infos.is_list(other_data_type) and type_infos.is_link_allowed(other_data_type, decl.list_type):
                yield decision_id, "LIST"
        elif isinstance(other_decl, ListSocketDecl):
            if other_decl.list_or_base == "LIST":
                list_decision_id = DecisionID(other_node, other_decl.prop_name)
                if list_decision_id in list_decisions:
                    other_base_type = list_decisions[list_decision_id]
                    if type_infos.is_link_allowed(other_base_type, decl.base_type):
                        yield decision_id, "LIST"
                else:
                    old_data_type = other_socket.data_type
                    if type_infos.is_link_allowed(old_data_type, decl.list_type):
                        yield decision_id, "LIST"

    for socket in output_sockets:
        node = tree_data.get_node(socket)
        decl = socket.get_decl(node)
        decision_ids = [DecisionID(node, p) for p in decl.input_prop_names]

        for other_node, other_socket in tree_data.iter_connected_sockets_with_nodes(socket):
            other_decl = other_socket.get_decl(other_node)
            if data_sockets_are_static(other_decl):
                other_data_type = other_socket.data_type
                if type_infos.is_base(other_data_type) and type_infos.is_link_allowed(other_data_type, decl.base_type):
                    for decision_id in decision_ids:
                        yield decision_id, "BASE"
            elif isinstance(other_decl, ListSocketDecl):
                if other_decl.list_or_base == "BASE":
                    list_decision_id = DecisionID(other_node, other_decl.prop_name)
                    if list_decision_id in list_decisions:
                        other_base_type = list_decisions[list_decision_id]
                        if type_infos.is_link_allowed(decl.base_type, other_base_type):
                            for decision_id in decision_ids:
                                yield decision_id, "BASE"
                    else:
                        old_data_type = other_socket.data_type
                        if type_infos.is_link_allowed(decl.base_type, old_data_type):
                            for decision_id in decision_ids:
                                yield decision_id, "BASE"


# Inference pack list decisions
########################################

def make_base_list_variadic_decisions(tree_data, list_decisions, vector_decisions):
    decisions = dict()

    for decision_id, decl, socket in iter_base_list_variadic_sockets(tree_data):
        assert not socket.is_output

        origin_node, origin_socket = tree_data.try_get_origin_with_node(socket)
        if origin_socket is None:
            decisions[decision_id] = "BASE"
            continue

        origin_decl = origin_socket.get_decl(origin_node)
        if data_sockets_are_static(origin_decl):
            data_type = origin_socket.data_type
            if type_infos.is_link_allowed(data_type, decl.base_type):
                decisions[decision_id] = "BASE"
            elif type_infos.is_link_allowed(data_type, decl.list_type):
                decisions[decision_id] = "LIST"
            else:
                decisions[decision_id] = "BASE"
        elif isinstance(origin_decl, ListSocketDecl):
            list_decision_id = DecisionID(origin_node, origin_decl.prop_name)
            if list_decision_id in list_decisions:
                other_base_type = list_decisions[list_decision_id]
                if type_infos.is_link_allowed(other_base_type, decl.base_type):
                    decisions[decision_id] = origin_decl.list_or_base
                else:
                    decisions[decision_id] = "BASE"
            else:
                old_origin_type = origin_socket.data_type
                if type_infos.is_link_allowed(old_origin_type, decl.base_type):
                    decisions[decision_id] = "BASE"
                elif type_infos.is_link_allowed(old_origin_type, decl.list_type):
                    decisions[decision_id] = "LIST"
                else:
                    decisions[decision_id] = "BASE"
        elif isinstance(origin_decl, VectorizedOutputDecl):
            other_base_type = origin_decl.base_type
            if type_infos.is_link_allowed(other_base_type, decl.base_type):
                for input_prop_name in origin_decl.input_prop_names:
                    input_decision_id = DecisionID(origin_node, input_prop_name)
                    if input_decision_id in vector_decisions:
                        if vector_decisions[input_decision_id] == "LIST":
                            decisions[decision_id] = "LIST"
                            break
                else:
                    decisions[decision_id] = "BASE"
            else:
                decisions[decision_id] = "BASE"
        else:
            decisions[decision_id] = "BASE"

    return decisions

def data_sockets_are_static(decl):
    return isinstance(decl, (FixedSocketDecl, AnyVariadicDecl))

def iter_base_list_variadic_sockets(tree_data):
    for node in tree_data.iter_nodes():
        for decl, sockets in node.decl_map.iter_decl_with_sockets():
            if isinstance(decl, BaseListVariadic):
                collection = decl.get_collection()
                for i, socket in enumerate(sockets[:-1]):
                    decision_id = DecisionID(node, f"{decl.prop_name}[{i}].state")
                    yield decision_id, decl, socket
