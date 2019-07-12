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
  DependenciesBody *body = fn->body<DependenciesBody>();
  if (body) {
    ExternalDependenciesBuilder builder({});
    body->dependencies(builder);
    update_depsgraph(deps_node, builder.get_transform_dependencies());
  }
}
