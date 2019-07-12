#include "FN_dependencies.hpp"

using namespace FN;

void FN_function_update_dependencies(FnFunction UNUSED(fn_c),
                                     struct DepsNodeHandle *UNUSED(deps_node))
{
  // Function *fn = unwrap(fn_c);
  // const DependenciesBody *body = fn->body<DependenciesBody>();
  // if (body) {
  //   Dependencies dependencies;
  //   body->dependencies(dependencies);
  //   dependencies.update_depsgraph(deps_node);
  // }
}
