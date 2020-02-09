#include "FN_expression_lexer.h"

namespace FN {
namespace Expr {

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

static void tokenize_number(StringRef str, uint &r_token_size, TokenType::Enum &r_token_type)
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

static void tokenize_identifier(StringRef str, uint &r_token_size, TokenType::Enum &r_token_type)
{
  BLI_assert(is_identifier_start(str[0]));
  r_token_size = count_while(str, is_identifier_letter);
  r_token_type = TokenType::Identifier;
}

void tokenize(StringRef str,
              Vector<TokenType::Enum> &r_token_types,
              Vector<TokenRange> &r_token_ranges)
{
  uint offset = 0;
  uint total_size = str.size();

  while (offset < total_size) {
    const char next_char = str[offset];

    uint token_size;
    TokenType::Enum token_type;
    switch (next_char) {
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
        token_size = 1;
        token_type = TokenType::Asterix;
        break;
      }
      case '/': {
        token_size = 1;
        token_type = TokenType::ForwardSlash;
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
          if (str[offset + 1] == '=') {
            token_size = 2;
            token_type = TokenType::LessOrEqual;
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
          if (str[offset + 1] == '=') {
            token_size = 2;
            token_type = TokenType::GreaterOrEqual;
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
      default: {
        if (is_identifier_start(next_char)) {
          tokenize_identifier(str.drop_prefix(offset), token_size, token_type);
        }
        else {
          BLI_assert(false);
        }
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

}  // namespace Expr
}  // namespace FN
