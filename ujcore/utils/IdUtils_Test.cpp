#include "ujcore/utils/IdUtils.h"

#include <cstdint>
#include <string>
#include <vector>

#include "gmock/gmock-matchers.h"
#include "gtest/gtest.h"

namespace ujcore {
namespace {

TEST(IdUtilsTest, RoundTripSampleIds) {
    const std::vector<uint64_t> ids = {
        0ULL,
        1ULL,
        2ULL,
        61ULL,
        62ULL,
        12345ULL,
        (1ULL << 63),
        ~0ULL,
    };

    for (const uint64_t id : ids) {
        const std::string encoded = EncodeStringId(id);
        const uint64_t decoded = DecodeStringId(encoded);
        EXPECT_EQ(decoded, id);
    }
}

TEST(IdUtilsTest, MultipleTestIds) {
    // Generate 20 test ids.
    const uint64_t testIds[] = {
        0ULL,
        1ULL,
        2ULL,
        61ULL,
        62ULL,
        12345ULL,
        (1ULL << 63),
        ~0ULL,
        13579ULL,
        24680ULL,
        9876543210ULL,
        1122334455ULL,
        5566778899ULL,
        1020304050ULL,
        9080706050ULL,
        1234567890123456789ULL,
        9876543210987654321ULL,
        5555555555555555555ULL,
        3333333333333333333ULL,
        7777777777777777777ULL,
    };

    // Tes corresponding encoded ids.
    const std::string encodedIds[] = {
        "Kk424zRFk8n",
        "9TZElof1KXm",
        "Jz0FwG0uEyz",
        "LZyMLHXhAtO",
        "LpZNBsJ7xFP",
        "DEOM97BZcRv",
        "IRHu6ejjYfL",
        "AKBcjfiQW8S",
        "J88Pp1CIWA2",
        "Ksdi0BSPuqR",
        "JEG4zXGKSQO",
        "1q3zhbhPiSH",
        "H6roZA1CAAI",
        "BuR82SzDXMj",
        "1j15ZWHYLtO",
        "94AuxmWhJNS",
        "AUfiFQnekPa",
        "An63aMGuvnD",
        "wDsQP7QRbZ",
        "AUciBjz8K0d",
    };

    for (int i = 0; i < sizeof(testIds) / sizeof(testIds[0]); ++i) {
        const uint64_t id = testIds[i];
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
    const std::string encoded = EncodeStringId(13579ULL);
    for (const char c : encoded) {
        EXPECT_TRUE((c >= '0' && c <= '9') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c >= 'a' && c <= 'z'));
    }
}

}  // namespace
}  // namespace ujcore
