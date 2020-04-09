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
typedef struct BlendReader BlendReader;

typedef struct BlendExpander {
  struct BlendReader *reader;
  struct Main *main;
} BlendExpander;

/* API for file writing.
 **********************************************/

void BLO_write_raw(BlendWriter *writer, int size_in_bytes, const void *data_ptr);

void BLO_write_struct_by_name(BlendWriter *writer, const char *struct_name, const void *data_ptr);
void BLO_write_struct_array_by_name(BlendWriter *writer,
                                    const char *struct_name,
                                    int array_size,
                                    const void *data_ptr);
void BLO_write_struct_by_id(BlendWriter *writer, int struct_id, const void *data_ptr);
void BLO_write_struct_array_by_id(BlendWriter *writer,
                                  int struct_id,
                                  int array_size,
                                  const void *data_ptr);
void BLO_write_struct_list_by_id(BlendWriter *writer, int struct_id, struct ListBase *list);

int BLO_get_struct_id_by_name(BlendWriter *writer, const char *struct_name);
#define BLO_get_struct_id(writer, struct_name) BLO_get_struct_id_by_name(writer, #struct_name)

#define BLO_write_struct(writer, struct_name, data_ptr) \
  BLO_write_struct_by_id(writer, BLO_get_struct_id(writer, struct_name), data_ptr)
#define BLO_write_struct_array(writer, struct_name, array_size, data_ptr) \
  BLO_write_struct_array_by_id( \
      writer, BLO_get_struct_id(writer, struct_name), array_size, data_ptr)
#define BLO_write_struct_list(writer, struct_name, list_ptr) \
  BLO_write_struct_list_by_id(writer, BLO_get_struct_id(writer, struct_name), list_ptr)

void BLO_write_int32_array(BlendWriter *writer, int size, const int32_t *data_ptr);
void BLO_write_uint32_array(BlendWriter *writer, int size, const uint32_t *data_ptr);
void BLO_write_float_array(BlendWriter *writer, int size, const float *data_ptr);
void BLO_write_float3_array(BlendWriter *writer, int size, const float *data_ptr);

/* API for file reading.
 **********************************************/

void *BLO_read_get_new_data_address(BlendReader *reader, const void *old_address);
ID *BLO_read_get_new_id_address(BlendReader *reader, struct Library *lib, struct ID *id);
bool BLO_read_requires_endian_switch(BlendReader *reader);

#define BLO_read_data_address(reader, ptr_p) \
  *(ptr_p) = BLO_read_get_new_data_address((reader), *(ptr_p))

#define BLO_read_id_address(reader, lib, id_ptr_p) \
  *(id_ptr_p) = (void *)BLO_read_get_new_id_address((reader), (lib), (ID *)*(id_ptr_p))

typedef void (*BlendReadListFn)(BlendReader *reader, void *data);
void BLO_read_list(BlendReader *reader, struct ListBase *list, BlendReadListFn callback);

void BLO_read_int32_array(BlendReader *reader, int array_size, int32_t **ptr_p);
void BLO_read_uint32_array(BlendReader *reader, int array_size, uint32_t **ptr_p);
void BLO_read_float_array(BlendReader *reader, int array_size, float **ptr_p);
void BLO_read_float3_array(BlendReader *reader, int array_size, float **ptr_p);
void BLO_read_double_array(BlendReader *reader, int array_size, double **ptr_p);
void BLO_read_pointer_array(BlendReader *reader, void **ptr_p);

void BLO_expand_id(BlendExpander *expander, ID *id);

#define BLO_expand(expander, id) BLO_expand_id(expander, (ID *)id)

#ifdef __cplusplus
}
#endif

#endif /* __BLO_READ_WRITE_H__ */
