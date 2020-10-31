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

#include "FN_lang_parse.hh"
#include "FN_lang_tokenize.hh"

#include "BLI_dot_export.hh"

namespace blender::fn::lang {

class TokensToAstBuilder {
 private:
  StringRef str_;
  Span<TokenType> token_types_;
  Span<TokenRange> token_ranges_;
  LinearAllocator<> &allocator_;
  int current_ = 0;

 public:
  TokensToAstBuilder(StringRef str,
                     Span<TokenType> token_types,
                     Span<TokenRange> token_ranges,
                     LinearAllocator<> &allocator)
      : str_(str), token_types_(token_types), token_ranges_(token_ranges), allocator_(allocator)
  {
    BLI_assert(token_types.last() == TokenType::EndOfString);
  }

  AstNode &parse_expression()
  {
    return this->parse_expression__comparison_level();
  }

  bool is_at_end() const
  {
    return current_ == token_ranges_.size();
  }

 private:
  AstNode &parse_expression__comparison_level()
  {
    AstNode *left_expr = &this->parse_expression__add_sub_level();
    if (this->is_comparison_token(this->next_type())) {
      AstNodeType node_type = this->get_comparison_node_type(this->next_type());
      this->consume();
      AstNode &right_expr = this->parse_expression__add_sub_level();
      return this->construct_binary_expression_node(node_type, *left_expr, right_expr);
    }
    return *left_expr;
  }

  AstNode &parse_expression__add_sub_level()
  {
    AstNode *left_expr = &this->parse_expression__mul_div_level();
    while (this->is_add_sub_token(this->next_type())) {
      AstNodeType node_type = this->get_add_sub_node_type(this->next_type());
      this->consume();
      AstNode &right_expr = this->parse_expression__mul_div_level();
      left_expr = &this->construct_binary_expression_node(node_type, *left_expr, right_expr);
    }
    return *left_expr;
  }

  AstNode &parse_expression__mul_div_level()
  {
    AstNode *left_expr = &this->parse_expression__power_level();
    while (is_mul_div_token(this->next_type())) {
      AstNodeType node_type = this->get_mul_div_node_type(this->next_type());
      this->consume();
      AstNode &right_expr = this->parse_expression__power_level();
      left_expr = &this->construct_binary_expression_node(node_type, *left_expr, right_expr);
    }
    return *left_expr;
  }

  AstNode &parse_expression__power_level()
  {
    AstNode &base_expr = this->parse_expression__attribute_level();
    if (this->next_type() == TokenType::DoubleAsterix) {
      this->consume();
      AstNode &exponent_expr = this->parse_expression__attribute_level();
      return this->construct_binary_expression_node(AstNodeType::Power, base_expr, exponent_expr);
    }
    return base_expr;
  }

  AstNode &parse_expression__attribute_level()
  {
    AstNode &expr = parse_expression__atom_level();
    if (this->next_type() == TokenType::Dot) {
      this->consume();
      BLI_assert(this->next_type() == TokenType::Identifier);
      StringRef token_str = this->consume_next_str();
      StringRefNull name = allocator_.copy_string(token_str);
      if (this->next_type() == TokenType::ParenOpen) {
        Vector<AstNode *> args;
        args.append(&expr);
        this->parse_argument_list(args);
        MutableSpan<AstNode *> children = allocator_.construct_array_copy(args.as_span());
        return *allocator_.construct<MethodCallNode>(name, children);
      }
      MutableSpan<AstNode *> children = allocator_.allocate_array<AstNode *>(1);
      children[0] = &expr;
      return *allocator_.construct<AttributeNode>(name, children);
    }
    return expr;
  }

  AstNode &parse_expression__atom_level()
  {
    switch (this->next_type()) {
      case TokenType::Identifier: {
        StringRef token_str = this->consume_next_str();
        StringRefNull identifier = allocator_.copy_string(token_str);
        if (this->next_type() == TokenType::ParenOpen) {
          Vector<AstNode *> args;
          this->parse_argument_list(args);
          MutableSpan<AstNode *> children = allocator_.construct_array_copy(args.as_span());
          return *allocator_.construct<CallNode>(identifier, children);
        }
        return *allocator_.construct<IdentifierNode>(identifier);
      }
      case TokenType::IntLiteral:
        return this->consume_constant_int();
      case TokenType::FloatLiteral:
        return this->consume_constant_float();
      case TokenType::StringLiteral:
        return this->consume_constant_string();
      case TokenType::Minus: {
        this->consume();
        AstNode &expr = this->parse_expression__mul_div_level();
        return this->construct_unary_expression_node(AstNodeType::Negate, expr);
      }
      case TokenType::Plus: {
        this->consume();
        return this->parse_expression__mul_div_level();
      }
      case TokenType::ParenOpen: {
        this->consume();
        AstNode &expr = this->parse_expression();
        this->consume(TokenType::ParenClose);
        return expr;
      }
      case TokenType::EndOfString: {
        throw std::runtime_error("unexpected end of string");
      }
      default:
        throw std::runtime_error("unexpected token: " + token_type_to_string(this->next_type()));
    }
  }

