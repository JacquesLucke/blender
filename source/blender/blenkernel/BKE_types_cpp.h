#ifndef __BKE_DATA_TYPES_H__
#define __BKE_DATA_TYPES_H__

#include "BKE_type_cpp.h"

namespace BKE {

void init_data_types();
void free_data_types();

template<typename T> TypeCPP &get_type_cpp();

}  // namespace BKE

#endif /* __BKE_DATA_TYPES_H__ */
