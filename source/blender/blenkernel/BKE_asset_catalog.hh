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
#  error This is a C++ header. The C interface is yet to be implemented/designed.
#endif

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector_set.hh"

#include <filesystem>
#include <memory>
#include <string>

namespace blender::bke {

using CatalogID = std::string;
using CatalogPath = std::string;
using CatalogFilePath = std::filesystem::path;

class AssetCatalog;
class AssetCatalogDefinitionFile;

/* Manages the asset catalogs of a single asset library (i.e. of catalogs defined in a single
 * directory hierarchy). */
class AssetCatalogService {
 public:
  const char path_separator = '/';

 public:
  AssetCatalogService() = default;

  /* Return catalog with the given ID. Return nullptr if not found. */
  AssetCatalog *find_catalog(const CatalogID &catalog_id);

  void load_from_disk(const CatalogFilePath &asset_library_root);

  /* Get CDF for testing only. */
  AssetCatalogDefinitionFile *get_catalog_definition_file();

  /* Return true iff there are no catalogs known. */
  bool is_empty() const;

 protected:
  /* These pointers are owned by this AssetCatalogService. */
  Map<CatalogID, std::unique_ptr<AssetCatalog>> catalogs_;
  std::unique_ptr<AssetCatalogDefinitionFile> catalog_definition_file_;

  void load_directory_recursive(const CatalogFilePath &directory_path);
  void load_single_file(const CatalogFilePath &catalog_definition_file_path);

  std::unique_ptr<AssetCatalogDefinitionFile> parse_catalog_file(
      const CatalogFilePath &catalog_definition_file_path);

  std::unique_ptr<AssetCatalog> parse_catalog_line(
      StringRef line, const AssetCatalogDefinitionFile *catalog_definition_file);
};

/** Keeps track of which catalogs are defined in a certain file on disk.
 * Only contains non-owning pointers to the #AssetCatalog instances, so ensure the lifetime of this
 * class is shorter than that of the #`AssetCatalog`s themselves. */
class AssetCatalogDefinitionFile {
 public:
  CatalogFilePath file_path;

  AssetCatalogDefinitionFile() = default;

  /** Write the catalog definitions to the same file they were read from. */
  void write_to_disk() const;
  /** Write the catalog definitions to an arbitrary file path. */
  void write_to_disk(const CatalogFilePath &) const;

  bool contains(const CatalogID &catalog_id) const;
  /* Add a new catalog. Undefined behaviour if a catalog with the same ID was already added. */
  void add_new(AssetCatalog *catalog);

 protected:
  /* Catalogs stored in this file. They are mapped by ID to make it possible to query whether a
   * catalog is already known, without having to find the corresponding `AssetCatalog*`. */
  Map<CatalogID, AssetCatalog *> catalogs_;
};

/** Asset Catalog definition, containing a symbolic ID and a path that points to a node in the
 * catalog hierarchy. */
class AssetCatalog {
 public:
  AssetCatalog() = default;
  AssetCatalog(const CatalogID &catalog_id, const CatalogPath &path);

  CatalogID catalog_id;
  CatalogPath path;
};

}  // namespace blender::bke
