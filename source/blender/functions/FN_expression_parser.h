#pragma once

#include "BLI_array_ref.h"
#include "FN_expression_lexer.h"
#include "BLI_monotonic_allocator.h"

namespace FN {
namespace Expr {

using BLI::ArrayRef;
using BLI::MonotonicAllocator;
using BLI::MutableArrayRef;

namespace ASTNodeType {
enum Enum {
  Plus,
  Minus,
  Identifier,
  IntConstant,
  FloatConstant,
  InfixOperation,
};
}

struct ASTNode {
  MutableArrayRef<ASTNode *> children;
  ASTNodeType::Enum type;

  ASTNode(MutableArrayRef<ASTNode *> children, ASTNodeType::Enum type)
      : children(children), type(type)
  {
  }
};

struct IdentifierNode : public ASTNode {
  StringRef value;

  IdentifierNode(StringRef value) : ASTNode({}, ASTNodeType::Identifier), value(value)
  {
  }
};

struct IntConstantNode : public ASTNode {
  StringRef value;

  IntConstantNode(StringRef value) : ASTNode({}, ASTNodeType::IntConstant), value(value)
  {
  }
};

struct FloatConstantNode : public ASTNode {
  StringRef value;

  FloatConstantNode(StringRef value) : ASTNode({}, ASTNodeType::FloatConstant), value(value)
  {
  }
};

struct InfixOperationNode : public ASTNode {
  StringRef value;

  InfixOperationNode(StringRef value, MutableArrayRef<ASTNode *> operands)
      : ASTNode(operands, ASTNodeType::InfixOperation), value(value)
  {
  }
};

ASTNode &parse_tokens(StringRef str, ArrayRef<Token> tokens, MonotonicAllocator<> &allocator);

}  // namespace Expr
}  // namespace FN
