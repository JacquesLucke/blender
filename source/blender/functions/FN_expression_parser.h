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

enum class ASTNodeType : uchar {
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
  ShiftLeft,
  ShiftRight,
  Negate,
  Power,
};

StringRefNull node_type_to_string(ASTNodeType node_type);

struct ASTNode {
  MutableArrayRef<ASTNode *> children;
  ASTNodeType type;

  ASTNode(MutableArrayRef<ASTNode *> children, ASTNodeType type) : children(children), type(type)
  {
  }

  void print() const
  {
    std::cout << node_type_to_string(type) << "(";
    for (ASTNode *child : children) {
      child->print();
    }
    std::cout << ")";
  }
};

struct IdentifierNode : public ASTNode {
  StringRefNull value;

  IdentifierNode(StringRefNull value) : ASTNode({}, ASTNodeType::Identifier), value(value)
  {
  }
};

struct ConstantFloatNode : public ASTNode {
  float value;

  ConstantFloatNode(float value) : ASTNode({}, ASTNodeType::ConstantFloat), value(value)
  {
  }
};

struct ConstantIntNode : public ASTNode {
  int value;

  ConstantIntNode(int value) : ASTNode({}, ASTNodeType::ConstantInt), value(value)
  {
  }
};

struct ConstantStringNode : public ASTNode {
  StringRefNull value;

  ConstantStringNode(StringRefNull value) : ASTNode({}, ASTNodeType::ConstantString), value(value)
  {
  }
};

ASTNode &parse_expression(StringRef str, LinearAllocator<> &allocator);

}  // namespace Expr
}  // namespace FN
