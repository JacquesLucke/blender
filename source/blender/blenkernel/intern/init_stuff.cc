#include "BKE_init_stuff.h"
#include "BKE_cpp_types.h"

void BKE_init_stuff(void)
{
  BKE::init_data_types();
}

void BKE_free_stuff(void)
{
  BKE::free_data_types();
}