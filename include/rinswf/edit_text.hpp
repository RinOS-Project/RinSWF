// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/swf.hpp"
#include "rinswf/shapes.hpp"
#include <string>
namespace rinswf {
// DefineEditText (tag 37) metadata. Parsing does not execute scripts,
// create an editable widget, or interpret HTML as trusted markup.
struct EditTextAsset {
    std::uint16_t character_id=0;
    std::int32_t xmin=0,xmax=0,ymin=0,ymax=0;
    std::uint16_t flags=0,font_id=0,font_height=0,max_length=0;
    std::uint16_t left_margin=0,right_margin=0,indent=0;
    std::int16_t leading=0;
    std::uint8_t align=0;
    ShapeColor color{};
    std::string font_class;
    std::string variable_name,initial_text;
};
struct EditTextResult {
    Error error=Error::none;
    std::string message;
    EditTextAsset text;
    explicit operator bool() const noexcept {return error==Error::none;}
};
[[nodiscard]] EditTextResult decode_edit_text(const Tag& tag,
    std::size_t max_string_bytes=1024*1024);
}
