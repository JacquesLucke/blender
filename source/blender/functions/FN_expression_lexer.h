#pragma once

#include "BLI_string_ref.h"
#include "BLI_vector.h"

namespace FN {
namespace Expr {

using BLI::StringRef;
using BLI::Vector;

namespace TokenType {
enum Enum {
  ParenOpen,
  ParenClose,
  IntLiteral,
  FloatLiteral,
  Plus,
  Minus,
  Asterix,
  ForwardSlash,
  Identifier,
};
}

struct Token {
  TokenType::Enum type;
  StringRef str;
};

void tokenize(StringRef str, Vector<Token> &r_tokens);

}  // namespace Expr
}  // namespace FN
