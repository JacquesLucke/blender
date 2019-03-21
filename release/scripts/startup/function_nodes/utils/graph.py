def iter_connected_components(nodes: set, links: dict):
    nodes = set(nodes)
    while len(nodes) > 0:
        start_node = next(iter(nodes))
        component = depth_first_search(start_node, links)
        yield component
        nodes -= component

def depth_first_search(start_node, links):
    result = set()
    found = set()
    found.add(start_node)
    while len(found) > 0:
        node = found.pop()
        result.add(node)
        for linked_node in links[node]:
            if linked_node not in result:
                found.add(linked_node)
    return result