  void parse_argument_list(Vector<AstNode *> &r_args)
  {
    this->consume(TokenType::ParenOpen);
    while (this->next_type() != TokenType::ParenClose) {
      r_args.append(&parse_expression());
      if (this->next_type() == TokenType::Comma) {
        this->consume();
      }
    }
    this->consume(TokenType::ParenClose);
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

  IdentifierNode &consume_identifier()
  {
    StringRef token_str = this->consume_next_str();
    StringRefNull identifier = allocator_.copy_string(token_str);
    return *allocator_.construct<IdentifierNode>(identifier);
  }

  ConstantIntNode &consume_constant_int()
  {
    StringRef token_str = this->consume_next_str();
    char *str = (char *)alloca(token_str.size() + 1);
    token_str.unsafe_copy(str);
    int value = std::atoi(str);
    return *allocator_.construct<ConstantIntNode>(value);
  }

  ConstantFloatNode &consume_constant_float()
  {
    StringRef token_str = this->consume_next_str();
    char *str = (char *)alloca(token_str.size() + 1);
    token_str.unsafe_copy(str);
    float value = std::atof(str);
    return *allocator_.construct<ConstantFloatNode>(value);
  }

  ConstantStringNode &consume_constant_string()
  {
    StringRef token_str = this->consume_next_str();
    StringRef stripped_str = token_str.substr(1, token_str.size() - 2);
    StringRefNull value = allocator_.copy_string(stripped_str);
    return *allocator_.construct<ConstantStringNode>(value);
  }

  AstNode &construct_binary_expression_node(AstNodeType node_type,
                                            AstNode &left_node,
                                            AstNode &right_node)
  {
    MutableSpan<AstNode *> children = allocator_.allocate_array<AstNode *>(2);
    children[0] = &left_node;
    children[1] = &right_node;
    AstNode *node = allocator_.construct<AstNode>(children, node_type);
    return *node;
  }

  AstNode &construct_unary_expression_node(AstNodeType node_type, AstNode &sub_node)
  {
    MutableSpan<AstNode *> children = allocator_.allocate_array<AstNode *>(1);
    children[0] = &sub_node;
    AstNode *node = allocator_.construct<AstNode>(children, node_type);
    return *node;
  }

  bool is_comparison_token(TokenType token_type)
  {
    int type = (int)token_type;
    return type >= (int)TokenType::IsLess && type <= (int)TokenType::IsGreaterOrEqual;
  }

  AstNodeType get_comparison_node_type(TokenType token_type)
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

  bool is_add_sub_token(TokenType token_type)
  {
    return ELEM(token_type, TokenType::Plus, TokenType::Minus);
  }

  AstNodeType get_add_sub_node_type(TokenType token_type)
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

  bool is_mul_div_token(TokenType token_type)
  {
    return ELEM(token_type, TokenType::Asterix, TokenType::ForwardSlash);
  }

  AstNodeType get_mul_div_node_type(TokenType token_type)
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
};

AstNode &parse_expression(StringRef str, LinearAllocator<> &allocator)
{
  TokenizeResult tokens = tokenize(str);

  tokens.types.append(TokenType::EndOfString);
  TokensToAstBuilder builder(str, tokens.types, tokens.ranges, allocator);
  AstNode &node = builder.parse_expression();
  if (!builder.is_at_end()) {
    throw std::runtime_error("syntax error");
  }
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
  for (int i : ast_node.children.index_range()) {
    AstNode &child = *ast_node.children[i];
    dot::Node &dot_child = ast_to_dot_node(digraph, child);
    dot::DirectedEdge &dot_edge = digraph.new_edge(dot_node, dot_child);
    dot_edge.attributes.set("label", std::to_string(i));
  }
  return dot_node;
}

std::string AstNode::to_dot() const
{
  dot::DirectedGraph digraph;
  ast_to_dot_node(digraph, *this);
  return digraph.to_dot_string();
}

}  // namespace blender::fn::lang
