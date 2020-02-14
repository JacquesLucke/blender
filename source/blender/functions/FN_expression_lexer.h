#pragma once

#include "BLI_string_ref.h"
#include "BLI_vector.h"

namespace FN {
namespace Expr {

using BLI::StringRef;
using BLI::StringRefNull;
using BLI::Vector;

enum class TokenType : uchar {
  EndOfString = 0,

  Less = 1,
  Greater = 2,
  Equal = 3,
  LessOrEqual = 4,
  GreaterOrEqual = 5,

  Plus = 6,
  Minus = 7,
  Asterix = 8,
  ForwardSlash = 9,

  ParenOpen,
  ParenClose,
  IntLiteral,
  FloatLiteral,
  DoubleAsterix,
  Identifier,
  String,
  DoubleLess,
  DoubleRight,
  Comma,
  Dot,
};

struct TokenRange {
  uint start;
  uint size;
};

void tokenize(StringRef str, Vector<TokenType> &r_token_types, Vector<TokenRange> &r_token_ranges);
StringRefNull token_type_to_string(TokenType token_type);

}  // namespace Expr
}  // namespace FN
