#include "ujcore/utils/IdUtils.h"

#include <cstdint>
#include <string>
#include <vector>

#include "gmock/gmock-matchers.h"
#include "gtest/gtest.h"

namespace ujcore {
namespace {

TEST(IdUtilsTest, RoundTripSampleIds) {
    const std::vector<uint32_t> ids = {
        0U,
        1U,
        2U,
        61U,
        62U,
        12345U,
        0x80000000U,
        0xffffffffU,
    };

    for (const uint32_t id : ids) {
        const std::string encoded = EncodeStringId(id);
        const uint32_t decoded = DecodeStringId(encoded);
        EXPECT_EQ(decoded, id);
    }
}

TEST(IdUtilsTest, MultipleTestIds) {
    // Generate 20 test ids.
    const uint32_t testIds[] = {
        0U,
        1U,
        2U,
        61U,
        62U,
        12345U,
        0x80000000U,
        0xffffffffU,
        13579U,
        24680U,
        987654321U,
        1122334455U,
        2147483647U,
        1020304050U,
        908070605U,
        1234567890U,
        4000000001U,
        3555555555U,
        3333333333U,
        3777777777U,
    };

    // Tes corresponding encoded ids.
    const std::string encodedIds[] = {
        "418340",
        "3Wqn4z",
        "1uBurk",
        "p5dzW",
        "1hsmgH",
        "3wv4iV",
        "1XnK8s",
        "2rzb8d",
        "36JhuW",
        "hKt0a",
        "16ZjEC",
        "1WjTA0",
        "3G33QQ",
        "JYouQ",
        "3t74oL",
        "49WcF9",
        "f2Jyo",
        "LPpJ3",
        "4KQtEF",
        "2CQrrA",
    };

    for (int i = 0; i < sizeof(testIds) / sizeof(testIds[0]); ++i) {
        const uint32_t id = testIds[i];
        const std::string encoded = EncodeStringId(id);

        // Verify that the encoded string matches the expected value for the test id.
        EXPECT_EQ(encoded, encodedIds[i]) << " for id " << id;

        // Check that the encoded strings are not the same as the original numeric ids.
        EXPECT_NE(EncodeStringId(id), std::to_string(id)) << " for id " << id;

        // Check that re-encoding is deterministic.
        EXPECT_EQ(EncodeStringId(id), encoded) << " for id " << id;
    }
}

TEST(IdUtilsTest, EncodedStringsUseBase62Chars) {
    const std::string encoded = EncodeStringId(13579U);
    for (const char c : encoded) {
        EXPECT_TRUE((c >= '0' && c <= '9') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c >= 'a' && c <= 'z'));
    }
}

}  // namespace
}  // namespace ujcore
