import functools

cached_items = set()

def cache_enum_items(items_cb):
    @functools.wraps(items_cb)
    def wrapper(self, context):
        items = items_cb(self, context)
        cached_items.update(items)
        return items
    return wrapper
