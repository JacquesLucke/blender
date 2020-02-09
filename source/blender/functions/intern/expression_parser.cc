#include "FN_expression_parser.h"

namespace FN {
namespace Expr {

class TokenStream {
 private:
  ArrayRef<Token> m_tokens;
  uint m_current = 0;

 public:
  TokenStream(ArrayRef<Token> tokens) : m_tokens(tokens)
  {
  }

  bool next_is(TokenType::Enum token_type) const
  {
    return !end_reached() && m_tokens[m_current].type == token_type;
  }

  Token consume(TokenType::Enum token_type)
  {
    BLI_assert(this->next_is(token_type));
    return this->consume();
  }

  Token consume()
  {
    BLI_assert(!this->end_reached());
    Token token = m_tokens[m_current];
    m_current++;
    return token;
  }

  bool end_reached() const
  {
    return m_current == m_tokens.size();
  }
};

static ASTNode *parse_expression(TokenStream &tokens, MonotonicAllocator<> &allocator);

static ASTNode *parse_expression__atom_level(TokenStream &tokens, MonotonicAllocator<> &allocator)
{
  Token token = tokens.consume();
  switch (token.type) {
    case TokenType::Identifier:
      return allocator.construct<IdentifierNode>(token.str);
    case TokenType::IntLiteral: {
      std::string str = token.str;
      void *value = allocator.construct<int>(std::atoi(str.c_str()));
      return allocator.construct<ConstantNode>(value, CPP_TYPE<int>());
    }
    case TokenType::FloatLiteral: {
      std::string str = token.str;
      void *value = allocator.construct<float>(std::atof(str.c_str()));
      return allocator.construct<ConstantNode>(value, CPP_TYPE<float>());
    }
    case TokenType::ParenOpen: {
      ASTNode *expr = parse_expression(tokens, allocator);
      tokens.consume(TokenType::ParenClose);
      return expr;
    }
    default:
      BLI_assert(false);
      return nullptr;
  }
}

static ASTNode *parse_expression__mul_div_level(TokenStream &tokens,
                                                MonotonicAllocator<> &allocator)
{
  ASTNode *left_expr = parse_expression__atom_level(tokens, allocator);
  while (tokens.next_is(TokenType::Asterix) || tokens.next_is(TokenType::ForwardSlash)) {
    Token operator_token = tokens.consume();
    ASTNode *right_expr = parse_expression__atom_level(tokens, allocator);
    MutableArrayRef<ASTNode *> operands = allocator.allocate_array<ASTNode *>(2);
    operands[0] = left_expr;
    operands[1] = right_expr;
    if (tokens.next_is(TokenType::Asterix)) {
      left_expr = allocator.construct<BinaryOperationNode>(BinaryOperationType::Multiply,
                                                           operands);
    }
    else {
      left_expr = allocator.construct<BinaryOperationNode>(BinaryOperationType::Divide, operands);
    }
  }
  return left_expr;
}

static ASTNode *parse_expression__add_sub_level(TokenStream &tokens,
                                                MonotonicAllocator<> &allocator)
{
  ASTNode *left_expr = parse_expression__mul_div_level(tokens, allocator);
  while (tokens.next_is(TokenType::Plus) || tokens.next_is(TokenType::Minus)) {
    Token operator_token = tokens.consume();
    ASTNode *right_expr = parse_expression__mul_div_level(tokens, allocator);
    MutableArrayRef<ASTNode *> operands = allocator.allocate_array<ASTNode *>(2);
    operands[0] = left_expr;
    operands[1] = right_expr;
    if (tokens.next_is(TokenType::Plus)) {
      left_expr = allocator.construct<BinaryOperationNode>(BinaryOperationType::Plus, operands);
    }
    else {
      left_expr = allocator.construct<BinaryOperationNode>(BinaryOperationType::Minus, operands);
    }
  }
  return left_expr;
}

static ASTNode *parse_expression(TokenStream &tokens, MonotonicAllocator<> &allocator)
{
  return parse_expression__add_sub_level(tokens, allocator);
}

ASTNode &parse_tokens(StringRef str, ArrayRef<Token> tokens, MonotonicAllocator<> &allocator)
{
  TokenStream token_stream(tokens);
  ASTNode *node = parse_expression(token_stream, allocator);
  return *node;
}

}  // namespace Expr
}  // namespace FN
