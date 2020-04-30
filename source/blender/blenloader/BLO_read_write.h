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

#ifndef __BLO_READ_WRITE_H__
#define __BLO_READ_WRITE_H__

#include "BLI_endian_switch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BlendWriter BlendWriter;
typedef struct BlendDataReader BlendDataReader;
typedef struct BlendLibReader BlendLibReader;
typedef struct BlendExpander BlendExpander;

/* API for file writing.
 **********************************************/

void BLO_write_raw(BlendWriter *writer, int size_in_bytes, const void *data_ptr);

void BLO_write_struct_by_name(BlendWriter *writer, const char *struct_name, const void *data_ptr);
void BLO_write_struct_array_by_name(BlendWriter *writer,
                                    const char *struct_name,
                                    int array_size,
                                    const void *data_ptr);
void BLO_write_struct_by_id_at_address(BlendWriter *writer,
                                       int struct_data,
                                       const void *data_ptr,
                                       const void *address);

void BLO_write_struct_by_id(BlendWriter *writer, int struct_id, const void *data_ptr);
void BLO_write_struct_array_by_id(BlendWriter *writer,
                                  int struct_id,
                                  int array_size,
                                  const void *data_ptr);
void BLO_write_struct_list_by_id(BlendWriter *writer, int struct_id, struct ListBase *list);

void blo_write_id_struct(BlendWriter *writer,
                         int struct_id,
                         const void *id_address,
                         const struct ID *id);

int BLO_get_struct_id_by_name(BlendWriter *writer, const char *struct_name);
#define BLO_get_struct_id(writer, struct_name) BLO_get_struct_id_by_name(writer, #struct_name)

#define BLO_write_struct(writer, struct_name, data_ptr) \
  BLO_write_struct_by_id(writer, BLO_get_struct_id(writer, struct_name), data_ptr)
#define BLO_write_struct_array(writer, struct_name, array_size, data_ptr) \
  BLO_write_struct_array_by_id( \
      writer, BLO_get_struct_id(writer, struct_name), array_size, data_ptr)
#define BLO_write_struct_list(writer, struct_name, list_ptr) \
  BLO_write_struct_list_by_id(writer, BLO_get_struct_id(writer, struct_name), list_ptr)
#define BLO_write_id_struct(writer, struct_name, id_address, id) \
  blo_write_id_struct(writer, BLO_get_struct_id(writer, struct_name), id_address, id)

void BLO_write_int32_array(BlendWriter *writer, int size, const int32_t *data_ptr);
void BLO_write_uint32_array(BlendWriter *writer, int size, const uint32_t *data_ptr);
void BLO_write_float_array(BlendWriter *writer, int size, const float *data_ptr);
void BLO_write_float3_array(BlendWriter *writer, int size, const float *data_ptr);
void BLO_write_string(BlendWriter *writer, const char *str);

bool BLO_write_is_undo(BlendWriter *writer);

/* API for data pointer reading.
 **********************************************/

void *BLO_read_get_new_data_address(BlendDataReader *reader, const void *old_address);
bool BLO_read_requires_endian_switch(BlendDataReader *reader);

#define BLO_read_data_address(reader, ptr_p) \
  *(ptr_p) = BLO_read_get_new_data_address((reader), *(ptr_p))

typedef void (*BlendReadListFn)(BlendDataReader *reader, void *data);
void BLO_read_list(BlendDataReader *reader, struct ListBase *list, BlendReadListFn callback);

void BLO_read_int32_array(BlendDataReader *reader, int array_size, int32_t **ptr_p);
void BLO_read_uint32_array(BlendDataReader *reader, int array_size, uint32_t **ptr_p);
void BLO_read_float_array(BlendDataReader *reader, int array_size, float **ptr_p);
void BLO_read_float3_array(BlendDataReader *reader, int array_size, float **ptr_p);
void BLO_read_double_array(BlendDataReader *reader, int array_size, double **ptr_p);
void BLO_read_pointer_array(BlendDataReader *reader, void **ptr_p);

/* API for id pointer reading.
 ***********************************************/

ID *BLO_read_get_new_id_address(BlendLibReader *reader, struct Library *lib, struct ID *id);

#define BLO_read_id_address(reader, lib, id_ptr_p) \
  *(id_ptr_p) = (void *)BLO_read_get_new_id_address((reader), (lib), (ID *)*(id_ptr_p))

/* API for expand process.
 **********************************************/

void BLO_expand_id(BlendExpander *expander, struct ID *id);

#define BLO_expand(expander, id) BLO_expand_id(expander, (struct ID *)id)

#ifdef __cplusplus
}
#endif

#endif /* __BLO_READ_WRITE_H__ */
