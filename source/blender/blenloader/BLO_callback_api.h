#ifndef __BLO_CALLBACK_API_H__
#define __BLO_CALLBACK_API_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BloWriter BloWriter;
typedef struct BloReader BloReader;

/* API for file writing.
 **********************************************/

void BLO_write_raw(BloWriter *writer, const void *data_ptr, int length);
void BLO_write_struct_by_name(BloWriter *writer, const char *struct_name, const void *data_ptr);
void BLO_write_struct_array_by_name(BloWriter *writer,
                                    const char *struct_name,
                                    const void *data_ptr,
                                    int array_size);
void BLO_write_struct_by_id(BloWriter *writer, int struct_id, const void *data_ptr);
void BLO_write_struct_array_by_id(BloWriter *writer,
                                  int struct_id,
                                  const void *data_ptr,
                                  int array_size);

int BLO_get_struct_id_by_name(BloWriter *writer, const char *struct_name);
#define BLO_get_struct_id(writer, struct_name) BLO_get_struct_id_by_name(writer, #struct_name)

#define BLO_write_struct(writer, struct_name, data_ptr) \
  BLO_write_struct_by_id(writer, BLO_get_struct_id(writer, struct_name), data_ptr)
#define BLO_write_struct_array(writer, struct_name, data_ptr, array_size) \
  BLO_write_struct_array_by_id( \
      writer, BLO_get_struct_id(writer, struct_name), data_ptr, array_size)

/* API for file reading.
 **********************************************/

void *BLO_read_new_address(BloReader *reader, const void *old_address);
bool BLO_read_requires_endian_switch(BloReader *reader);
#define BLO_read_update_address(reader, ptr) ptr = BLO_read_new_address(reader, ptr)

#ifdef __cplusplus
}
#endif

#endif /* __BLO_CALLBACK_API_H__ */
