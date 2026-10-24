// SPDX-License-Identifier: MIT
#include "rinswf/swf.hpp"
#include <cassert>
#include <cstdint>
#include <vector>
#include <zlib.h>
int main() {
    // Minimal valid SWF: RECT (all-zero), frame rate 24 fps, one frame, End tag.
    const std::vector<std::uint8_t> fws = {
        'F','W','S',9, 16,0,0,0, 0x08,0x00, 0x00,0x18, 0x01,0x00, 0x00,0x00
    };
    auto r = rinswf::parse(fws);
    assert(r);
    assert(r.document.header.version == 9);
    assert(r.document.header.frame_count == 1);
    assert(r.document.tags.size() == 1 && r.document.tags[0].code == 0);
    auto bad = fws;
    bad[4] = 17;
    assert(rinswf::parse(bad).error == rinswf::Error::invalid_length);
    bad = fws; bad.pop_back();
    assert(rinswf::parse(bad).error == rinswf::Error::invalid_length);
    bad = fws; bad[14] = 0x40; // code 1, not End
    assert(rinswf::parse(bad).error == rinswf::Error::truncated);
    std::vector<std::uint8_t> cws(fws.begin(),fws.begin()+8);
    cws[0]='C';
    uLongf compressed_length = compressBound(fws.size()-8);
    std::vector<std::uint8_t> compressed(compressed_length);
    assert(compress2(compressed.data(), &compressed_length, fws.data()+8,
                     fws.size()-8, Z_BEST_COMPRESSION) == Z_OK);
    cws.insert(cws.end(), compressed.begin(), compressed.begin()+compressed_length);
    assert(rinswf::parse(cws));
    return 0;
}
