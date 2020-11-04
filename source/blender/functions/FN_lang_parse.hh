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
#include "BLI_map.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"

namespace blender::fn::lang {

class Type;

class TypeMember {
 private:
  const Type *type_;
  std::string name_;

 public:
  TypeMember(std::string name, const Type &type) : type_(&type), name_(std::move(name))
  {
  }

  const Type &type() const
  {
    return *type_;
  }

  StringRef name() const
  {
    return name_;
  }
};

class Type {
 private:
  std::string name_;
  Array<TypeMember> members_;

 public:
  Type(std::string name, Array<TypeMember> members)
      : name_(std::move(name)), members_(std::move(members))
  {
  }

  StringRef name() const
  {
    return name_;
  }

  Span<TypeMember> members() const
  {
    return members_;
  }
};

class Variable {
 private:
  std::string name_;
  const Type *type_;

 public:
  Variable(std::string name, const Type &type) : name_(std::move(name)), type_(&type)
  {
  }

  StringRef name() const
  {
    return name_;
  }

  const Type &type() const
  {
    return *type_;
  }
};

struct Parameter {
  const Type *type;
  std::string name;
};

class Function {
 private:
  std::string name_;
  const Type *return_type_;
  Array<Parameter> parameters_;

 public:
  Function(std::string name, const Type &return_type, Array<Parameter> parameters)
      : name_(std::move(name)), return_type_(&return_type), parameters_(std::move(parameters))
  {
  }

  StringRef name() const
  {
    return name_;
  }

  const Type &return_type() const
  {
    return *return_type_;
  }

  Span<Parameter> parameters() const
  {
    return parameters_;
  }
};

struct FunctionArgs {
  Vector<const Type *> positional_args;
  Map<StringRef, const Type *> keyword_args;
};

class Scope {
 private:
  const Scope *parent_;
  Map<std::string, std::unique_ptr<Type>> types_;
  MultiValueMap<std::string, std::unique_ptr<Function>> functions_;
  Map<std::string, std::unique_ptr<Variable>> variables_;
  Set<std::pair<const Type *, const Type *>> implicit_conversions_;

 public:
  Scope(const Scope *parent = nullptr) : parent_(parent)
  {
  }

  const Type &add_type(StringRef name, Span<TypeMember> members)
  {
    if (this->resolve_type(name) != nullptr) {
      throw std::runtime_error("type exists already: " + name);
    }
    std::unique_ptr<Type> type = std::make_unique<Type>(name, members);
    const Type &type_ref = *type;
    types_.add_new_as(name, std::move(type));
    return type_ref;
  }

  const Function &add_function(StringRef name,
                               const Type &return_type,
                               Span<Parameter> parameters = {})
  {
    std::unique_ptr<Function> function = std::make_unique<Function>(name, return_type, parameters);
    const Function &function_ref = *function;
    functions_.add_as(name, std::move(function));
    return function_ref;
  }

  const Variable &add_variable(StringRef name, const Type &type)
  {
    if (this->resolve_variable(name) != nullptr) {
      throw std::runtime_error(
          "variable exists already (variable shadowing is not allowed currently): " + name);
    }
    std::unique_ptr<Variable> variable = std::make_unique<Variable>(name, type);
    const Variable &variable_ref = *variable;
    variables_.add_new_as(name, std::move(variable));
    return variable_ref;
  }

  void add_implicit_conversion(const Type &from_type, const Type &to_type)
  {
    implicit_conversions_.add({&from_type, &to_type});
  }

  const Type *resolve_type(StringRef name) const
  {
    const std::unique_ptr<Type> *type = types_.lookup_ptr_as(name);
    if (type != nullptr) {
      return type->get();
    }
    if (parent_ != nullptr) {
      return parent_->resolve_type(name);
    }
    return nullptr;
  }

  /* Might return 0 or more function candidates that match equally well. */
  Vector<const Function *> resolve_function(StringRef name, const FunctionArgs &args) const
  {
    return this->find_best_function_candidates(name, args);
  }

  const Variable *resolve_variable(StringRef name) const
  {
    const std::unique_ptr<Variable> *variable = variables_.lookup_ptr_as(name);
    if (variable != nullptr) {
      return variable->get();
    }
    if (parent_ != nullptr) {
      return parent_->resolve_variable(name);
    }
    return nullptr;
  }

  bool is_implicitely_convertible(const Type &from_type, const Type &to_type) const
  {
    return implicit_conversions_.contains({&from_type, &to_type});
  }

 private:
  Vector<const Function *> find_best_function_candidates(StringRef name,
                                                         const FunctionArgs &args,
                                                         const int max_suitability = INT_MAX) const
  {
    int best_suitability = -1;
    Vector<const Function *> best_functions;
    Span<std::unique_ptr<Function>> functions_with_name = functions_.lookup_as(name);
    for (const std::unique_ptr<Function> &function : functions_with_name) {
      const int suitability = this->compute_function_suitability(*function, args);
      if (suitability == -1) {
        continue;
      }
      if (suitability > max_suitability) {
        continue;
      }
      if (suitability == best_suitability) {
        best_functions.append(function.get());
      }
      if (suitability < best_suitability) {
        best_functions.clear();
        best_functions.append(function.get());
        best_suitability = suitability;
      }
    }

    if (best_suitability == 0) {
      return best_functions;
    }
    if (parent_ != nullptr) {
      Vector<const Function *> parent_functions = this->find_best_function_candidates(
          name, args, best_suitability - 1);
      if (!parent_functions.is_empty()) {
        return parent_functions;
      }
    }
    return best_functions;
  }

  /* Return -1 when the function cannot be used. Otherwise lower values are better. */
  int compute_function_suitability(const Function &function, const FunctionArgs &args) const
  {
    const int total_args = args.positional_args.size() + args.keyword_args.size();
    Span<Parameter> parameters = function.parameters();
    if (total_args > parameters.size()) {
      return -1;
    }

    Vector<int> used_parameter_indices;

    int conversion_count = 0;
    for (const int arg_index : args.positional_args.index_range()) {
      const Type *parameter_type = parameters[arg_index].type;
      const Type *arg_type = args.positional_args[arg_index];
      if (parameter_type != arg_type) {
        if (this->is_implicitely_convertible(*arg_type, *parameter_type)) {
          conversion_count++;
        }
        else {
          return -1;
        }
      }
      used_parameter_indices.append(arg_index);
    }

    for (auto &&[arg_name, arg_type] : args.keyword_args.items()) {
      bool found_matching_parameter = false;
      for (const int parameter_index : parameters.index_range()) {
        const Parameter &parameter = parameters[parameter_index];
        if (parameter.name == arg_name) {
          if (used_parameter_indices.contains(parameter_index)) {
            return -1;
          }
          if (parameter.type != arg_type) {
            if (this->is_implicitely_convertible(*arg_type, *parameter.type)) {
              conversion_count++;
            }
            else {
              return -1;
            }
          }
          found_matching_parameter = true;
          used_parameter_indices.append(parameter_index);
          break;
        }
      }
      if (!found_matching_parameter) {
        return -1;
      }
    }

    return conversion_count;
  }
};

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
  AssignmentStmt,
  IfStmt,
  GroupStmt,
  ExpressionStmt,
  DeclarationStmt,
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
