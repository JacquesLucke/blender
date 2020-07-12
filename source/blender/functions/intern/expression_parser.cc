#include "BLI_dot_export.hh"
#include "FN_expression_lexer.hh"
#include "FN_expression_parser.hh"

namespace blender::fn {

class TokensToAstBuilder {
 private:
  StringRef str_;
  Span<TokenType> token_types_;
  Span<TokenRange> token_ranges_;
  LinearAllocator<> &allocator_;
  uint current_ = 0;

 public:
  TokensToAstBuilder(StringRef str,
                     Span<TokenType> token_types,
                     Span<TokenRange> token_ranges,
                     LinearAllocator<> &allocator)
      : str_(str), token_types_(token_types), token_ranges_(token_ranges), allocator_(allocator)
  {
    BLI_assert(token_types.last() == TokenType::EndOfString);
  }

  LinearAllocator<> &allocator()
  {
    return allocator_;
  }

  TokenType next_type() const
  {
    return token_types_[current_];
  }

  StringRef next_str() const
  {
    BLI_assert(!this->is_at_end());
    TokenRange range = token_ranges_[current_];
    return str_.substr(range.start, range.size);
  }

  StringRef consume_next_str()
  {
    StringRef str = this->next_str();
    current_++;
    return str;
  }

  bool is_at_end() const
  {
    return current_ == token_ranges_.size();
  }

  void consume(TokenType token_type)
  {
    UNUSED_VARS_NDEBUG(token_type);
    BLI_assert(this->next_type() == token_type);
    this->consume();
  }

  void consume()
  {
    BLI_assert(!this->is_at_end());
    current_++;
  }

  IdentifierNode *consume_identifier()
  {
    StringRef token_str = this->consume_next_str();
    StringRefNull identifier = allocator_.copy_string(token_str);
    return allocator_.construct<IdentifierNode>(identifier);
  }

  ConstantIntNode *consume_constant_int()
  {
    StringRef token_str = this->consume_next_str();
    char *str = (char *)alloca(token_str.size() + 1);
    token_str.unsafe_copy(str);
    int value = std::atoi(str);
    return allocator_.construct<ConstantIntNode>(value);
  }

  ConstantFloatNode *consume_constant_float()
  {
    StringRef token_str = this->consume_next_str();
    char *str = (char *)alloca(token_str.size() + 1);
    token_str.unsafe_copy(str);
    float value = std::atof(str);
    return allocator_.construct<ConstantFloatNode>(value);
  }

  ConstantStringNode *consume_constant_string()
  {
    StringRef token_str = this->consume_next_str();
    StringRefNull value = allocator_.copy_string(token_str);
    return allocator_.construct<ConstantStringNode>(value);
  }

  AstNode *construct_binary_node(AstNodeType node_type, AstNode *left_node, AstNode *right_node)
  {
    MutableSpan<AstNode *> children = allocator_.allocate_array<AstNode *>(2);
    children[0] = left_node;
    children[1] = right_node;
    AstNode *node = allocator_.construct<AstNode>(children, node_type);
    return node;
  }

