#ifndef __FN_MULTI_FUNCTION_DEPENDENCIES_H__
#define __FN_MULTI_FUNCTION_DEPENDENCIES_H__

#include "BLI_set.h"

#include "DNA_object_types.h"
#include "DNA_image_types.h"

#include "FN_node_tree.h"

namespace FN {

using BLI::Set;

inline Set<Object *> get_objects_used_by_sockets(const InlinedNodeTree &inlined_tree)
{
  Set<Object *> objects;
  for (const XSocket *xsocket : inlined_tree.all_sockets()) {
    if (xsocket->idname() == "fn_ObjectSocket") {
      Object *object = (Object *)RNA_pointer_get(xsocket->rna(), "value").data;
      if (object != nullptr) {
        objects.add(object);
      }
    }
  }
  for (const XGroupInput *group_input : inlined_tree.all_group_inputs()) {
    if (group_input->vsocket().idname() == "fn_ObjectSocket") {
      Object *object = (Object *)RNA_pointer_get(group_input->vsocket().rna(), "value").data;
      if (object != nullptr) {
        objects.add(object);
      }
    }
  }
  return objects;
}

inline Set<Image *> get_images_used_by_sockets(const InlinedNodeTree &inlined_tree)
{
  Set<Image *> images;
  for (const XSocket *xsocket : inlined_tree.all_sockets()) {
    if (xsocket->idname() == "fn_ImageSocket") {
      Image *image = (Image *)RNA_pointer_get(xsocket->rna(), "value").data;
      if (image != nullptr) {
        images.add(image);
      }
    }
  }
  for (const XGroupInput *group_input : inlined_tree.all_group_inputs()) {
    if (group_input->vsocket().idname() == "fn_ImageSocket") {
      Image *image = (Image *)RNA_pointer_get(group_input->vsocket().rna(), "value").data;
      if (image != nullptr) {
        images.add(image);
      }
    }
  }
  return images;
}

inline void add_ids_used_by_nodes(IDHandleLookup &id_handle_lookup,
                                  const InlinedNodeTree &inlined_tree)
{
  for (Object *object : get_objects_used_by_sockets(inlined_tree)) {
    id_handle_lookup.add(object->id);
  }
  for (Image *image : get_images_used_by_sockets(inlined_tree)) {
    id_handle_lookup.add(image->id);
  }
}

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_DEPENDENCIES_H__ */
