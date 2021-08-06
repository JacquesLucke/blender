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

#pragma once

#ifndef __cplusplus
#  error This is a C++-only header file.
#endif

#include "BKE_asset_catalog.hh"
#include "BKE_asset_library.h"

#include <filesystem>
#include <memory>

namespace blender::bke {

namespace fs = std::filesystem;

struct AssetLibrary {
  std::unique_ptr<AssetCatalogService> catalog_service;

  void load(const fs::path &library_root_directory);
};

}  // namespace blender::bke
