// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/swf.hpp"
#include <cstdint>
#include <vector>
namespace rinswf {
struct ShapePoint { std::int32_t x=0, y=0; }; // twips
enum class ShapeCommandKind { move_to, line_to, quadratic_to };
struct ShapeCommand {
    ShapeCommandKind kind=ShapeCommandKind::move_to;
    ShapePoint control{}, endpoint{};
    std::uint32_t fill0=0, fill1=0, line=0;
};
struct ShapeGeometry {
    std::uint16_t character_id=0;
    std::vector<ShapeCommand> commands;
};
struct ShapeResult {
    Error error=Error::none;
    std::string message;
    ShapeGeometry shape;
    explicit operator bool() const noexcept { return error==Error::none; }
};
// Supports DefineShape 1/2/3/4 with empty style tables.
// Non-empty style arrays are rejected explicitly until style decoding is implemented.
[[nodiscard]] ShapeResult decode_shape(const Tag& tag, std::size_t max_commands=100000);
}
