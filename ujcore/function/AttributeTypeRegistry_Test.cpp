#include "ujcore/function/AttributeTypeRegistry.h"

#include <algorithm>
#include <string>

#include "gtest/gtest.h"

namespace ujcore {
namespace {

using ::testing::Test;

class AttributeTypeRegistryTest : public Test {
protected:
    void TearDown() override {
        // Clear the registry after each test to avoid interference between tests.
        AttributeTypeRegistry::GetInstance().ClearRegistry();
    }
};

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define FILE_LINE __FILE__ ":" TOSTRING(__LINE__)

TEST_F(AttributeTypeRegistryTest, RegisterEncodeDecodeFloat) {
    struct FloatStore {
        float val = 0.f;
    };

    auto enode = [](std::shared_ptr<void> data) -> std::string {
        auto* floatStore = static_cast<FloatStore*>(data.get());
        return std::to_string(floatStore->val);
    };

    auto decode = [](const std::string& encoded) -> std::shared_ptr<void> {
        auto floatStore = std::make_shared<FloatStore>();
        floatStore->val = std::stof(encoded);
        return floatStore;
    };

    auto& registry = AttributeTypeRegistry::GetInstance();

    registry.MutableTypeBuilder("float", FILE_LINE)
        .SetLabel("Float")
        .SetToEncodedFn(std::move(enode))
        .SetFromEncodedFn(std::move(decode));
    
    const auto* toEncodedFn = registry.GetEncodFn("float");
    const auto* fromEncodedFn = registry.GetDecodFn("float");
    ASSERT_NE(toEncodedFn, nullptr);
    ASSERT_NE(fromEncodedFn, nullptr);

    std::string encoded = "3.141596";
    std::shared_ptr<void> data = (*fromEncodedFn)(encoded);
    std::string reEncoded = (*toEncodedFn)(data);
    EXPECT_EQ(encoded, reEncoded);
}

TEST_F(AttributeTypeRegistryTest, RegisterEncodeDecodePoint2D) {
    struct Point2D {
        float x = 0.f;
        float y = 0.f;
    };

    auto encode = [](std::shared_ptr<void> data) -> std::string {
        auto* point = static_cast<Point2D*>(data.get());
        return std::to_string(point->x) + "," + std::to_string(point->y);
    };

    auto decode = [](const std::string& encoded) -> std::shared_ptr<void> {
        auto point = std::make_shared<Point2D>();
        size_t commaPos = encoded.find(',');
        point->x = std::stof(encoded.substr(0, commaPos));
        point->y = std::stof(encoded.substr(commaPos + 1));
        return point;
    };

    auto& registry = AttributeTypeRegistry::GetInstance();

    registry.MutableTypeBuilder("point2d", FILE_LINE)
        .SetLabel("2D Point")
        .SetToEncodedFn(std::move(encode))
        .SetFromEncodedFn(std::move(decode));
    
    const auto* toEncodedFn = registry.GetEncodFn("point2d");
    const auto* fromEncodedFn = registry.GetDecodFn("point2d");
    ASSERT_NE(toEncodedFn, nullptr);
    ASSERT_NE(fromEncodedFn, nullptr);

    std::shared_ptr<void> data = std::make_shared<Point2D>(Point2D{1.5f, 2.5f});
    std::string encoded = (*toEncodedFn)(data);
    std::shared_ptr<void> decodedData = (*fromEncodedFn)(encoded);
    auto* decodedPoint = static_cast<Point2D*>(decodedData.get());
    EXPECT_FLOAT_EQ(decodedPoint->x, 1.5f);
    EXPECT_FLOAT_EQ(decodedPoint->y, 2.5f);
}

TEST_F(AttributeTypeRegistryTest, GetAllTypesAndLabels) {
    auto& registry = AttributeTypeRegistry::GetInstance();

    registry.MutableTypeBuilder("float", FILE_LINE).SetLabel("Float");
    registry.MutableTypeBuilder("point2d", FILE_LINE).SetLabel("2D Point");

    std::vector<std::tuple<std::string, std::string>> typesAndLabels = registry.GetAllTypesAndLabels();
    ASSERT_EQ(typesAndLabels.size(), 2);

    std::sort(typesAndLabels.begin(), typesAndLabels.end(),
        [](const auto& a, const auto& b) { return std::get<0>(a) < std::get<0>(b); });
    EXPECT_EQ(std::get<0>(typesAndLabels[0]), "float");
    EXPECT_EQ(std::get<1>(typesAndLabels[0]), "Float");
    EXPECT_EQ(std::get<0>(typesAndLabels[1]), "point2d");
    EXPECT_EQ(std::get<1>(typesAndLabels[1]), "2D Point");
}

}  // namespace
}  // namespace ujcore