import bpy
from . base import FunctionNode, DataSocket
from . inferencer import Inferencer
from . socket_decl import FixedSocketDecl, ListSocketDecl, BaseSocketDecl
from collections import defaultdict
from . sockets import type_infos

class TestOperator(bpy.types.Operator):
    bl_idname = "fn.test_operator"
    bl_label = "FN Test"

    def execute(self, context):
        tree = context.space_data.node_tree
        run_socket_type_inferencer(tree)
        return {'FINISHED'}

def run_socket_type_inferencer(tree):
    inferencer = Inferencer(type_infos)

    for node in tree.nodes:
        insert_constraints__within_node(inferencer, node)

    for link in tree.links:
        insert_constraints__link(inferencer, link)

    inferencer.inference()

    rebuild_nodes_and_links_that_changed(tree, inferencer)

def rebuild_nodes_and_links_that_changed(tree, inferencer):
    links_by_node = get_link_ids_by_node(tree)

    nodes_to_rebuild = set()
    links_to_rebuild = set()

    for (node, prop_name), value in inferencer.get_decisions().items():
        if getattr(node, prop_name) != value:
            setattr(node, prop_name, value)
            nodes_to_rebuild.add(node)

    for node in nodes_to_rebuild:
        links_to_rebuild.update(links_by_node.get(node))
        node.rebuild_existing_sockets()

    for link_id in links_to_rebuild:
        from_socket = socket_by_id(link_id[0])
        to_socket = socket_by_id(link_id[1])
        tree.links.new(to_socket, from_socket)

def get_link_ids_by_node(tree):
    links_by_node = defaultdict(set)

    for link in tree.links:
        link_id = get_link_id(link)
        links_by_node[link.from_node].add(link_id)
        links_by_node[link.to_node].add(link_id)

    return links_by_node


# Insert inferencer constraints
########################################

def insert_constraints__within_node(inferencer, node):
    if isinstance(node, FunctionNode):
        insert_constraints__function_node(inferencer, node)
    else:
        insert_constraints__other_node(inferencer, node)

def insert_constraints__function_node(inferencer, node):
    inputs, outputs = node.get_sockets()

    list_ids_per_prop = defaultdict(set)
    base_ids_per_prop = defaultdict(set)

    for decl, socket_id in iter_sockets_decl_with_ids(node):
        if isinstance(decl, FixedSocketDecl):
            inferencer.insert_final_type(socket_id, decl.data_type)
        elif isinstance(decl, ListSocketDecl):
            list_ids_per_prop[decl.type_property].add(socket_id)
        elif isinstance(decl, BaseSocketDecl):
            base_ids_per_prop[decl.type_property].add(socket_id)

    properties = set()
    properties.update(list_ids_per_prop.keys())
    properties.update(base_ids_per_prop.keys())

    for prop in properties:
        inferencer.insert_list_constraint(
            list_ids_per_prop[prop],
            base_ids_per_prop[prop],
            (node, prop))

def insert_constraints__other_node(inferencer, node):
    for socket, socket_id in iter_sockets_with_ids(node):
        if isinstance(socket, DataSocket):
            inferencer.insert_final_type(socket_id, socket.data_type)

def insert_constraints__link(inferencer, link):
    if not isinstance(link.from_socket, DataSocket):
        return
    if not isinstance(link.from_socket, DataSocket):
        return

    from_id = get_socket_id_from_socket(link.from_node, link.from_socket)
    to_id = get_socket_id_from_socket(link.to_node, link.to_socket)

    inferencer.insert_equality_constraint((from_id, to_id))

def iter_sockets_with_ids(node):
    for i, socket in enumerate(node.inputs):
        yield socket, get_socket_id(node, socket.is_output, i)
    for i, socket in enumerate(node.outputs):
        yield socket, get_socket_id(node, socket.is_output, i)

def iter_sockets_decl_with_ids(node):
    inputs, outputs = node.get_sockets()
    for i, decl in enumerate(inputs):
        yield decl, get_socket_id(node, False, i)
    for i, decl in enumerate(outputs):
        yield decl, get_socket_id(node, True, i)


# Temporary IDs
########################################

def get_socket_id_from_socket(node, socket):
    index = get_socket_index(node, socket)
    return (node, socket.is_output, index)

def get_socket_id(node, is_output, i):
    return (node, is_output, i)

def get_socket_index(node, socket):
    if socket.is_output:
        return tuple(node.outputs).index(socket)
    else:
        return tuple(node.inputs).index(socket)

def get_link_id(link):
    from_id = get_socket_id_from_socket(link.from_node, link.from_socket)
    to_id = get_socket_id_from_socket(link.to_node, link.to_socket)
    return (from_id, to_id)

def socket_by_id(socket_id):
    node = socket_id[0]
    if socket_id[1]:
        return node.outputs[socket_id[2]]
    else:
        return node.inputs[socket_id[2]]