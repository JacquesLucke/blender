#include "FN_expression_lexer.h"

namespace FN {
namespace Expr {

static bool is_digit(char c)
{
  return c >= '0' & c <= '9';
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

void tokenize(StringRef str, Vector<Token> &r_tokens)
{
  uint offset = 0;
  uint total_size = str.size();

  while (offset < total_size) {
    const char next_char = str[offset];
    Token token;
    token.start = offset;
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
        tokenize_number(str.drop_prefix(offset), token.size, token.type);
        break;
      }
      case '+': {
        token.size = 1;
        token.type = TokenType::Plus;
        break;
      }
      case '-': {
        token.size = 1;
        token.type = TokenType::Minus;
        break;
      }
      case '*': {
        token.size = 1;
        token.type = TokenType::Asterix;
        break;
      }
      case '/': {
        token.size = 1;
        token.type = TokenType::ForwardSlash;
        break;
      }
      case '(': {
        token.size = 1;
        token.type = TokenType::ParenOpen;
        break;
      }
      case ')': {
        token.size = 1;
        token.type = TokenType::ParenClose;
        break;
      }
      default: {
        if (is_identifier_start(next_char)) {
          tokenize_identifier(str.drop_prefix(offset), token.size, token.type);
        }
        else {
          BLI_assert(false);
        }
        break;
      }
    }

    r_tokens.append(token);
    offset += token.size;
  }
}

}  // namespace Expr
}  // namespace FN
