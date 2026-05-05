#pragma once

#include <optional>
#include <string>

#include "cppschema/common/enum_registry.h"
#include "cppschema/common/visitor_macros.h"
#include "ujcore/data/IdTypes.h"

namespace ujcore {

// Stores info about an asset: media, artifacts, graph inputs / outputs.
struct AssetInfo {
  enum class AssetType {
    UNKNOWN = 0,
    GRAPHIN, // Graph input
    MANUAL, // Manually entered slot input.
    ARTIFACT, // Created at an output slot.
    GRAPHOUT, // Graph output
  };

  // In case of a graph input node or graph output node, this is the slot id of the
  // node's single slot.
  // In case of manual data, this is the slot id of the input slot.
  // In case of artifact, this is the slot id of the output slot where the artifact
  // is created.
  SlotId slotId;

  AssetType assetType = AssetType::UNKNOWN;

  // The data type within the pipeline that this asset maps into, e.g. "bitmap"
  std::string dtype;

  // A URI that references the asset, e.g. "idb:/media/lake-12345.png" (for IndexedDb).
  // It is populated for graph input assets and manually entered assets, and is generated
  // after pipeline execution for artifact assets.
  std::optional<std::string> assetUri;

  DEFINE_ENUM_CONVERSION_FUNCTION(AssetType, UNKNOWN, GRAPHIN, MANUAL, ARTIFACT, GRAPHOUT);
  DEFINE_STRUCT_VISITOR_FUNCTION(slotId, assetType, dtype, assetUri);
};

} // namespace ujcore
