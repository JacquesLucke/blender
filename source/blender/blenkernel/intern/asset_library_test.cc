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
 * The Original Code is Copyright (C) 2020 Blender Foundation
 * All rights reserved.
 */

#include "BKE_appdir.h"
#include "BKE_asset_catalog.hh"
#include "BKE_asset_library.hh"

#include "testing/testing.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace blender::bke::tests {

TEST(AssetLibraryTest, load_and_free_c_functions)
{
  const fs::path test_files_dir = blender::tests::flags_test_asset_dir();
  if (test_files_dir.empty()) {
    FAIL();
  }

  /* Load the asset library. */
  const fs::path library_path = test_files_dir / "asset_library";
  ::AssetLibrary *library_c_ptr = BKE_asset_library_load(library_path.c_str());
  ASSERT_NE(nullptr, library_c_ptr);

  /* Check that it can be cast to the C++ type and has a Catalog Service. */
  blender::bke::AssetLibrary *library_cpp_ptr = reinterpret_cast<blender::bke::AssetLibrary *>(
      library_c_ptr);
  AssetCatalogService *service = library_cpp_ptr->catalog_service.get();
  ASSERT_NE(nullptr, service);

  /* Check that the catalogs defined in the library are actually loaded. This just tests one single
   * catalog, as that indicates the file has been loaded. Testing that that loading went OK is for
   * the asset catalog service tests. */
  AssetCatalog *poses_elly = service->find_catalog("POSES_ELLY");
  ASSERT_NE(nullptr, poses_elly) << "unable to find POSES_ELLY catalog";
  EXPECT_EQ("character/Elly/poselib", poses_elly->path);

  BKE_asset_library_free(library_c_ptr);
}

TEST(AssetLibraryTest, load_nonexistent_directory)
{
  const fs::path test_files_dir = blender::tests::flags_test_asset_dir();
  if (test_files_dir.empty()) {
    FAIL();
  }

  /* Load the asset library. */
  const fs::path library_path = test_files_dir / "asset_library/this/subdir/does/not/exist";
  ::AssetLibrary *library_c_ptr = BKE_asset_library_load(library_path.c_str());
  ASSERT_NE(nullptr, library_c_ptr);

  /* Check that it can be cast to the C++ type and has a Catalog Service. */
  blender::bke::AssetLibrary *library_cpp_ptr = reinterpret_cast<blender::bke::AssetLibrary *>(
      library_c_ptr);
  AssetCatalogService *service = library_cpp_ptr->catalog_service.get();
  ASSERT_NE(nullptr, service);

  /* Check that the catalog service doesn't have any catalogs. */
  EXPECT_TRUE(service->catalogs.is_empty());

  BKE_asset_library_free(library_c_ptr);
}

}  // namespace blender::bke::tests
