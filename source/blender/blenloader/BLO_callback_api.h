#ifndef __BLO_CALLBACK_API_H__
#define __BLO_CALLBACK_API_H__

#include "BLI_endian_switch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BloWriter BloWriter;
typedef struct BloReader BloReader;

/* API for file writing.
 **********************************************/

void BLO_write_raw(BloWriter *writer, int size_in_bytes, const void *data_ptr);

void BLO_write_struct_by_name(BloWriter *writer, const char *struct_name, const void *data_ptr);
void BLO_write_struct_array_by_name(BloWriter *writer,
                                    const char *struct_name,
                                    int array_size,
                                    const void *data_ptr);
void BLO_write_struct_by_id(BloWriter *writer, int struct_id, const void *data_ptr);
void BLO_write_struct_array_by_id(BloWriter *writer,
                                  int struct_id,
                                  int array_size,
                                  const void *data_ptr);

int BLO_get_struct_id_by_name(BloWriter *writer, const char *struct_name);
#define BLO_get_struct_id(writer, struct_name) BLO_get_struct_id_by_name(writer, #struct_name)

#define BLO_write_struct(writer, struct_name, data_ptr) \
  BLO_write_struct_by_id(writer, BLO_get_struct_id(writer, struct_name), data_ptr)
#define BLO_write_struct_array(writer, struct_name, array_size, data_ptr) \
  BLO_write_struct_array_by_id( \
      writer, BLO_get_struct_id(writer, struct_name), array_size, data_ptr)

void BLO_write_int32_array(BloWriter *writer, int size, const int32_t *data_ptr);
void BLO_write_uint32_array(BloWriter *writer, int size, const uint32_t *data_ptr);
void BLO_write_float_array(BloWriter *writer, int size, const float *data_ptr);
void BLO_write_float3_array(BloWriter *writer, int size, const float *data_ptr);

/* API for file reading.
 **********************************************/

void *BLO_read_get_new_data_address(BloReader *reader, const void *old_address);
bool BLO_read_requires_endian_switch(BloReader *reader);
#define BLO_read_data_address(reader, ptr) ptr = BLO_read_get_new_data_address(reader, ptr)

typedef void (*BloReadListFn)(BloReader *reader, void *data);
void BLO_read_list(BloReader *reader, struct ListBase *list, BloReadListFn callback);

#define BLO_read_int32_array(reader, array_size, ptr) \
  BLO_read_data_address(reader, ptr); \
  if (BLO_read_requires_endian_switch(reader)) { \
    BLI_endian_switch_int32_array((int32_t *)ptr, array_size); \
  }

#define BLO_read_uint32_array(reader, array_size, ptr) \
  BLO_read_data_address(reader, ptr); \
  if (BLO_read_requires_endian_switch(reader)) { \
    BLI_endian_switch_uint32_array((uint32_t *)ptr, array_size); \
  }

#define BLO_read_float_array(reader, array_size, ptr) \
  BLO_read_data_address(reader, ptr); \
  if (BLO_read_requires_endian_switch(reader)) { \
    BLI_endian_switch_float_array((float *)ptr, array_size); \
  }

#define BLO_read_float3_array(reader, array_size, ptr) \
  BLO_read_float_array(reader, 3 * (int)(array_size), ptr)

#ifdef __cplusplus
}
#endif

#endif /* __BLO_CALLBACK_API_H__ */
