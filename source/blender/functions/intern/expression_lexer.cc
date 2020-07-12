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

#include "FN_expression_lexer.hh"

namespace blender::fn {

static bool is_digit(char c)
{
  return c >= '0' && c <= '9';
}

static bool is_identifier_start(char c)
{
  return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_identifier_letter(char c)
{
  return is_identifier_start(c) || is_digit(c);
}

template<typename FuncT> static uint count_while(StringRef str, const FuncT &func)
{
  uint count = 0;
  for (uint c : str) {
    if (func(c)) {
      count++;
    }
    else {
      break;
    }
  }
  return count;
}

static void tokenize_number(StringRef str, uint &r_token_size, TokenType &r_token_type)
{
  BLI_assert(is_digit(str[0]));
  uint size = count_while(str, is_digit);
  if (size == str.size() || str[size] != '.') {
    r_token_size = size;
    r_token_type = TokenType::IntLiteral;
    return;
  }

  uint decimals_size = count_while(str.drop_prefix(size + 1), is_digit);
  r_token_size = size + 1 + decimals_size;
  r_token_type = TokenType::FloatLiteral;
}

static void tokenize_identifier(StringRef str, uint &r_token_size, TokenType &r_token_type)
{
  BLI_assert(is_identifier_start(str[0]));
  r_token_size = count_while(str, is_identifier_letter);
  r_token_type = TokenType::Identifier;
}

void tokenize(StringRef str, Vector<TokenType> &r_token_types, Vector<TokenRange> &r_token_ranges)
{
  uint offset = 0;
  uint total_size = str.size();

  while (offset < total_size) {
    const char current_char = str[offset];

    uint token_size;
    TokenType token_type;
    switch (current_char) {
      case ' ':
      case '\t':
      case '\n':
      case '\r': {
        offset++;
        continue;
      }
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9': {
        tokenize_number(str.drop_prefix(offset), token_size, token_type);
        break;
      }
      case '+': {
        token_size = 1;
        token_type = TokenType::Plus;
        break;
      }
      case '-': {
        token_size = 1;
        token_type = TokenType::Minus;
        break;
      }
      case '*': {
        if (offset + 1 < total_size) {
          char next_char = str[offset + 1];
          if (next_char == '*') {
            token_size = 2;
            token_type = TokenType::DoubleAsterix;
          }
          else {
            token_size = 1;
            token_type = TokenType::Asterix;
          }
        }
        else {
          token_size = 1;
          token_type = TokenType::Asterix;
        }
        break;
      }
      case '/': {
        token_size = 1;
        token_type = TokenType::ForwardSlash;
        break;
      }
      case ',': {
        token_size = 1;
        token_type = TokenType::Comma;
        break;
      }
      case '.': {
        token_size = 1;
        token_type = TokenType::Dot;
        break;
      }
      case '(': {
        token_size = 1;
        token_type = TokenType::ParenOpen;
        break;
      }
      case ')': {
        token_size = 1;
        token_type = TokenType::ParenClose;
        break;
      }
      case '=': {
        BLI_assert(str[offset + 1] == '=');
        token_size = 2;
        token_type = TokenType::Equal;
        break;
      }
      case '<': {
        if (offset + 1 < total_size) {
          char next_char = str[offset + 1];
          if (str[next_char] == '=') {
            token_size = 2;
            token_type = TokenType::LessOrEqual;
          }
          else if (next_char == '<') {
            token_size = 2;
            token_type = TokenType::DoubleLess;
          }
          else {
            token_size = 1;
            token_type = TokenType::Less;
          }
        }
        else {
          token_size = 1;
          token_type = TokenType::Less;
        }
        break;
      }
      case '>': {
        if (offset + 1 < total_size) {
          char next_char = str[offset + 1];
          if (next_char == '=') {
            token_size = 2;
            token_type = TokenType::GreaterOrEqual;
          }
          else if (next_char == '>') {
            token_size = 2;
            token_type = TokenType::DoubleRight;
          }
          else {
            token_size = 1;
            token_type = TokenType::Greater;
          }
        }
        else {
          token_size = 1;
          token_type = TokenType::Greater;
        }
        break;
      }
      case '"': {
        bool is_escaped = false;
        token_type = TokenType::String;
        token_size = 2 + count_while(str.drop_prefix(offset + 1), [&](char c) {
                       if (is_escaped) {
                         is_escaped = false;
                         return true;
                       }
                       else if (c == '\\') {
                         is_escaped = true;
                         return true;
                       }
                       else {
                         bool is_end = c == '"';
                         return !is_end;
                       }
                     });
        break;
      }
      case '_':
      case 'a':
      case 'b':
      case 'c':
      case 'd':
      case 'e':
      case 'f':
      case 'g':
      case 'h':
      case 'i':
      case 'j':
      case 'k':
      case 'l':
      case 'm':
      case 'n':
      case 'o':
      case 'p':
      case 'q':
      case 'r':
      case 's':
      case 't':
      case 'u':
      case 'v':
      case 'w':
      case 'x':
      case 'y':
      case 'z':
      case 'A':
      case 'B':
      case 'C':
      case 'D':
      case 'E':
      case 'F':
      case 'G':
      case 'H':
      case 'I':
      case 'J':
      case 'K':
      case 'L':
      case 'M':
      case 'N':
      case 'O':
      case 'P':
      case 'Q':
      case 'R':
      case 'S':
      case 'T':
      case 'U':
      case 'V':
      case 'W':
      case 'X':
      case 'Y':
      case 'Z': {
        tokenize_identifier(str.drop_prefix(offset), token_size, token_type);
        break;
      }
      default: {
        BLI_assert(false);
        token_type = TokenType::EndOfString;
        token_size = 1;
        break;
      }
    }

    TokenRange range;
    range.start = offset;
    range.size = token_size;

    r_token_types.append(token_type);
    r_token_ranges.append(range);

    offset += token_size;
  }
}

StringRefNull token_type_to_string(TokenType token_type)
{
  switch (token_type) {
    case TokenType::EndOfString:
      return "EndOfString";
    case TokenType::ParenOpen:
      return "ParenOpen";
    case TokenType::ParenClose:
      return "ParenClose";
    case TokenType::IntLiteral:
      return "IntLiteral";
    case TokenType::FloatLiteral:
      return "FloatLiteral";
    case TokenType::Plus:
      return "Plus";
    case TokenType::Minus:
      return "Minus";
    case TokenType::Asterix:
      return "Asterix";
    case TokenType::DoubleAsterix:
      return "DoubleAsterix";
    case TokenType::ForwardSlash:
      return "ForwardSlash";
    case TokenType::Comma:
      return "Comma";
    case TokenType::Identifier:
      return "Identifier";
    case TokenType::Less:
      return "Less";
    case TokenType::Greater:
      return "Greater";
    case TokenType::Equal:
      return "Equal";
    case TokenType::LessOrEqual:
      return "LessOrEqual";
    case TokenType::GreaterOrEqual:
      return "GreaterOrEqual";
    case TokenType::String:
      return "String";
    case TokenType::DoubleLess:
      return "DoubleLess";
    case TokenType::DoubleRight:
      return "DoubleRight";
    case TokenType::Dot:
      return "Dot";
  }
  BLI_assert(false);
  return "";
}

}  // namespace blender::fn
