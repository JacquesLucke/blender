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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

#ifndef __BLO_WRITEFILE_H__
#define __BLO_WRITEFILE_H__

/** \file
 * \ingroup blenloader
 * \brief external writefile function prototypes.
 */

struct BlendThumbnail;
struct Main;
struct MemFile;
struct ReportList;

extern bool BLO_write_file(struct Main *mainvar,
                           const char *filepath,
                           int write_flags,
                           struct ReportList *reports,
                           const struct BlendThumbnail *thumb);
extern bool BLO_write_file_mem(struct Main *mainvar,
                               struct MemFile *compare,
                               struct MemFile *current,
                               int write_flags);

typedef struct BloWriter BloWriter;

void BLO_write_data(BloWriter *writer, const void *data_ptr, int length);
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
#define BLO_get_struct_id(writer, struct_name) BLO_get_struct_id_by_name(writer, ##struct_name)

#define BLO_write_struct(writer, struct_name, data_ptr) \
  BLO_write_struct_by_id(writer, BLO_get_struct_id(struct_name), data_ptr)
#define BLO_write_struct_array(writer, struct_name, data_ptr, array_size) \
  BLO_write_struct_array_by_id(writer, BLO_get_struct_id(struct_name), data_ptr, array_size)

#endif
