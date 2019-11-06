import bpy
from pprint import pprint
from . base import BaseNode
from . tree_data import TreeData
from . graph import DirectedGraphBuilder
from contextlib import contextmanager

_is_syncing = False

def sync_trees_and_dependent_trees(trees):
    global _is_syncing
    if _is_syncing:
        return
    if _skip_syncing:
        return

    _is_syncing = True

    try:
        for tree in iter_trees_to_sync_in_order(trees):
            sync_tree(tree)
    finally:
        _is_syncing = False

def sync_tree(tree):
    rebuild_currently_outdated_nodes(tree)

    tree_data = TreeData(tree)

    tree_changed = run_socket_operators(tree_data)
    if tree_changed: tree_data = TreeData(tree)

    tree_changed = do_inferencing_and_update_nodes(tree_data)
    if tree_changed: tree_data = TreeData(tree)

    tree_changed = remove_invalid_links(tree_data)

# Sync skipping
######################################

_skip_syncing = False

@contextmanager
def skip_syncing():
    global _skip_syncing
    last_state = _skip_syncing
    _skip_syncing = True

    try:
        yield
    finally:
        _skip_syncing = last_state


# Tree sync ordering
############################################

def iter_trees_to_sync_in_order(trees):
    stored_tree_ids = {id(tree) for tree in bpy.data.node_groups}
    if any(id(tree) not in stored_tree_ids for tree in trees):
        # can happen after undo or on load
        return

    dependency_graph = build_tree_dependency_graph()
    all_trees_to_sync = dependency_graph.reachable(trees)
    trees_in_sync_order = dependency_graph.toposort_partial(all_trees_to_sync)
    yield from trees_in_sync_order

def build_tree_dependency_graph():
    '''
    Builds a directed graph in which every tree is a vertex.
    Every edge (A, B) means: Changes in A might affect B.
    '''
    from . function_tree import FunctionTree

    builder = DirectedGraphBuilder()
    for tree in bpy.data.node_groups:
        if isinstance(tree, FunctionTree):
            builder.add_vertex(tree)
            for dependency_tree in tree.iter_dependency_trees():
                builder.add_directed_edge(
                    from_v=dependency_tree,
                    to_v=tree)
    return builder.build()


# Rebuild already outdated nodes
############################################

def rebuild_currently_outdated_nodes(tree):
    outdated_nodes = list(iter_nodes_with_outdated_sockets(tree))
    rebuild_nodes_and_try_keep_state(outdated_nodes)

def iter_nodes_with_outdated_sockets(tree):
    for node in tree.nodes:
        if isinstance(node, BaseNode):
            if not node_matches_current_declaration(node):
                yield node

def node_matches_current_declaration(node):
    from . node_builder import NodeBuilder
    builder = node.get_node_builder()
    return builder.matches_sockets()


# Socket Operators
############################################

def run_socket_operators(tree_data):
    from . sockets import OperatorSocket

    tree_changed = False
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
            tree_changed = True
        else:
            return tree_changed


# Inferencing
####################################

def do_inferencing_and_update_nodes(tree_data):
    from . inferencing import get_inferencing_decisions

    decisions = get_inferencing_decisions(tree_data)

    nodes_to_rebuild = set()

    for decision_id, base_type in decisions.items():
        if getattr(decision_id.group, decision_id.prop_name) != base_type:
            setattr(decision_id.group, decision_id.prop_name, base_type)
            nodes_to_rebuild.add(decision_id.node)

    rebuild_nodes_and_try_keep_state(nodes_to_rebuild)

    tree_changed = len(nodes_to_rebuild) > 0
    return tree_changed


# Remove Invalid Links
####################################

def remove_invalid_links(tree_data):
    links_to_remove = set()
    for from_socket, to_socket in tree_data.iter_connections():
        if not is_link_valid(tree_data, from_socket, to_socket):
            links_to_remove.update(tree_data.iter_incident_links(to_socket))

    tree_changed = len(links_to_remove) > 0

    tree = tree_data.tree
    for link in links_to_remove:
        tree.links.remove(link)

    return tree_changed

def is_link_valid(tree_data, from_socket, to_socket):
    from . types import type_infos
    from . base import DataSocket

    is_data_src = isinstance(from_socket, DataSocket)
    is_data_dst = isinstance(to_socket, DataSocket)

    if is_data_src != is_data_dst:
        return False

    if is_data_src and is_data_dst:
        from_type = from_socket.data_type
        to_type = to_socket.data_type
        return type_infos.is_link_allowed(from_type, to_type)

    return True


# Utils
######################################

def rebuild_nodes_and_try_keep_state(nodes):
    for node in nodes:
        node.rebuild()
