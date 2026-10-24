// SPDX-License-Identifier: MIT
#include "rinswf/jpeg.hpp"
#include <cassert>
#include <vector>
#include <zlib.h>
using namespace rinswf;
int main() {
    Tag jpeg2{21,0,{4,0,0xff,0xd8,0xff,0xd9}};
    auto basic=parse_jpeg_asset(jpeg2);
    assert(basic && basic.asset.character_id==4);
    assert(!basic.asset.has_alpha && basic.asset.image_data.size()==4);
    std::vector<std::uint8_t> pixels{17,127};
    uLongf n=compressBound(pixels.size());
    std::vector<std::uint8_t> encoded(n);
    assert(compress2(encoded.data(),&n,pixels.data(),pixels.size(),Z_BEST_COMPRESSION)==Z_OK);
    Tag jpeg3{35,0,{9,0,4,0,0,0,0xff,0xd8,0xff,0xd9}};
    jpeg3.data.insert(jpeg3.data.end(),encoded.begin(),encoded.begin()+n);
    auto parsed=parse_jpeg_asset(jpeg3);
    assert(parsed && parsed.asset.has_alpha && parsed.asset.image_data.size()==4);
    std::vector<std::uint8_t> decoded;
    assert(decode_jpeg_alpha(parsed.asset,2,1,decoded)==Error::none);
    assert(decoded==pixels);
    assert(decode_jpeg_alpha(parsed.asset,3,1,decoded)==Error::decompression_failed);
    jpeg3.data[2]=255;
    assert(!parse_jpeg_asset(jpeg3));
    Tag jpeg4{90,0,{10,0,4,0,0,0,0x80,0x01,0xff,0xd8,0xff,0xd9}};
    jpeg4.data.insert(jpeg4.data.end(),encoded.begin(),encoded.begin()+n);
    auto parsed4=parse_jpeg_asset(jpeg4);
    assert(parsed4 && parsed4.asset.deblock_fixed8==384);
    assert(!parse_jpeg_asset({9,0,{}}));
}
