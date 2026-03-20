#pragma once

#include <cstdint>
#include <string>

namespace ujcore {

// Extension fields of a node which are exclusively used in UI.
struct NodeUIFields {
    std::string name_u8 = u8"(not set! 😀)";
};

// Node representation for UI, it contains UI specific fields, and minimal data to locate it back in the pipeline.
struct NodeFacade {
    std::string node_id;
    NodeUIFields ui_fields;
};


// File: Ops

struct InsertNodeOp {
    struct Request {};
    struct Response {
        NodeFacade node_facade;
    };
    operator Response ()(const Request& req);
}




}  // namespace ujcore