  AstNode *construct_unary_node(AstNodeType node_type, AstNode *sub_node)
  {
    MutableSpan<AstNode *> children = allocator_.allocate_array<AstNode *>(1);
    children[0] = sub_node;
    AstNode *node = allocator_.construct<AstNode>(children, node_type);
    return node;
  }
};

static bool is_comparison_token(TokenType token_type)
{
  int type = (int)token_type;
  return type >= (int)TokenType::IsLess && type <= (int)TokenType::IsGreaterOrEqual;
}

static AstNodeType get_comparison_node_type(TokenType token_type)
{
  switch (token_type) {
    case TokenType::IsLess:
      return AstNodeType::IsLess;
    case TokenType::IsGreater:
      return AstNodeType::IsGreater;
    case TokenType::IsEqual:
      return AstNodeType::IsEqual;
    case TokenType::IsLessOrEqual:
      return AstNodeType::IsLessOrEqual;
    case TokenType::IsGreaterOrEqual:
      return AstNodeType::IsGreaterOrEqual;
    default:
      BLI_assert(false);
      return AstNodeType::Error;
  }
}

static bool is_add_sub_token(TokenType token_type)
{
  return ELEM(token_type, TokenType::Plus, TokenType::Minus);
}

static AstNodeType get_add_sub_node_type(TokenType token_type)
{
  switch (token_type) {
    case TokenType::Plus:
      return AstNodeType::Plus;
    case TokenType::Minus:
      return AstNodeType::Minus;
    default:
      BLI_assert(false);
      return AstNodeType::Error;
  }
}

static bool is_mul_div_token(TokenType token_type)
{
  return ELEM(token_type, TokenType::Asterix, TokenType::ForwardSlash);
}

static AstNodeType get_mul_div_node_type(TokenType token_type)
{
  switch (token_type) {
    case TokenType::Asterix:
      return AstNodeType::Multiply;
    case TokenType::ForwardSlash:
      return AstNodeType::Divide;
    default:
      BLI_assert(false);
      return AstNodeType::Error;
  }
}

static AstNode *parse_expression(TokensToAstBuilder &builder);
static AstNode *parse_expression__comparison_level(TokensToAstBuilder &builder);
static AstNode *parse_expression__add_sub_level(TokensToAstBuilder &builder);
static AstNode *parse_expression__mul_div_level(TokensToAstBuilder &builder);
static AstNode *parse_expression__power_level(TokensToAstBuilder &builder);
static AstNode *parse_expression__attribute_level(TokensToAstBuilder &builder);
static AstNode *parse_expression__atolevel_(TokensToAstBuilder &builder);

static AstNode *parse_expression(TokensToAstBuilder &builder)
{
  return parse_expression__comparison_level(builder);
}

static AstNode *parse_expression__comparison_level(TokensToAstBuilder &builder)
{
  AstNode *left_expr = parse_expression__add_sub_level(builder);
  if (is_comparison_token(builder.next_type())) {
    AstNodeType node_type = get_comparison_node_type(builder.next_type());
    builder.consume();
    AstNode *right_expr = parse_expression__add_sub_level(builder);
    return builder.construct_binary_node(node_type, left_expr, right_expr);
  }
  else {
    return left_expr;
  }
}

static AstNode *parse_expression__add_sub_level(TokensToAstBuilder &builder)
{
  AstNode *left_expr = parse_expression__mul_div_level(builder);
  while (is_add_sub_token(builder.next_type())) {
    AstNodeType node_type = get_add_sub_node_type(builder.next_type());
    builder.consume();
    AstNode *right_expr = parse_expression__mul_div_level(builder);
    left_expr = builder.construct_binary_node(node_type, left_expr, right_expr);
  }
  return left_expr;
}

static AstNode *parse_expression__mul_div_level(TokensToAstBuilder &builder)
{
  AstNode *left_expr = parse_expression__power_level(builder);
  while (is_mul_div_token(builder.next_type())) {
    AstNodeType node_type = get_mul_div_node_type(builder.next_type());
    builder.consume();
    AstNode *right_expr = parse_expression__power_level(builder);
    left_expr = builder.construct_binary_node(node_type, left_expr, right_expr);
  }
  return left_expr;
}

static AstNode *parse_expression__power_level(TokensToAstBuilder &builder)
{
  AstNode *base_expr = parse_expression__attribute_level(builder);
  if (builder.next_type() == TokenType::DoubleAsterix) {
    builder.consume();
    AstNode *exponent_expr = parse_expression__attribute_level(builder);
    return builder.construct_binary_node(AstNodeType::Power, base_expr, exponent_expr);
  }
  else {
    return base_expr;
  }
}

static void parse_argument_list(TokensToAstBuilder &builder, Vector<AstNode *> &r_args)
{
  builder.consume(TokenType::ParenOpen);
  while (builder.next_type() != TokenType::ParenClose) {
    r_args.append(parse_expression(builder));
    if (builder.next_type() == TokenType::Comma) {
      builder.consume();
    }
  }
  builder.consume(TokenType::ParenClose);
}

static AstNode *parse_expression__attribute_level(TokensToAstBuilder &builder)
{
  AstNode *expr = parse_expression__atolevel_(builder);
  if (builder.next_type() == TokenType::Dot) {
    builder.consume();
    BLI_assert(builder.next_type() == TokenType::Identifier);
    StringRef token_str = builder.consume_next_str();
    StringRefNull name = builder.allocator().copy_string(token_str);
    if (builder.next_type() == TokenType::ParenOpen) {
      Vector<AstNode *> args;
      args.append(expr);
      parse_argument_list(builder, args);
      MutableSpan<AstNode *> children = builder.allocator().construct_array_copy(args.as_span());
      return builder.allocator().construct<MethodCallNode>(name, children);
    }
    else {
      MutableSpan<AstNode *> children = builder.allocator().allocate_array<AstNode *>(1);
      children[0] = expr;
      return builder.allocator().construct<AttributeNode>(name, children);
    }
  }
  else {
    return expr;
  }
}

static AstNode *parse_expression__atolevel_(TokensToAstBuilder &builder)
{
  switch (builder.next_type()) {
    case TokenType::Identifier: {
      StringRef token_str = builder.consume_next_str();
      StringRefNull identifier = builder.allocator().copy_string(token_str);
      if (builder.next_type() == TokenType::ParenOpen) {
        Vector<AstNode *> args;
        parse_argument_list(builder, args);
        MutableSpan<AstNode *> children = builder.allocator().construct_array_copy(args.as_span());
        return builder.allocator().construct<CallNode>(identifier, children);
      }
      else {
        return builder.allocator().construct<IdentifierNode>(identifier);
      }
    }
    case TokenType::IntLiteral:
      return builder.consume_constant_int();
    case TokenType::FloatLiteral:
      return builder.consume_constant_float();
    case TokenType::Minus: {
      builder.consume();
      AstNode *expr = parse_expression__mul_div_level(builder);
      return builder.construct_unary_node(AstNodeType::Negate, expr);
    }
    case TokenType::Plus: {
      builder.consume();
      return parse_expression__mul_div_level(builder);
    }
    case TokenType::ParenOpen: {
      builder.consume();
      AstNode *expr = parse_expression(builder);
      builder.consume(TokenType::ParenClose);
      return expr;
    }
    default:
      BLI_assert(false);
      return nullptr;
  }
}

AstNode &parse_expression(StringRef str, LinearAllocator<> &allocator)
{
  TokenizeResult tokens = tokenize(str);

  for (uint i : tokens.types.index_range()) {
    std::cout << i << ": " << token_type_to_string(tokens.types[i]) << ": "
              << tokens.ranges[i].get(str) << '\n';
  }

  tokens.types.append(TokenType::EndOfString);
  TokensToAstBuilder builder(str, tokens.types, tokens.ranges, allocator);
  AstNode &node = *parse_expression(builder);
  BLI_assert(builder.is_at_end());
  return node;
}

StringRefNull node_type_to_string(AstNodeType node_type)
{
  switch (node_type) {
    case AstNodeType::Error:
      return "Error";
    case AstNodeType::Identifier:
      return "Identifier";
    case AstNodeType::ConstantInt:
      return "ConstantInt";
    case AstNodeType::ConstantFloat:
      return "ConstantFloat";
    case AstNodeType::ConstantString:
      return "ConstantString";
    case AstNodeType::Plus:
      return "Plus";
    case AstNodeType::Minus:
      return "Minus";
    case AstNodeType::Multiply:
      return "Multiply";
    case AstNodeType::Divide:
      return "Divide";
    case AstNodeType::IsLess:
      return "IsLess";
    case AstNodeType::IsGreater:
      return "IsIsGreater";
    case AstNodeType::IsEqual:
      return "IsEqual";
    case AstNodeType::IsLessOrEqual:
      return "IsLessOrEqual";
    case AstNodeType::IsGreaterOrEqual:
      return "IsGreaterOrEqual";
    case AstNodeType::Negate:
      return "Negate";
    case AstNodeType::Power:
      return "Power";
    case AstNodeType::Call:
      return "Call";
    case AstNodeType::Attribute:
      return "Attribute";
    case AstNodeType::MethodCall:
      return "MethodCall";
  }
  BLI_assert(false);
  return "";
}

static std::string get_ast_node_label(const AstNode &ast_node)
{
  switch (ast_node.type) {
    case AstNodeType::Identifier:
      return ((IdentifierNode &)ast_node).value;
    case AstNodeType::ConstantFloat:
      return std::to_string(((ConstantFloatNode &)ast_node).value);
    case AstNodeType::ConstantInt:
      return std::to_string(((ConstantIntNode &)ast_node).value);
    case AstNodeType::ConstantString:
      return ((ConstantStringNode &)ast_node).value;
    case AstNodeType::Call:
      return ((CallNode &)ast_node).name;
    default: {
      return node_type_to_string(ast_node.type);
    }
  }
}

static dot::Node &ast_to_dot_node(dot::DirectedGraph &digraph, const AstNode &ast_node)
{
  std::string node_label = get_ast_node_label(ast_node);
  dot::Node &dot_node = digraph.new_node(node_label);
  for (uint i : ast_node.children.index_range()) {
    AstNode &child = *ast_node.children[i];
    dot::Node &dot_child = ast_to_dot_node(digraph, child);
    dot::DirectedEdge &dot_edge = digraph.new_edge(dot_node, dot_child);
    dot_edge.set_attribute("label", std::to_string(i));
  }
  return dot_node;
}

std::string AstNode::to_dot() const
{
  dot::DirectedGraph digraph;
  ast_to_dot_node(digraph, *this);
  return digraph.to_dot_string();
}

}  // namespace blender::fn
