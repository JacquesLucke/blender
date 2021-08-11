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
  static const char PATH_SEPARATOR;
  static const CatalogFilePath DEFAULT_CATALOG_FILENAME;

 public:
  explicit AssetCatalogService(const CatalogFilePath &asset_library_root);

  /** Load asset catalog definitions from the files found in the asset library. */
  void load_from_disk();
  /** Load asset catalog definitions from the given file or directory. */
  void load_from_disk(const CatalogFilePath &file_path);

  /** Return catalog with the given ID. Return nullptr if not found. */
  AssetCatalog *find_catalog(const CatalogID &catalog_id);

  /** Create a catalog with some sensible auto-generated catalog ID.
   * The catalog will be saved to the default catalog file.*/
  AssetCatalog *create_catalog(const CatalogPath &catalog_path);

  /** For testing only, get the loaded catalog definition file. */
  AssetCatalogDefinitionFile *get_catalog_definition_file();

  /** Return true iff there are no catalogs known. */
  bool is_empty() const;

 protected:
  /* These pointers are owned by this AssetCatalogService. */
  Map<CatalogID, std::unique_ptr<AssetCatalog>> catalogs_;
  std::unique_ptr<AssetCatalogDefinitionFile> catalog_definition_file_;
  CatalogFilePath asset_library_root_;

  void load_directory_recursive(const CatalogFilePath &directory_path);
  void load_single_file(const CatalogFilePath &catalog_definition_file_path);

  std::unique_ptr<AssetCatalogDefinitionFile> parse_catalog_file(
      const CatalogFilePath &catalog_definition_file_path);

  std::unique_ptr<AssetCatalog> parse_catalog_line(
      StringRef line, const AssetCatalogDefinitionFile *catalog_definition_file);

  /**
   * Ensure that an #AssetCatalogDefinitionFile exists in memory.
   * This is used when no such file has been loaded, and a new catalog is to be created. */
  void ensure_catalog_definition_file();

  /**
   * Ensure the asset library root directory exists, so that it can be written to.
   * TODO(@sybren): this might move to the #AssetLibrary class instead, and just assumed to exist
   * in this class. */
  bool ensure_asset_library_root();
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

  /** Create a new Catalog with the given path, auto-generating a sensible catalog ID. */
  static std::unique_ptr<AssetCatalog> from_path(const CatalogPath &path);
  static CatalogPath cleanup_path(const CatalogPath &path);

 protected:
  /** Generate a sensible catalog ID for the given path. */
  static CatalogID sensible_id_for_path(const CatalogPath &path);
};

}  // namespace blender::bke
