#pragma once

#include "FN_core.hpp"

struct bNodeTree;
struct bNodeSocket;

namespace FN { namespace Functions {

	SharedFunction float_socket_input(
		struct bNodeTree *btree,
		struct bNodeSocket *bsocket);

	SharedFunction vector_socket_input(
		struct bNodeTree *btree,
		struct bNodeSocket *bsocket);

} } /* namespace FN::Functions */