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
  ParenOpen,
  ParenClose,
  IntLiteral,
  FloatLiteral,
  Plus,
  Minus,
  Asterix,
  DoubleAsterix,
  ForwardSlash,
  Identifier,
  Less,
  Greater,
  Equal,
  LessOrEqual,
  GreaterOrEqual,
  String,
  DoubleLess,
  DoubleRight,
};

struct TokenRange {
  uint start;
  uint size;
};

void tokenize(StringRef str, Vector<TokenType> &r_token_types, Vector<TokenRange> &r_token_ranges);
StringRefNull token_type_to_string(TokenType token_type);

}  // namespace Expr
}  // namespace FN
