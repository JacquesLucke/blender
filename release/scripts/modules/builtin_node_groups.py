import os
import bpy
import json
from functools import lru_cache

# Export Builtin Node Group
#########################################################3

def get_socket_value(sock):
    if sock.type in {"VALUE", "INT", "BOOLEAN", "STRING"}:
        return sock.default_value
    elif sock.type in {"VECTOR", "RGBA"}:
        return tuple(sock.default_value)
    else:
        return None

def insert_socket_data(sock_data, sock):
    sock_data["identifier"] = sock.identifier
    sock_data["default_value"] = get_socket_value(sock)

def insert_group_socket_data(sock_data, sock):
    sock_data["name"] = sock.name
    sock_data["identifier"] = sock.identifier
    sock_data["default_value"] = get_socket_value(sock)
    sock_data["type"] = sock.type
    sock_data["bl_socket_idname"] = sock.bl_socket_idname

def insert_group_interface(json_data, group):
    for sock in group.inputs:
        sock_data = {}
        insert_group_socket_data(sock_data, sock)
        json_data["inputs"].append(sock_data)

    for sock in group.outputs:
        sock_data = {}
        insert_group_socket_data(sock_data, sock)
        json_data["outputs"].append(sock_data)

def insert_node_data(node_data, node):
    node_data["name"] = node.name
    node_data["bl_idname"] = node.bl_idname
    node_data["location"] = tuple(map(int, node.location))
    node_data["width"] = int(node.width)

    # Don't compute this only once in the beginning, because addons might register more properties.
    base_node_property_names = {prop.identifier for prop in bpy.types.Node.bl_rna.properties}

    if node.bl_idname in {"NodeGroupInput", "NodeGroupOutput"}:
        pass
    elif node.bl_idname == "SimulationNodeGroup":
        node_data["group_name"] = getattr(node.node_tree, "name", None)
    else:
        node_data["properties"] = {}
        for prop in node.bl_rna.properties:
            if prop.identifier not in base_node_property_names:
                node_data["properties"][prop.identifier] = getattr(node, prop.identifier)

        node_data["inputs"] = []
        node_data["outputs"] = []
        for sock in node.inputs:
            sock_data = {}
            insert_socket_data(sock_data, sock)
            node_data["inputs"].append(sock_data)
        for sock in node.outputs:
            sock_data = {}
            insert_socket_data(sock_data, sock)
            node_data["outputs"].append(sock_data)

def insert_nodes_data(json_data, group):
    for node in group.nodes:
        node_data = {}
        insert_node_data(node_data, node)
        json_data["nodes"].append(node_data)

def insert_links_data(json_data, group):
    for link in group.links:
        link_data = {}
        link_data["from"] = [link.from_node.name, link.from_socket.identifier, list(link.from_node.outputs).index(link.from_socket)]
        link_data["to"] = [link.to_node.name, link.to_socket.identifier, list(link.to_node.inputs).index(link.to_socket)]
        json_data["links"].append(link_data)

def find_direct_dependency_groups(group):
    dependencies = set()
    for node in group.nodes:
        if node.bl_idname == "SimulationNodeGroup":
            if node.node_tree is not None:
                dependencies.add(node.node_tree)
    return dependencies

def insert_dependencies(json_data, group):
    dependency_names = [dependency.name for dependency in find_direct_dependency_groups(group)]
    json_data["dependencies"] = dependency_names

def node_group_to_json_data(group):
    json_data = {}
    json_data["bpy.app.version"] = bpy.app.version
    json_data["bl_idname"] = group.bl_idname
    json_data["name"] = group.name
    json_data["inputs"] = []
    json_data["outputs"] = []
    json_data["nodes"] = []
    json_data["links"] = []

    insert_group_interface(json_data, group)
    insert_nodes_data(json_data, group)
    insert_links_data(json_data, group)
    insert_dependencies(json_data, group)

    return json_data

def save_group_as_json(group, file_path):
    json_data = node_group_to_json_data(group)
    json_str = json.dumps(json_data, indent=1)
    os.makedirs(os.path.dirname(file_path), exist_ok=True)
    with open(file_path, "w") as f:
        f.write(json_str)



# Import Builtin Node Group
#########################################################3

builtin_node_group_directory = os.path.join(bpy.utils.resource_path("LOCAL"), "datafiles", "node_groups")

def group_name_to_file_name(group_name):
    return group_name.replace(" ", "_") + ".json"

def group_name_to_file_path(group_name):
    file_name = group_name_to_file_name(group_name)
    return os.path.join(builtin_node_group_directory, file_name)

def property_has_given_value(owner, prop_name, value):
    prop = owner.bl_rna.properties[prop_name]
    if prop.type in {'BOOLEAN', 'INT', 'STRING', 'ENUM', 'POINTER'}:
        return getattr(owner, prop_name) == value
    elif prop.type == 'FLOAT':
        if prop.array_length <= 1:
            return getattr(owner, prop_name) == value
        else:
            return tuple(getattr(owner, prop_name)) == tuple(value)
    else:
        assert False

