#pragma once

/**
 * The SourceInfo class is used to track debugging information through the various stages of
 * building an function. It is not directly part of a function, because the same function can be
 * used in very different contexts.
 */

#include <string>

namespace FN {

class SourceInfo {
 public:
  virtual ~SourceInfo()
  {
  }

  virtual std::string to_string() const = 0;
  virtual void handle_warning(StringRef msg) const;
};

} /* namespace FN */
