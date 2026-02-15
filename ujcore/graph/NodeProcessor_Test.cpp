#include "ujcore/graph/NodeProcessor.h"

#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"

namespace ujcore::graph {

TEST(NodeProcessorTest, Basic) {
    NodeProcessor subject;
    EXPECT_EQ(subject.DummyConstApiMethod(), 0);
}

}  // namespace ujcore::graph
