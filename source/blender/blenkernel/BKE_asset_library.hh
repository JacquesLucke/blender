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
#  error This is a C++-only header file. Use BKE_asset_library.h instead.
#endif

#include "BKE_asset_library.h"

#include "BKE_asset_catalog.hh"

#include <filesystem>
#include <memory>

struct AssetLibrary {
  std::unique_ptr<blender::bke::AssetCatalogService> catalog_service;

  /* TODO(@sybren): revisit std::filesystem after D12117 has a conclusion. */
  void load(const std::filesystem::path &library_root_directory);
};
