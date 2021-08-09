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

/** \file
 * \ingroup bke
 */

#include "BKE_asset_library.hh"

#include "MEM_guardedalloc.h"

#include <memory>

/* TODO(@sybren): revisit after D12117 has a conclusion. */
namespace filesystem = std::filesystem;
using namespace blender::bke;

AssetLibrary *BKE_asset_library_load(const char *library_path)
{
  AssetLibrary *lib = new AssetLibrary();
  lib->load(library_path);
  return lib;
}

void BKE_asset_library_free(AssetLibrary *asset_library)
{
  delete asset_library;
}

void AssetLibrary::load(const filesystem::path &library_root_directory)
{
  auto catalog_service = std::make_unique<AssetCatalogService>();
  catalog_service->load_from_disk(library_root_directory);
  this->catalog_service = std::move(catalog_service);
}
