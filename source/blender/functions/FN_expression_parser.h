#pragma once

#include "BLI_array_ref.h"
#include "BLI_linear_allocator.h"

#include "FN_expression_lexer.h"
#include "FN_cpp_type.h"

namespace FN {
namespace Expr {

using BLI::ArrayRef;
using BLI::LinearAllocator;
using BLI::MutableArrayRef;

enum class AstNodeType : uchar {
  Identifier,
  ConstantInt,
  ConstantFloat,
  ConstantString,
  Plus,
  Minus,
  Multiply,
  Divide,
  Less,
  Greater,
  Equal,
  LessOrEqual,
  GreaterOrEqual,
  DoubleLess,
  DoubleRight,
  Negate,
  Power,
};

StringRefNull node_type_to_string(AstNodeType node_type);

struct AstNode {
  MutableArrayRef<AstNode *> children;
  AstNodeType type;

  AstNode(MutableArrayRef<AstNode *> children, AstNodeType type) : children(children), type(type)
  {
  }

  void print() const
  {
    std::cout << node_type_to_string(type) << "(";
    for (AstNode *child : children) {
      child->print();
    }
    std::cout << ")";
  }
};

struct IdentifierNode : public AstNode {
  StringRefNull value;

  IdentifierNode(StringRefNull value) : AstNode({}, AstNodeType::Identifier), value(value)
  {
  }
};

struct ConstantFloatNode : public AstNode {
  float value;

  ConstantFloatNode(float value) : AstNode({}, AstNodeType::ConstantFloat), value(value)
  {
  }
};

struct ConstantIntNode : public AstNode {
  int value;

  ConstantIntNode(int value) : AstNode({}, AstNodeType::ConstantInt), value(value)
  {
  }
};

struct ConstantStringNode : public AstNode {
  StringRefNull value;

  ConstantStringNode(StringRefNull value) : AstNode({}, AstNodeType::ConstantString), value(value)
  {
  }
};

AstNode &parse_expression(StringRef str, LinearAllocator<> &allocator);

}  // namespace Expr
}  // namespace FN
