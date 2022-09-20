/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_space_types.h"

struct BlendWriter;
struct BlendDataReader;
struct BlendLibReader;
struct LibraryForeachIDData;
struct Library;
struct IDRemapper;

#ifdef __cplusplus
extern "C" {
#endif

void BKE_viewer_path_init(ViewerPath *viewer_path);
void BKE_viewer_path_clear(ViewerPath *viewer_path);
void BKE_viewer_path_copy(ViewerPath *dst, const ViewerPath *src);
bool BKE_viewer_path_equal(const ViewerPath *a, const ViewerPath *b);
void BKE_viewer_path_blend_write(struct BlendWriter *writer, const ViewerPath *viewer_path);
void BKE_viewer_path_blend_read_data(struct BlendDataReader *reader, ViewerPath *viewer_path);
void BKE_viewer_path_blend_read_lib(struct BlendLibReader *reader,
                                    struct Library *lib,
                                    ViewerPath *viewer_path);
void BKE_viewer_path_foreach_id(struct LibraryForeachIDData *data, ViewerPath *viewer_path);
void BKE_viewer_path_remap_id(ViewerPath *viewer_path, const struct IDRemapper *mappings);

ViewerPathElem *BKE_viewer_path_elem_new(ViewerPathElemType type);
ViewerPathElem *BKE_viewer_path_elem_copy(const ViewerPathElem *src);
bool BKE_viewer_path_elem_equal(const ViewerPathElem *a, const ViewerPathElem *b);
void BKE_viewer_path_elem_free(ViewerPathElem *elem);

#ifdef __cplusplus
}
#endif
