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

#include "BKE_asset_catalog.hh"

#include "BLI_string_ref.hh"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace blender::bke {

const char AssetCatalogService::PATH_SEPARATOR = '/';
const CatalogFilePath AssetCatalogService::DEFAULT_CATALOG_FILENAME = "blender_assets.cats.txt";

AssetCatalogService::AssetCatalogService(const CatalogFilePath &asset_library_root)
    : asset_library_root_(asset_library_root)
{
}

bool AssetCatalogService::is_empty() const
{
  return catalogs_.is_empty();
}

AssetCatalog *AssetCatalogService::find_catalog(const CatalogID &catalog_id)
{
  std::unique_ptr<AssetCatalog> *catalog_uptr_ptr = this->catalogs_.lookup_ptr(catalog_id);
  if (catalog_uptr_ptr == nullptr) {
    return nullptr;
  }
  return catalog_uptr_ptr->get();
}

AssetCatalog *AssetCatalogService::create_catalog(const CatalogPath &catalog_path)
{
  std::unique_ptr<AssetCatalog> catalog = AssetCatalog::from_path(catalog_path);

  /* So we can std::move(catalog) and still use the non-owning pointer: */
  AssetCatalog *const catalog_ptr = catalog.get();

  /* TODO(@sybren): move the `AssetCatalog::from_path()` function to another place, that can reuse
   * catalogs when a catalog with the given path is already known, and avoid duplicate catalog IDs.
   */
  BLI_assert_msg(!catalogs_.contains(catalog->catalog_id), "duplicate catalog ID not supported");
  catalogs_.add_new(catalog->catalog_id, std::move(catalog));

  /* Ensure the new catalog gets written to disk. */
  this->ensure_asset_library_root();
  this->ensure_catalog_definition_file();
  catalog_definition_file_->add_new(catalog_ptr);
  catalog_definition_file_->write_to_disk();

  return catalog_ptr;
}

void AssetCatalogService::ensure_catalog_definition_file()
{
  if (catalog_definition_file_) {
    return;
  }

  auto cdf = std::make_unique<AssetCatalogDefinitionFile>();
  cdf->file_path = asset_library_root_ / DEFAULT_CATALOG_FILENAME;
  catalog_definition_file_ = std::move(cdf);
}

bool AssetCatalogService::ensure_asset_library_root()
{
  /* TODO(@sybren): design a way to get such errors presented to users (or ensure that they never
   * occur). */
  if (asset_library_root_.empty()) {
    std::cerr
        << "AssetCatalogService: no asset library root configured, unable to ensure it exists."
        << std::endl;
    return false;
  }

  if (fs::exists(asset_library_root_)) {
    if (!fs::is_directory(asset_library_root_)) {
      std::cerr << "AssetCatalogService: " << asset_library_root_
                << " exists but is not a directory, this is not a supported situation."
                << std::endl;
      return false;
    }

    /* Root directory exists, work is done. */
    return true;
  }

  /* Ensure the root directory exists. */
  std::error_code err_code;
  if (!fs::create_directories(asset_library_root_, err_code)) {
    std::cerr << "AssetCatalogService: error creating directory " << asset_library_root_ << ": "
              << err_code << std::endl;
    return false;
  }

  /* Root directory has been created, work is done. */
  return true;
}

void AssetCatalogService::load_from_disk()
{
  load_from_disk(asset_library_root_);
}

void AssetCatalogService::load_from_disk(const CatalogFilePath &file_or_directory_path)
{
  fs::file_status status = fs::status(file_or_directory_path);
  switch (status.type()) {
    case fs::file_type::regular:
      load_single_file(file_or_directory_path);
      break;
    case fs::file_type::directory:
      load_directory_recursive(file_or_directory_path);
      break;
    default:
      // TODO(@sybren): throw an appropriate exception.
      return;
  }
}

void AssetCatalogService::load_directory_recursive(const CatalogFilePath &directory_path)
{
  // TODO(@sybren): implement proper multi-file support. For now, just load
  // the default file if it is there.
  CatalogFilePath file_path = directory_path / DEFAULT_CATALOG_FILENAME;
  fs::file_status fs_status = fs::status(file_path);

  if (!fs::exists(fs_status)) {
    /* No file to be loaded is perfectly fine. */
    return;
  }
  this->load_single_file(file_path);
}

void AssetCatalogService::load_single_file(const CatalogFilePath &catalog_definition_file_path)
{
  /* TODO(@sybren): check that #catalog_definition_file_path is contained in #asset_library_root_,
   * otherwise some assumptions may fail. */
  std::unique_ptr<AssetCatalogDefinitionFile> cdf = parse_catalog_file(
      catalog_definition_file_path);

  BLI_assert_msg(!this->catalog_definition_file_,
                 "Only loading of a single catalog definition file is supported.");
  this->catalog_definition_file_ = std::move(cdf);
}

