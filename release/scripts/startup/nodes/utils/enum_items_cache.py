import functools
from collections import defaultdict

cached_item_tuples_by_hash = defaultdict(list)

def cache_enum_items(items_cb):

    @functools.wraps(items_cb)
    def wrapper(self, context):
        item_tuples = tuple(items_cb(self, context))
        item_tuples_hash = hash(item_tuples)

        for cached_item_tuple in cached_item_tuples_by_hash[item_tuples_hash]:
            if cached_item_tuple == item_tuples:
                return cached_item_tuple
        else:
            cached_item_tuples_by_hash[item_tuples_hash].append(item_tuples)
            return item_tuples

    return wrapper
