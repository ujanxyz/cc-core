#include "ujcore/graph/IdGenerator.h"

#include "gtest/gtest.h"
#include "gmock/gmock-matchers.h"

namespace ujcore {

TEST(IdGeneratorTest, DefaultLength) {
    IdGenerator subject;
    EXPECT_EQ(subject.GetNextId(), "pFE8FGqNWL");
    EXPECT_EQ(subject.GetNextId(), "s2GhcWpBLP");
    EXPECT_EQ(subject.GetNextId(), "ZBqg1rBrgq");
    EXPECT_EQ(subject.GetNextId(), "uAZRqnZLOd");
    EXPECT_EQ(subject.GetNextId(), "tW66Slet2W");
    EXPECT_EQ(subject.GetNextId(), "v2ASqZclhU");
    EXPECT_EQ(subject.GetNextId(), "g8obobcbbq");
    EXPECT_EQ(subject.GetNextId(), "zdgx3voHgX");
}

TEST(IdGeneratorTest, ModifiedLength) {
    IdGenerator subject;
    subject.SetIdLength(4);
    EXPECT_EQ(subject.GetNextId(), "qNWL");
    EXPECT_EQ(subject.GetNextId(), "pBLP");
    EXPECT_EQ(subject.GetNextId(), "Brgq");
    EXPECT_EQ(subject.GetNextId(), "ZLOd");
    EXPECT_EQ(subject.GetNextId(), "et2W");
    EXPECT_EQ(subject.GetNextId(), "clhU");
    EXPECT_EQ(subject.GetNextId(), "cbbq");
    EXPECT_EQ(subject.GetNextId(), "oHgX");
}

}  // namespace ujcore