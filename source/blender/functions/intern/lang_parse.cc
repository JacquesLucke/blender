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

  AstNode &parse_program()
  {
    Vector<AstNode *> statements;
    while (!this->is_at_end()) {
      AstNode &stmt = this->parse_statement();
      statements.append(&stmt);
    }
    MutableSpan<AstNode *> statements_span = allocator_.construct_array_copy(statements.as_span());
    return *allocator_.construct<AstNode>(statements_span, AstNodeType::Program);
  }

  AstNode &parse_expression()
  {
    return this->parse_expression__comparison_level();
  }

  bool is_at_end() const
  {
    return current_ + 1 == token_ranges_.size();
  }

 private:
  AstNode &parse_statement()
  {
    if (this->next_token_is("if")) {
      return this->parse_statement__if();
    }
    if (this->next_token_is(TokenType::CurlyOpen)) {
      return this->parse_statement__group();
    }
    return this->parse_statement__expression_or_assignment();
  }

  AstNode &parse_statement__if()
  {
    this->consume("if");
    this->consume(TokenType::ParenOpen);
    AstNode &condition = this->parse_expression();
    this->consume(TokenType::ParenClose);
    AstNode &then_stmt = this->parse_statement();
    if (this->next_token_is("else")) {
      this->consume("else");
      AstNode &else_stmt = this->parse_statement();
      return this->construct_node(AstNodeType::IfStmt, {&condition, &then_stmt, &else_stmt});
    }
    return this->construct_node(AstNodeType::IfStmt, {&condition, &then_stmt});
  }

  AstNode &parse_statement__group()
  {
    this->consume(TokenType::CurlyOpen);
    Vector<AstNode *> statements;
    while (!this->next_token_is(TokenType::CurlyClose)) {
      AstNode &stmt = this->parse_statement();
      statements.append(&stmt);
    }
    this->consume(TokenType::CurlyClose);
    return this->construct_node(AstNodeType::GroupStmt, statements);
  }

  AstNode &parse_statement__expression_or_assignment()
  {
    AstNode &left_side = this->parse_expression();
    if (this->next_token_is(TokenType::Semicolon)) {
      this->consume(TokenType::Semicolon);
      return this->construct_node(AstNodeType::ExpressionStmt, {&left_side});
    }
    if (this->next_token_is(TokenType::Equal)) {
      this->consume(TokenType::Equal);
      AstNode &right_side = this->parse_expression();
      this->consume(TokenType::Semicolon);
      return this->construct_node(AstNodeType::AssignmentStmt, {&left_side, &right_side});
    }
    throw std::runtime_error("expected semicolon or assignment");
  }

  AstNode &parse_expression__comparison_level()
  {
    AstNode *left_expr = &this->parse_expression__add_sub_level();
    if (this->is_comparison_token(this->next_type())) {
      AstNodeType node_type = this->get_comparison_node_type(this->next_type());
      this->consume();
      AstNode &right_expr = this->parse_expression__add_sub_level();
      return this->construct_node(node_type, {left_expr, &right_expr});
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
      left_expr = &this->construct_node(node_type, {left_expr, &right_expr});
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
      left_expr = &this->construct_node(node_type, {left_expr, &right_expr});
    }
    return *left_expr;
  }

  AstNode &parse_expression__power_level()
  {
    AstNode &base_expr = this->parse_expression__attribute_level();
    if (this->next_token_is(TokenType::DoubleAsterix)) {
      this->consume();
      AstNode &exponent_expr = this->parse_expression__attribute_level();
      return this->construct_node(AstNodeType::Power, {&base_expr, &exponent_expr});
    }
    return base_expr;
  }

  AstNode &parse_expression__attribute_level()
  {
    AstNode &expr = parse_expression__atom_level();
    if (this->next_token_is(TokenType::Dot)) {
      this->consume();
      BLI_assert(this->next_type() == TokenType::Identifier);
      StringRef token_str = this->consume_next_str();
      StringRefNull name = allocator_.copy_string(token_str);
      if (this->next_token_is(TokenType::ParenOpen)) {
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
      case TokenType::Identifier:
        return this->parse_expression__identifier_or_call();
      case TokenType::IntLiteral:
        return this->parse_expression__constant_int();
      case TokenType::FloatLiteral:
        return this->parse_expression__constant_float();
      case TokenType::StringLiteral:
        return this->parse_expression__constant_string();
      case TokenType::Minus:
        return this->parse_expression__unary_subtract();
      case TokenType::Plus:
        return this->parse_expression__unary_add();
      case TokenType::ParenOpen:
        return this->parse_expression__parentheses();
      case TokenType::EndOfString: {
        throw std::runtime_error("unexpected end of string");
      }
      default:
        throw std::runtime_error("unexpected token: " + token_type_to_string(this->next_type()));
    }
  }

  AstNode &parse_expression__identifier_or_call()
  {
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

  AstNode &parse_expression__constant_int()
  {
    StringRef token_str = this->consume_next_str();
    char *str = (char *)alloca(token_str.size() + 1);
    token_str.unsafe_copy(str);
    int value = std::atoi(str);
    return *allocator_.construct<ConstantIntNode>(value);
  }

  AstNode &parse_expression__constant_float()
  {
    StringRef token_str = this->consume_next_str();
    char *str = (char *)alloca(token_str.size() + 1);
    token_str.unsafe_copy(str);
    float value = std::atof(str);
    return *allocator_.construct<ConstantFloatNode>(value);
  }

  AstNode &parse_expression__constant_string()
  {
    StringRef token_str = this->consume_next_str();
    StringRef stripped_str = token_str.substr(1, token_str.size() - 2);
    StringRefNull value = allocator_.copy_string(stripped_str);
    return *allocator_.construct<ConstantStringNode>(value);
  }

  AstNode &parse_expression__unary_subtract()
  {
    this->consume(TokenType::Minus);
    AstNode &expr = this->parse_expression__mul_div_level();
    return this->construct_node(AstNodeType::Negate, {&expr});
  }

  AstNode &parse_expression__unary_add()
  {
    this->consume(TokenType::Plus);
    return this->parse_expression__mul_div_level();
  }

  AstNode &parse_expression__parentheses()
  {
    this->consume(TokenType::ParenOpen);
    AstNode &expr = this->parse_expression();
    this->consume(TokenType::ParenClose);
    return expr;
  }

  void parse_argument_list(Vector<AstNode *> &r_args)
  {
    this->consume(TokenType::ParenOpen);
    while (!this->next_token_is(TokenType::ParenClose)) {
      r_args.append(&parse_expression());
      if (this->next_token_is(TokenType::Comma)) {
        this->consume();
      }
    }
    this->consume(TokenType::ParenClose);
  }

  bool next_token_is(TokenType token_type)
  {
    return token_types_[current_] == token_type;
  }

  bool next_token_is(StringRef str)
  {
    return token_ranges_[current_].get(str_) == str;
  }

  TokenType next_type() const
  {
    return token_types_[current_];
  }

  StringRef consume_next_str()
  {
    StringRef str = token_ranges_[current_].get(str_);
    this->consume();
    current_++;
    return str;
  }

  void consume(TokenType token_type)
  {
    if (!this->next_token_is(token_type)) {
      throw std::runtime_error("unexpected token: " + token_type_to_string(this->next_type()) +
                               ", expected " + token_type_to_string(token_type));
    }
    this->consume();
  }

  void consume(StringRef str)
  {
    if (!this->next_token_is(str)) {
      throw std::runtime_error("unexpected token: " + token_ranges_[current_].get(str_));
    }
    this->consume();
  }

  void consume()
  {
    BLI_assert(!this->is_at_end());
    current_++;
  }

  AstNode &construct_node(AstNodeType node_type, Span<AstNode *> children)
  {
    MutableSpan<AstNode *> children_copy = allocator_.construct_array_copy(children);
    return *allocator_.construct<AstNode>(children_copy, node_type);
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

AstNode &parse_expression(StringRef expression_str, LinearAllocator<> &allocator)
{
  TokenizeResult tokens = tokenize(expression_str);
  tokens.ranges.append({0, 0});
  tokens.types.append(TokenType::EndOfString);

  TokensToAstBuilder builder(expression_str, tokens.types, tokens.ranges, allocator);
  AstNode &node = builder.parse_expression();
  if (!builder.is_at_end()) {
    throw std::runtime_error("unexpected end of expression");
  }
  return node;
}

AstNode &parse_program(StringRef program_str, LinearAllocator<> &allocator)
{
  TokenizeResult tokens = tokenize(program_str);
  tokens.ranges.append({0, 0});
  tokens.types.append(TokenType::EndOfString);

  TokensToAstBuilder builder(program_str, tokens.types, tokens.ranges, allocator);
  AstNode &node = builder.parse_program();
  if (!builder.is_at_end()) {
    throw std::runtime_error("unexpected end of program");
  }
  std::cout << node.to_dot() << "\n";
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
    case AstNodeType::Program:
      return "Program";
    case AstNodeType::AssignmentStmt:
      return "AssignmentStmt";
    case AstNodeType::IfStmt:
      return "IfStmt";
    case AstNodeType::GroupStmt:
      return "GroupStmt";
    case AstNodeType::ExpressionStmt:
      return "ExpressionStmt";
    case AstNodeType::DeclarationStmt:
      return "DeclarationStmt";
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
  dot_node.attributes.set("ordering", "out");
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
