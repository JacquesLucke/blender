#include "FN_dependencies.hpp"
#include "DEG_depsgraph_build.h"
#include "intern/builder/deg_builder_relations.h"

using namespace FN;

static void update_depsgraph(DepsNodeHandle *deps_node, ArrayRef<Object *> transform_dependencies)
{
  for (struct Object *ob : transform_dependencies) {
    DEG_add_object_relation(deps_node, ob, DEG_OB_COMP_TRANSFORM, __func__);
  }
}

void FN_function_update_dependencies(FnFunction fn_c, struct DepsNodeHandle *deps_node)
{
  Function *fn = unwrap(fn_c);
  DepsBody *body = fn->body<DepsBody>();
  if (body) {
    SmallMultiMap<uint, ID *> input_ids;
    SmallMultiMap<uint, ID *> output_ids;
    SmallSetVector<Object *> transform_dependencies;

    FunctionDepsBuilder builder(input_ids, output_ids, transform_dependencies);
    body->build_deps(builder);
    update_depsgraph(deps_node, transform_dependencies);
  }
}
