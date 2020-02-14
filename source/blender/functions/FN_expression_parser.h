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
  Less = 1,
  Greater = 2,
  Equal = 3,
  LessOrEqual = 4,
  GreaterOrEqual = 5,

  Plus = 6,
  Minus = 7,
  Multiply = 8,
  Divide = 9,

  Identifier,
  ConstantInt,
  ConstantFloat,
  ConstantString,
  Negate,
  Power,
  Call,
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

  std::string to_dot() const;
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

struct CallNode : public AstNode {
  StringRefNull name;

  CallNode(StringRefNull name, MutableArrayRef<AstNode *> args)
      : AstNode(args, AstNodeType::Call), name(name)
  {
  }
};

AstNode &parse_expression(StringRef str, LinearAllocator<> &allocator);

}  // namespace Expr
}  // namespace FN
