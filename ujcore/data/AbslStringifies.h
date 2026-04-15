#pragma once

#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ujcore/data/IdTypes.h"
#include "ujcore/data/plinfo.h"

namespace ujcore {

// Template logic not meant to be directly referenced are placed inside "detail"
// TODO(avik): Move these into an "...-inl.h" (inline header) file.
namespace detail {

template <typename T>
concept HasValueType = requires { typename T::value_type; };

template <HasValueType T>
std::string CollectionToStr(const T& collection) {
    std::vector<std::string> str_parts;
    str_parts.reserve(collection.size());
    for (const auto& e : collection) {
        str_parts.push_back(absl::StrCat(e));
    }
    return absl::StrCat("{", absl::StrJoin(str_parts, ", "), "}");
}

}  // namespace detail

template <typename Sink>
void AbslStringify(Sink& sink, const NodeId& nodeId) {
    absl::Format(&sink, "%d", nodeId.value);
}

template <typename Sink>
void AbslStringify(Sink& sink, const EdgeId& edgeId) {
    absl::Format(&sink, "%d", edgeId.value);
}

template <typename Sink>
void AbslStringify(Sink& sink, const SlotId& slotId) {
    absl::Format(&sink, "%d.%s", slotId.parent.value, slotId.name);
}

namespace plinfo {

template <typename Sink>
void AbslStringify(Sink& sink, const plinfo::NodeInfo& node) {
    std::string slotsStr;
    if (node.ins.size() > 0) {
      absl::StrAppend(&slotsStr, "; ins:", absl::StrJoin(node.ins, ","));
    }
    if (node.outs.size() > 0) {
      absl::StrAppend(&slotsStr, "; outs:", absl::StrJoin(node.outs, ","));
    }
    if (node.inouts.size() > 0) {
      absl::StrAppend(&slotsStr, "; inouts:", absl::StrJoin(node.inouts, ","));
    }
    absl::Format(&sink, "(n#%d:%s; fn:%s%s)", node.rawId.value, node.alnumid, node.fnuri, slotsStr);
}

template <typename Sink>
void AbslStringify(Sink& sink, const plinfo::EdgeInfo& edge) {
    absl::Format(&sink, "(%s: [%d/%s] -> [%d/%s])",
         edge.catid, edge.node0.value, edge.slot0, edge.node1.value, edge.slot1);
}

template <typename Sink>
void AbslStringify(Sink& sink, const plinfo::SlotInfo& slot) {
    std::string accessStr;
    switch (slot.access) {
        case plinfo::SlotInfo::AccessEnum::I:
          accessStr = "I";
          break;
        case plinfo::SlotInfo::AccessEnum::O:
          accessStr = "O";
          break;
        case plinfo::SlotInfo::AccessEnum::M:
          accessStr = "M";
          break;
    }
    absl::Format(&sink, "(%d.%s, %s %s)", slot.parent.value, slot.name, accessStr, slot.dtype);
}

// Array of nodes, edges, slots.

template <typename Sink>
void AbslStringify(Sink& sink, const std::vector<plinfo::NodeInfo>& arr) {
    absl::Format(&sink, "%s", detail::CollectionToStr(arr));
}

template <typename Sink>
void AbslStringify(Sink& sink, const std::vector<plinfo::EdgeInfo>& arr) {
    absl::Format(&sink, "%s", detail::CollectionToStr(arr));
}

template <typename Sink>
void AbslStringify(Sink& sink, const std::vector<plinfo::SlotInfo>& arr) {
    absl::Format(&sink, "%s", detail::CollectionToStr(arr));
}


}  // namespace plinfo

}  // namespace ujcore
