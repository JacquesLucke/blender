import bpy
from . base import FunctionNode, DataSocket
from . inferencer import Inferencer
from collections import defaultdict
from . sockets import type_infos, OperatorSocket, DataSocket

from . socket_decl import (
    FixedSocketDecl,
    ListSocketDecl,
    BaseSocketDecl,
    AnyVariadicDecl,
    AnyOfDecl,
)

class UpdateFunctionTreeOperator(bpy.types.Operator):
    bl_idname = "fn.update_function_tree"
    bl_label = "Update Function Tree"
    bl_description = "Execute socket operators and run inferencer"

    def execute(self, context):
        tree = context.space_data.node_tree
        run_socket_operators(tree)
        run_socket_type_inferencer(tree)
        return {'FINISHED'}


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

def run_socket_type_inferencer(tree):
    inferencer = Inferencer(type_infos)

    for node in tree.nodes:
        insert_constraints__within_node(inferencer, node)

    for link in tree.links:
        insert_constraints__link(inferencer, link)

    inferencer.inference()

    nodes_to_rebuild = set()

    for (node, prop_name), value in inferencer.get_decisions().items():
        if getattr(node, prop_name) != value:
            setattr(node, prop_name, value)
            nodes_to_rebuild.add(node)

    for node in nodes_to_rebuild:
        node.rebuild_and_try_keep_state()


# Insert inferencer constraints
########################################

def insert_constraints__within_node(inferencer, node):
    storage = node.storage

    list_ids_per_prop = defaultdict(set)
    base_ids_per_prop = defaultdict(set)

    for decl, sockets in storage.sockets_per_decl.items():
        if isinstance(decl, FixedSocketDecl):
            inferencer.insert_final_type(sockets[0].to_id(node), decl.data_type)
        elif isinstance(decl, ListSocketDecl):
            list_ids_per_prop[decl.type_property].add(sockets[0].to_id(node))
        elif isinstance(decl, BaseSocketDecl):
            base_ids_per_prop[decl.type_property].add(sockets[0].to_id(node))
        elif isinstance(decl, AnyVariadicDecl):
            for socket in sockets[:-1]:
                inferencer.insert_final_type(socket.to_id(node), socket.data_type)
        elif isinstance(decl, AnyOfDecl):
            inferencer.insert_union_constraint(
                [sockets[0].to_id(node)],
                decl.allowed_types,
                (node, decl.prop_name))

    properties = set()
    properties.update(list_ids_per_prop.keys())
    properties.update(base_ids_per_prop.keys())

    for prop in properties:
        inferencer.insert_list_constraint(
            list_ids_per_prop[prop],
            base_ids_per_prop[prop],
            (node, prop))

def insert_constraints__link(inferencer, link):
    if not isinstance(link.from_socket, DataSocket):
        return
    if not isinstance(link.from_socket, DataSocket):
        return

    from_id = link.from_socket.to_id(link.from_node)
    to_id = link.to_socket.to_id(link.to_node)

    inferencer.insert_equality_constraint((from_id, to_id))

