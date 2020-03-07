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
#define BLO_write_raw_array(writer, element_size, length, data_ptr) \
  BLO_write_raw(writer, (element_size) * (length), data_ptr)

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

/* API for file reading.
 **********************************************/

void *BLO_read_new_address(BloReader *reader, const void *old_address);
bool BLO_read_requires_endian_switch(BloReader *reader);
#define BLO_read_update_address(reader, ptr) ptr = BLO_read_new_address(reader, ptr)

typedef void (*BloLinkListFn)(BloReader *reader, void *data);
void BLO_read_list(BloReader *reader, struct ListBase *list, BloLinkListFn callback);

#define BLO_read_array_endian_corrected(reader, type_name, ptr, array_size) \
  BLO_read_update_address(reader, ptr); \
  if (BLO_read_requires_endian_switch(reader)) { \
    BLI_endian_switch_##type_name##_array(ptr, array_size); \
  }

#define BLO_read_array_int32(reader, ptr, array_size) \
  BLO_read_array_endian_corrected(reader, int32, ptr, array_size)
#define BLO_read_array_float(reader, ptr, array_size) \
  BLO_read_array_endian_corrected(reader, float, ptr, array_size)
#define BLO_read_array_float3(reader, ptr, array_size) \
  BLO_read_array_float(reader, ptr, 3 * (array_size))

#ifdef __cplusplus
}
#endif

#endif /* __BLO_CALLBACK_API_H__ */
