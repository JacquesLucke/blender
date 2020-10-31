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

#pragma once

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

namespace blender::fn::lang {

enum class TokenType : uint8_t {
  EndOfString = 0,
  IsLess,
  IsGreater,
  IsEqual,
  IsLessOrEqual,
  IsGreaterOrEqual,
  Plus,
  Minus,
  Asterix,
  ForwardSlash,
  ParenOpen,
  ParenClose,
  IntLiteral,
  FloatLiteral,
  DoubleAsterix,
  Identifier,
  StringLiteral,
  DoubleLess,
  DoubleRight,
  Comma,
  Dot,
  Equal,
  At,
  Semicolon,
  Colon,
  CurlyOpen,
  CurlyClose,
};

struct TokenRange {
  int start;
  int size;

  StringRef get(StringRef str) const
  {
    return str.substr(start, size);
  }
};

struct TokenizeResult {
  Vector<TokenType> types;
  Vector<TokenRange> ranges;
};

TokenizeResult tokenize(StringRef str);
StringRefNull token_type_to_string(TokenType token_type);

}  // namespace blender::fn::lang
