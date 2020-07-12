/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup fn
 */

#ifndef __FN_EXPRESSION_LEXER_HH__
#define __FN_EXPRESSION_LEXER_HH__

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

namespace blender::fn {

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

}  // namespace blender::fn

#endif /* __FN_EXPRESSION_LEXER_HH__ */
