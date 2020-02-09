#pragma once

#include "BLI_string_ref.h"
#include "BLI_vector.h"

namespace FN {
namespace Expr {

using BLI::StringRef;
using BLI::Vector;

namespace TokenType {
enum Enum : uchar {
  ParenOpen,
  ParenClose,
  IntLiteral,
  FloatLiteral,
  Plus,
  Minus,
  Asterix,
  ForwardSlash,
  Identifier,
  Less,
  Greater,
  Equal,
  LessOrEqual,
  GreaterOrEqual,
};
}

struct TokenRange {
  uint start;
  uint size;
};

void tokenize(StringRef str,
              Vector<TokenType::Enum> &r_token_types,
              Vector<TokenRange> &r_token_ranges);

}  // namespace Expr
}  // namespace FN
