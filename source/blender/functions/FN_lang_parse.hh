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

#pragma once

#include "BLI_linear_allocator.hh"
#include "BLI_string_ref.hh"

namespace blender::fn::lang {

enum class AstNodeType : uint8_t {
  Error,
  IsLess,
  IsGreater,
  IsEqual,
  IsLessOrEqual,
  IsGreaterOrEqual,
  Plus,
  Minus,
  Multiply,
  Divide,
  Identifier,
  ConstantInt,
  ConstantFloat,
  ConstantString,
  Negate,
  Power,
  Call,
  Attribute,
  MethodCall,
  Program,
};

StringRefNull node_type_to_string(AstNodeType node_type);

struct AstNode : NonCopyable, NonMovable {
  MutableSpan<AstNode *> children;
  AstNodeType type;

  AstNode(MutableSpan<AstNode *> children, AstNodeType type) : children(children), type(type)
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

  CallNode(StringRefNull name, MutableSpan<AstNode *> args)
      : AstNode(args, AstNodeType::Call), name(name)
  {
  }
};

struct AttributeNode : public AstNode {
  StringRefNull name;

  AttributeNode(StringRefNull name, MutableSpan<AstNode *> args)
      : AstNode(args, AstNodeType::Attribute), name(name)
  {
    BLI_assert(args.size() == 1);
  }
};

struct MethodCallNode : public AstNode {
  StringRefNull name;

  MethodCallNode(StringRefNull name, MutableSpan<AstNode *> args)
      : AstNode(args, AstNodeType::MethodCall), name(name)
  {
    BLI_assert(args.size() >= 1);
  }
};

AstNode &parse_expression(StringRef expression_str, LinearAllocator<> &allocator);
AstNode &parse_program(StringRef program_str, LinearAllocator<> &allocator);

}  // namespace blender::fn::lang
