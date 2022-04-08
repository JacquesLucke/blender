/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_contextual_logger.hh"

namespace blender::contextual_logger {

IndexedContextualLogger::IndexedContextualLogger(ContextualLogger &logger)
{
  for (LocalContextualLogger &local_logger : logger.local_loggers_) {
    for (destruct_ptr<StoredContextBase> &context_ptr : local_logger.contexts_) {
      StoredContextBase &context = *context_ptr;
      if (context.parent_) {
        children_by_context_.add(context.parent_, &context);
      }
      else {
        root_contexts_.append(&context);
      }
    }
  }
}

}  // namespace blender::contextual_logger
