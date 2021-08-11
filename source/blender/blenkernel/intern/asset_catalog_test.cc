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

#include "BLI_fileops.h"

#include "testing/testing.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace blender::bke::tests {

class AssetCatalogTest : public testing::Test {
 protected:
  CatalogFilePath asset_library_root_;
  CatalogFilePath temp_library_path_;

  void SetUp() override
  {
    const fs::path test_files_dir = blender::tests::flags_test_asset_dir();
    if (test_files_dir.empty()) {
      FAIL();
    }

    asset_library_root_ = test_files_dir / "asset_library";
    temp_library_path_ = "";
  }

  /* Register a temporary path, which will be removed at the end of the test. */
  CatalogFilePath use_temp_path()
  {
    const CatalogFilePath tempdir = BKE_tempdir_session();
    temp_library_path_ = tempdir / "test-temporary-path";
    return temp_library_path_;
  }

  void TearDown() override
  {
    if (!temp_library_path_.empty()) {
      fs::remove_all(temp_library_path_);
      temp_library_path_ = "";
    }
  }
};

TEST_F(AssetCatalogTest, load_single_file)
{
  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ / "blender_assets.cats.txt");

  // Test getting a non-existant catalog ID.
  EXPECT_EQ(nullptr, service.find_catalog("NONEXISTANT"));

  // Test getting an invalid catalog (without path definition).
  EXPECT_EQ(nullptr, service.find_catalog("ID_WITHOUT_PATH"));

  // Test getting a 7-bit ASCII catalog ID.
  AssetCatalog *poses_elly = service.find_catalog("POSES_ELLY");
  ASSERT_NE(nullptr, poses_elly);
  EXPECT_EQ("POSES_ELLY", poses_elly->catalog_id);
  EXPECT_EQ("character/Elly/poselib", poses_elly->path);

  // Test whitespace stripping and support in the path.
  AssetCatalog *poses_whitespace = service.find_catalog("POSES_ELLY_WHITESPACE");
  ASSERT_NE(nullptr, poses_whitespace);
  EXPECT_EQ("POSES_ELLY_WHITESPACE", poses_whitespace->catalog_id);
  EXPECT_EQ("character/Elly/poselib/white space", poses_whitespace->path);

  // Test getting a UTF-8 catalog ID.
  AssetCatalog *poses_ruzena = service.find_catalog("POSES_RUŽENA");
  ASSERT_NE(nullptr, poses_ruzena);
  EXPECT_EQ("POSES_RUŽENA", poses_ruzena->catalog_id);
  EXPECT_EQ("character/Ružena/poselib", poses_ruzena->path);
}

TEST_F(AssetCatalogTest, write_single_file)
{
  AssetCatalogService service(asset_library_root_);
  service.load_from_disk(asset_library_root_ / "blender_assets.cats.txt");

  const CatalogFilePath save_to_path = use_temp_path();
  AssetCatalogDefinitionFile *cdf = service.get_catalog_definition_file();
  cdf->write_to_disk(save_to_path);

  AssetCatalogService loaded_service(save_to_path);
  loaded_service.load_from_disk();

  // Test that the expected catalogs are there.
  EXPECT_NE(nullptr, loaded_service.find_catalog("POSES_ELLY"));
  EXPECT_NE(nullptr, loaded_service.find_catalog("POSES_ELLY_WHITESPACE"));
  EXPECT_NE(nullptr, loaded_service.find_catalog("POSES_ELLY_TRAILING_SLASH"));
  EXPECT_NE(nullptr, loaded_service.find_catalog("POSES_RUŽENA"));
  EXPECT_NE(nullptr, loaded_service.find_catalog("POSES_RUŽENA_HAND"));
  EXPECT_NE(nullptr, loaded_service.find_catalog("POSES_RUŽENA_FACE"));

  // Test that the invalid catalog definition wasn't copied.
  EXPECT_EQ(nullptr, loaded_service.find_catalog("ID_WITHOUT_PATH"));

  // TODO(@sybren): test ordering of catalogs in the file.
}

TEST_F(AssetCatalogTest, create_first_catalog_from_scratch)
{
  /* Even from scratch a root directory should be known. */
  const CatalogFilePath temp_lib_root = use_temp_path();
  AssetCatalogService service(temp_lib_root);

  /* Just creating the service should NOT create the path. */
  EXPECT_FALSE(fs::exists(temp_lib_root));

  AssetCatalog *cat = service.create_catalog("some/catalog/path");
  ASSERT_NE(nullptr, cat);
  EXPECT_EQ(cat->path, "some/catalog/path");
  EXPECT_EQ(cat->catalog_id, "some-catalog-path");

  /* Creating a new catalog should create the directory + the default file. */
  EXPECT_TRUE(fs::is_directory(temp_lib_root));

  const CatalogFilePath definition_file_path = temp_lib_root /
                                               AssetCatalogService::DEFAULT_CATALOG_FILENAME;
  EXPECT_TRUE(fs::is_regular_file(definition_file_path));

  AssetCatalogService loaded_service(temp_lib_root);
  loaded_service.load_from_disk();

  // Test that the expected catalog is there.
  AssetCatalog *written_cat = loaded_service.find_catalog(cat->catalog_id);
  ASSERT_NE(nullptr, written_cat);
  EXPECT_EQ(written_cat->catalog_id, cat->catalog_id);
  EXPECT_EQ(written_cat->path, cat->path);
}

TEST_F(AssetCatalogTest, create_catalog_after_loading_file)
{
  const CatalogFilePath temp_lib_root = use_temp_path();

  /* Copy the asset catalog definition files to a separate location, so that we can test without
   * overwriting the test file in SVN. */
  fs::copy(asset_library_root_, temp_lib_root, fs::copy_options::recursive);

  AssetCatalogService service(temp_lib_root);
  service.load_from_disk();
  EXPECT_NE(nullptr, service.find_catalog("POSES_ELLY")) << "expected catalogs to be loaded";
  EXPECT_EQ(nullptr, service.find_catalog("new-catalog"))
      << "not expecting catalog that's only added in this test";

  /* This should create a new catalog and write to disk. */
  service.create_catalog("new/catalog");

  /* Reload the written catalog files. */
  AssetCatalogService loaded_service(temp_lib_root);
  loaded_service.load_from_disk();

  EXPECT_NE(nullptr, service.find_catalog("POSES_ELLY"))
      << "expected pre-existing catalogs to be kept in the file";
  EXPECT_NE(nullptr, service.find_catalog("new-catalog"))
      << "expecting newly added catalog to exist in the file";
}

TEST_F(AssetCatalogTest, create_catalog_path_cleanup)
{
  const CatalogFilePath temp_lib_root = use_temp_path();
  AssetCatalogService service(temp_lib_root);
  AssetCatalog *cat = service.create_catalog(" /some/path  /  ");

  EXPECT_EQ("some-path", cat->catalog_id);
  EXPECT_EQ("some/path", cat->path);
}

}  // namespace blender::bke::tests