std::unique_ptr<AssetCatalogDefinitionFile> AssetCatalogService::parse_catalog_file(
    const CatalogFilePath &catalog_definition_file_path)
{
  auto cdf = std::make_unique<AssetCatalogDefinitionFile>();
  cdf->file_path = catalog_definition_file_path;

  std::fstream infile(catalog_definition_file_path);
  std::string line;
  while (std::getline(infile, line)) {
    const StringRef trimmed_line = StringRef(line).trim();
    if (trimmed_line.is_empty() || trimmed_line[0] == '#') {
      continue;
    }

    std::unique_ptr<AssetCatalog> catalog = this->parse_catalog_line(trimmed_line, cdf.get());
    if (!catalog) {
      continue;
    }

    if (cdf->contains(catalog->catalog_id)) {
      std::cerr << catalog_definition_file_path << ": multiple definitions of catalog "
                << catalog->catalog_id << " in the same file, using first occurrence."
                << std::endl;
      /* Don't store 'catalog'; unique_ptr will free its memory. */
      continue;
    }

    if (this->catalogs_.contains(catalog->catalog_id)) {
      // TODO(@sybren): apparently another CDF was already loaded. This is not supported yet.
      std::cerr << catalog_definition_file_path << ": multiple definitions of catalog "
                << catalog->catalog_id << " in multiple files, ignoring this one." << std::endl;
      /* Don't store 'catalog'; unique_ptr will free its memory. */
      continue;
    }

    /* The AssetDefinitionFile should include this catalog when writing it back to disk. */
    cdf->add_new(catalog.get());

    /* The AssetCatalog pointer is owned by the AssetCatalogService. */
    this->catalogs_.add_new(catalog->catalog_id, std::move(catalog));
  }

  return cdf;
}

std::unique_ptr<AssetCatalog> AssetCatalogService::parse_catalog_line(
    const StringRef line, const AssetCatalogDefinitionFile *catalog_definition_file)
{
  const int64_t first_space = line.find_first_of(' ');
  if (first_space == StringRef::not_found) {
    std::cerr << "Invalid line in " << catalog_definition_file->file_path << ": " << line
              << std::endl;
    return std::unique_ptr<AssetCatalog>(nullptr);
  }

  const StringRef catalog_id = line.substr(0, first_space);
  const CatalogPath catalog_path = AssetCatalog::cleanup_path(line.substr(first_space + 1));

  return std::make_unique<AssetCatalog>(catalog_id, catalog_path);
}

AssetCatalogDefinitionFile *AssetCatalogService::get_catalog_definition_file()
{
  return catalog_definition_file_.get();
}

bool AssetCatalogDefinitionFile::contains(const CatalogID &catalog_id) const
{
  return catalogs_.contains(catalog_id);
}

void AssetCatalogDefinitionFile::add_new(AssetCatalog *catalog)
{
  catalogs_.add_new(catalog->catalog_id, catalog);
}

void AssetCatalogDefinitionFile::write_to_disk() const
{
  this->write_to_disk(this->file_path);
}

void AssetCatalogDefinitionFile::write_to_disk(const CatalogFilePath &file_path) const
{
  // TODO(@sybren): create a backup of the original file, if it exists.
  std::ofstream output(file_path);

  // TODO(@sybren): remember the line ending style that was originally read, then use that to write
  // the file again.

  // Write the header.
  // TODO(@sybren): move the header definition to some other place.
  output << "# This is an Asset Catalog Definition file for Blender." << std::endl;
  output << "#" << std::endl;
  output << "# Empty lines and lines starting with `#` will be ignored." << std::endl;
  output << "# Other lines are of the format \"CATALOG_ID /catalog/path/for/assets\"" << std::endl;
  output << "" << std::endl;

  // Write the catalogs.
  // TODO(@sybren): order them by Catalog ID or Catalog Path.
  for (const auto &catalog : catalogs_.values()) {
    output << catalog->catalog_id << " " << catalog->path << std::endl;
  }
}

AssetCatalog::AssetCatalog(const CatalogID &catalog_id, const CatalogPath &path)
    : catalog_id(catalog_id), path(path)
{
}

std::unique_ptr<AssetCatalog> AssetCatalog::from_path(const CatalogPath &path)
{
  const CatalogPath clean_path = cleanup_path(path);
  const CatalogID cat_id = sensible_id_for_path(clean_path);
  auto catalog = std::make_unique<AssetCatalog>(cat_id, clean_path);
  return catalog;
}

CatalogID AssetCatalog::sensible_id_for_path(const CatalogPath &path)
{
  CatalogID cat_id = path;
  std::replace(cat_id.begin(), cat_id.end(), AssetCatalogService::PATH_SEPARATOR, '-');
  std::replace(cat_id.begin(), cat_id.end(), ' ', '-');
  return cat_id;
}

CatalogPath AssetCatalog::cleanup_path(const CatalogPath &path)
{
  /* TODO(@sybren): maybe go over each element of the path, and trim those? */
  CatalogPath clean_path = StringRef(path).trim().trim(AssetCatalogService::PATH_SEPARATOR).trim();
  return clean_path;
}

}  // namespace blender::bke
