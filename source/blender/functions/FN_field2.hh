/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

#include "BLI_cpp_type.hh"
#include "BLI_map.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_vector.hh"

#include "FN_lazy_function.hh"

namespace blender::fn::fields {

class GField;
class FieldFunction;
class FieldNode;

namespace eval_graph {

struct Node {
  FieldFunction *function;
  void *data;
  bool is_multi_function = false;
};

struct Socket {
  Node *node;
  int index;
  bool is_input;
};

struct InputSocket {
  Node *node;
  int index;
};

struct OutputSocket {
  Node *node;
  int index;
};

struct EvalGraph {
  Vector<Node *> nodes;
  MultiValueMap<InputSocket, OutputSocket> targets_map;
  Map<OutputSocket, InputSocket> origins_map;
};

class Builder {
 private:
  EvalGraph &eval_graph_;

 public:
  OutputSocket context_socket() const;
  void set_input(int index, InputSocket socket, OutputSocket context_socket);
  void set_output(int index, OutputSocket socket);
};

}  // namespace eval_graph

namespace array_eval {

class EvaluatorParams {
 public:
};

class Evaluator {
 public:
  void evaluate(EvaluatorParams &params) const;
};

Evaluator build_evaluator(Span<GField> fields);

}  // namespace array_eval

class GField {
 private:
  std::shared_ptr<const FieldNode> node_;
  int output_index_;
};

class FieldFunction {
 public:
  virtual void build_eval_graph(eval_graph::Builder &builder) const = 0;
  virtual const CPPType &array_eval_input_base_type(const int index) const = 0;
  virtual const CPPType &array_eval_output_base_type(const int index) const = 0;
};

class FieldNode {
 private:
  Vector<GField> inputs_;
  std::unique_ptr<FieldFunction> function_;
};

}  // namespace blender::fn::fields
