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

#include "FN_lang_tokenize.hh"

namespace blender::fn::lang {

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

template<typename FuncT> static int count_while(StringRef str, const FuncT &func)
{
  int count = 0;
  for (char c : str) {
    if (func(c)) {
      count++;
    }
    else {
      break;
    }
  }
  return count;
}

static void tokenize_number(StringRef str, int &r_token_size, TokenType &r_token_type)
{
  BLI_assert(is_digit(str[0]));
  int size = count_while(str, is_digit);
  if (size == str.size() || str[size] != '.') {
    r_token_size = size;
    r_token_type = TokenType::IntLiteral;
    return;
  }

  int decimals_size = count_while(str.drop_prefix(size + 1), is_digit);
  r_token_size = size + 1 + decimals_size;
  r_token_type = TokenType::FloatLiteral;
}

static void tokenize_identifier(StringRef str, int &r_token_size, TokenType &r_token_type)
{
  BLI_assert(is_identifier_start(str[0]));
  r_token_size = count_while(str, is_identifier_letter);
  r_token_type = TokenType::Identifier;
}

TokenizeResult tokenize(StringRef str)
{
  TokenizeResult result;

  int offset = 0;
  int total_size = str.size();

  while (offset < total_size) {
    const char current_char = str[offset];

    int token_size;
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
            break;
          }
        }
        token_size = 1;
        token_type = TokenType::Asterix;
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
      case '@': {
        token_size = 1;
        token_type = TokenType::At;
        break;
      }
      case ';': {
        token_size = 1;
        token_type = TokenType::Semicolon;
        break;
      }
      case ':': {
        token_size = 1;
        token_type = TokenType::Colon;
        break;
      }
      case '{': {
        token_size = 1;
        token_type = TokenType::CurlyOpen;
        break;
      }
      case '}': {
        token_size = 1;
        token_type = TokenType::CurlyClose;
        break;
      }
      case '=': {
        if (offset + 1 < total_size) {
          char next_char = str[offset + 1];
          if (next_char == '=') {
            token_size = 2;
            token_type = TokenType::IsEqual;
            break;
          }
        }
        token_size = 1;
        token_type = TokenType::Equal;
        break;
      }
      case '<': {
        if (offset + 1 < total_size) {
          char next_char = str[offset + 1];
          if (next_char == '=') {
            token_size = 2;
            token_type = TokenType::IsLessOrEqual;
            break;
          }
          if (next_char == '<') {
            token_size = 2;
            token_type = TokenType::DoubleLess;
            break;
          }
        }
        token_size = 1;
        token_type = TokenType::IsLess;
        break;
      }
      case '>': {
        if (offset + 1 < total_size) {
          char next_char = str[offset + 1];
          if (next_char == '=') {
            token_size = 2;
            token_type = TokenType::IsGreaterOrEqual;
            break;
          }
          if (next_char == '>') {
            token_size = 2;
            token_type = TokenType::DoubleRight;
            break;
          }
        }
        token_size = 1;
        token_type = TokenType::IsGreater;

        break;
      }
      case '"': {
        bool is_escaped = false;
        token_type = TokenType::StringLiteral;
        token_size = 2 + count_while(str.drop_prefix(offset + 1), [&](char c) {
                       if (is_escaped) {
                         is_escaped = false;
                         return true;
                       }
                       if (c == '\\') {
                         is_escaped = true;
                         return true;
                       }
                       bool is_end = c == '"';
                       return !is_end;
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
        throw std::runtime_error("unexpected character: " + current_char);
      }
    }

    TokenRange range;
    range.start = offset;
    range.size = token_size;

    result.types.append(token_type);
    result.ranges.append(range);

    offset += token_size;
  }

  return result;
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
    case TokenType::IsLess:
      return "IsLess";
    case TokenType::IsGreater:
      return "IsGreater";
    case TokenType::IsEqual:
      return "IsEqual";
    case TokenType::IsLessOrEqual:
      return "IsLessOrEqual";
    case TokenType::IsGreaterOrEqual:
      return "IsGreaterOrEqual";
    case TokenType::StringLiteral:
      return "StringLiteral";
    case TokenType::DoubleLess:
      return "DoubleLess";
    case TokenType::DoubleRight:
      return "DoubleRight";
    case TokenType::Dot:
      return "Dot";
    case TokenType::Equal:
      return "Equal";
    case TokenType::At:
      return "At";
    case TokenType::Semicolon:
      return "Semicolon";
    case TokenType::Colon:
      return "Colon";
    case TokenType::CurlyOpen:
      return "CurlyOpen";
    case TokenType::CurlyClose:
      return "CurlyClose";
  }
  BLI_assert(false);
  return "";
}

}  // namespace blender::fn::lang
