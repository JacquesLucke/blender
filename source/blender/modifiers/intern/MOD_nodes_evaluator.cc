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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "MOD_nodes_evaluator.hh"

#include "BLI_set.hh"
#include "BLI_stack.hh"

namespace blender::modifiers {

namespace {
struct SocketUsage {
  Vector<DInputSocket, 0> users;
};

class Evaluator {
 private:
  const GeometryNodesEvaluationParams &params_;
  Map<DSocket, SocketUsage> socket_usage_;

 public:
  Evaluator(const GeometryNodesEvaluationParams &params) : params_(params)
  {
  }

  void execute()
  {
    this->initialize_socket_usage();
    socket_usage_.print_stats();
  }

  void initialize_socket_usage()
  {
    SCOPED_TIMER(__func__);
    Stack<DInputSocket> sockets_to_check;
    Set<DNode> found_nodes;
    sockets_to_check.push_multiple(params_.output_sockets);

    while (!sockets_to_check.is_empty()) {
      const DInputSocket input_socket = sockets_to_check.pop();

      input_socket.foreach_origin_socket([&, this](const DSocket origin) {
        SocketUsage &usage = this->socket_usage_.lookup_or_add_default(origin);
        usage.users.append_non_duplicates(input_socket);
        if (origin->is_output()) {
          const DNode origin_node = origin.node();
          if (found_nodes.add(origin_node)) {
            for (const InputSocketRef *input_socket_ref : origin_node->inputs()) {
              sockets_to_check.push(DInputSocket(origin.context(), input_socket_ref));
            }
          }
        }
      });
    }
  }
};
}  // namespace

void evaluate_geometry_nodes(const GeometryNodesEvaluationParams &params)
{
  Evaluator evaluator{params};
  evaluator.execute();
}

}  // namespace blender::modifiers