def group_matches_json_data(group, json_data, loaded_group_by_name):
    groups_json_data = get_builtin_groups_data()

    if len(group.nodes) != len(json_data["nodes"]):
        return False
    if len(group.links) != len(json_data["links"]):
        return False
    if len(group.inputs) != len(json_data["inputs"]):
        return False
    if len(group.outputs) != len(json_data["outputs"]):
        return False

    for node_data in json_data["nodes"]:
        node = group.nodes.get(node_data["name"])
        if node is None:
            return False
        if node.bl_idname != node_data["bl_idname"]:
            return False

        if node.bl_idname in {"NodeGroupInput", "NodeGroupOutput"}:
            pass
        elif node.bl_idname == "SimulationNodeGroup":
            if node.node_tree is None:
                return False
            elif node.node_tree == loaded_group_by_name[node_data["group_name"]]:
                pass
            elif not group_matches_json_data(node.node_tree,
                                             groups_json_data[node_data["group_name"]],
                                             loaded_group_by_name):
                return False

        else:
            for key, value in node_data["properties"].items():
                if getattr(node, key) != value:
                    return False
            if len(node.inputs) != len(node_data["inputs"]):
                return False
            if len(node.outputs) != len(node_data["outputs"]):
                return False
            for sock_data, sock in zip(node_data["inputs"], node.inputs):
                if hasattr(sock, "default_value"):
                    if not property_has_given_value(sock, "default_value", sock_data["default_value"]):
                        return False

    for sock_data, sock in zip(json_data["inputs"], group.inputs):
        if sock_data["bl_socket_idname"] != sock.bl_socket_idname:
            return False
        if sock_data["name"] != sock.name:
            return False

    for sock_data, sock in zip(json_data["outputs"], group.outputs):
        if sock_data["bl_socket_idname"] != sock.bl_socket_idname:
            return False
        if sock_data["name"] != sock.name:
            return False

    link_cache = set()
    for link in group.links:
        link_cache.add((
            link.from_node.name, list(link.from_node.outputs).index(link.from_socket),
            link.to_node.name, list(link.to_node.inputs).index(link.to_socket)))
    for link_data in json_data["links"]:
        if (link_data["from"][0], link_data["from"][2], link_data["to"][0], link_data["to"][2]) not in link_cache:
            return False

    return True

def get_or_create_node_group(json_data, loaded_group_by_name):
    for potential_group in bpy.data.node_groups:
        if group_matches_json_data(potential_group, json_data, loaded_group_by_name):
            return potential_group

    group = bpy.data.node_groups.new(json_data["name"], "SimulationNodeTree")

    for node_data in json_data["nodes"]:
        node = group.nodes.new(node_data["bl_idname"])
        node.name = node_data["name"]
        node.location = node_data["location"]
        node.select = False
        node.width = node_data["width"]

        if node.bl_idname in {"NodeGroupInput", "NodeGroupOutput"}:
            pass
        elif node.bl_idname == "SimulationNodeGroup":
            node.node_tree = loaded_group_by_name[node_data["group_name"]]
        else:
            for key, value in node_data["properties"].items():
                setattr(node, key, value)

            for sock_data, sock in zip(node_data["inputs"], node.inputs):
                if hasattr(sock, "default_value"):
                    sock.default_value = sock_data["default_value"]

    for sock_data in json_data["inputs"]:
        sock = group.inputs.new(sock_data["bl_socket_idname"], sock_data["name"])
        if hasattr(sock, "default_value"):
            sock.default_value = sock_data["default_value"]

    for sock_data in json_data["outputs"]:
        group.outputs.new(sock_data["bl_socket_idname"], sock_data["name"])

    for link_data in json_data["links"]:
        from_socket = group.nodes[link_data["from"][0]].outputs[link_data["from"][2]]
        to_socket = group.nodes[link_data["to"][0]].inputs[link_data["to"][2]]
        group.links.new(from_socket, to_socket)

    return group

def export_builtin_node_group(group):
    file_path = group_name_to_file_path(group.name)
    save_group_as_json(group, file_path)

@lru_cache()
def get_builtin_groups_data():
    json_data_by_group_name = dict()

    if not os.path.exists(builtin_node_group_directory):
        return json_data_by_group_name

    for file_name in os.listdir(builtin_node_group_directory):
        if not file_name.endswith(".json"):
            continue
        file_path = os.path.join(builtin_node_group_directory, file_name)
        with open(file_path, "r") as f:
            json_str = f.read()
        json_data = json.loads(json_str)
        group_name = json_data["name"]
        json_data_by_group_name[group_name] = json_data

    return json_data_by_group_name

def get_sorted_group_names_to_load(main_group_name):
    groups_json_data = get_builtin_groups_data()

    sorted_group_names = []

    def add_group(group_name):
        if group_name in sorted_group_names:
            return
        json_data = groups_json_data[group_name]
        for dependency_name in json_data["dependencies"]:
            add_group(dependency_name)
        sorted_group_names.append(group_name)

    add_group(main_group_name)
    return sorted_group_names

def get_builtin_group_items_cb(node_tree_idname):
    def items_generator(self, context):
        items = []
        for i, name in enumerate(get_builtin_groups_data().keys()):
            items.append((str(i), name, ""))
        return items
    return items_generator

def import_builtin_node_group_with_dependencies(group_name):
    sorted_group_names = get_sorted_group_names_to_load(group_name)
    groups_json_data = get_builtin_groups_data()
    loaded_group_by_name = dict()

    for group_name in sorted_group_names:
        json_data = groups_json_data[group_name]
        group = get_or_create_node_group(json_data, loaded_group_by_name)
        loaded_group_by_name[group_name] = group

    return loaded_group_by_name[group_name]

def import_builtin_node_group_by_item_identifier(item_identifier):
    index = int(item_identifier)
    group_name = list(get_builtin_groups_data().keys())[index]
    return import_builtin_node_group_with_dependencies(group_name)
