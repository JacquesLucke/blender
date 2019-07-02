from collections import defaultdict
from . base import BaseNode

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
