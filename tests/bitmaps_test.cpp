// SPDX-License-Identifier: MIT
#include "rinswf/bitmaps.hpp"
#include <cassert>
#include <vector>
#include <zlib.h>
using namespace rinswf;
int main() {
    // 1x1 lossless format-5 bitmap; reserved byte, then RGB.
    std::vector<std::uint8_t> raw{0,17,34,51};
    uLongf size=compressBound(raw.size());
    std::vector<std::uint8_t> compressed(size);
    assert(compress2(compressed.data(),&size,raw.data(),raw.size(),Z_BEST_COMPRESSION)==Z_OK);
    Tag tag{20,0,{7,0,5,1,0,1,0}};
    tag.data.insert(tag.data.end(),compressed.begin(),compressed.begin()+size);
    auto decoded=decode_lossless(tag);
    assert(decoded);
    assert(decoded.bitmap.character_id==7);
    assert(decoded.bitmap.rgba==std::vector<std::uint8_t>({17,34,51,255}));
    Document document;
    document.tags.push_back(tag);
    BitmapLibrary library;
    assert(collect_lossless(document,library)==Error::none);
    assert(library.find(7)!=nullptr);
    assert(library.find(8)==nullptr);
    assert(!decode_lossless(tag,0));
    tag.data[2]=8;
    assert(!decode_lossless(tag));
}
