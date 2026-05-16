#pragma once

#include <cstdint>

// Represents the dimension of a bitmap, or rectangle size.
struct IDimension {
	int32_t width {0};
	int32_t height {0};

	[[nodiscard]] static constexpr IDimension MakeWH(int32_t w, int32_t h) {
		return IDimension{.width = w, .height = h};
	}

	int32_t area() const {
		return width * height;
	}
};
