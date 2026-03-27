#pragma once

#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "ujcore/data/graph/GraphEdge.h"
#include "ujcore/data/graph/GraphNode.h"
#include "ujcore/data/graph/GraphSlot.h"

namespace ujcore::data {

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
void AbslStringify(Sink& sink, const GraphNode& node) {
    const std::string slots_list = absl::StrJoin(node.slots, ",");
    absl::Format(&sink, "(n#%d:%s, slots:%s, fn:%s)", node.id, node.alnumid, slots_list, node.fnuri);
}

template <typename Sink>
void AbslStringify(Sink& sink, const GraphEdge& edge) {
    absl::Format(&sink, "(e#%d: [%d/%s] -> [%d/%s])", edge.id, edge.node0, edge.slot0, edge.node1, edge.slot1);
}

template <typename Sink>
void AbslStringify(Sink& sink, const GraphSlot& slot) {
    const std::string typestr = slot.isoutput ? "out" : "in";
    absl::Format(&sink, "(%d.%s, %s %s)", slot.parent, slot.name, typestr, slot.dtype);
}

// Array of nodes, edges, slots.

template <typename Sink>
void AbslStringify(Sink& sink, const std::vector<GraphNode>& arr) {
    absl::Format(&sink, "%s", detail::CollectionToStr(arr));
}

template <typename Sink>
void AbslStringify(Sink& sink, const std::vector<GraphEdge>& arr) {
    absl::Format(&sink, "%s", detail::CollectionToStr(arr));
}

template <typename Sink>
void AbslStringify(Sink& sink, const std::vector<GraphSlot>& arr) {
    absl::Format(&sink, "%s", detail::CollectionToStr(arr));
}


}  // namespace ujcore::data
