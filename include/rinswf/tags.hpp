// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/swf.hpp"
#include <optional>
#include <variant>
namespace rinswf {
struct Rgb { std::uint8_t red=0, green=0, blue=0, alpha=255; };
struct BackgroundColor { Rgb color; };
struct FileAttributes {
    bool use_direct_blit=false, use_gpu=false, has_metadata=false;
    bool action_script3=false, use_network=false;
};
struct FrameLabel { std::string name; bool anchor=false; };
struct SymbolClassEntry { std::uint16_t character_id=0; std::string class_name; };
struct SymbolClass { std::vector<SymbolClassEntry> entries; };
struct BinaryData { std::uint16_t character_id=0; std::vector<std::uint8_t> bytes; };
using TypedTag = std::variant<BackgroundColor, FileAttributes, FrameLabel, SymbolClass, BinaryData>;
struct TagDecodeResult {
    Error error=Error::none;
    std::string message;
    std::optional<TypedTag> value; // empty for unrecognized tag codes
    explicit operator bool() const noexcept { return error==Error::none; }
};
[[nodiscard]] TagDecodeResult decode_tag(const Tag& tag);
}
