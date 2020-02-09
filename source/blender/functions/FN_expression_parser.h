#pragma once

#include "BLI_array_ref.h"
#include "BLI_monotonic_allocator.h"

#include "FN_expression_lexer.h"
#include "FN_cpp_type.h"

namespace FN {
namespace Expr {

using BLI::ArrayRef;
using BLI::MonotonicAllocator;
using BLI::MutableArrayRef;

namespace ASTNodeType {
enum Enum {
  Identifier,
  Constant,
  BinaryOperation,
};
}

namespace BinaryOperationType {
enum Enum {
  Plus,
  Minus,
  Multiply,
  Divide,
};
}

struct ASTNode {
  MutableArrayRef<ASTNode *> children;
  ASTNodeType::Enum type;

  ASTNode(MutableArrayRef<ASTNode *> children, ASTNodeType::Enum type)
      : children(children), type(type)
  {
  }

  void print() const
  {
    std::cout << type << "(";
    for (ASTNode *child : children) {
      child->print();
    }
    std::cout << ")";
  }
};

struct IdentifierNode : public ASTNode {
  StringRef value;

  IdentifierNode(StringRef value) : ASTNode({}, ASTNodeType::Identifier), value(value)
  {
  }
};

struct ConstantNode : public ASTNode {
  void *value;
  const CPPType &type;

  ConstantNode(void *value, const CPPType &type)
      : ASTNode({}, ASTNodeType::Constant), value(value), type(type)
  {
  }
};

struct BinaryOperationNode : public ASTNode {
  BinaryOperationType::Enum op_type;

  BinaryOperationNode(BinaryOperationType::Enum op_type, MutableArrayRef<ASTNode *> operands)
      : ASTNode(operands, ASTNodeType::BinaryOperation), op_type(op_type)
  {
  }
};

ASTNode &parse_tokens(StringRef str, ArrayRef<Token> tokens, MonotonicAllocator<> &allocator);

}  // namespace Expr
}  // namespace FN
