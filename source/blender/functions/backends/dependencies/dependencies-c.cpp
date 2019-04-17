#include "FN_dependencies.hpp"

using namespace FN;

void FN_function_update_dependencies(FnFunction fn, struct DepsNodeHandle *deps_node)
{
  Function *fn_ = unwrap(fn);
  const DependenciesBody *body = fn_->body<DependenciesBody>();
  if (body) {
    Dependencies dependencies;
    body->dependencies(dependencies);
    dependencies.update_depsgraph(deps_node);
  }
}
