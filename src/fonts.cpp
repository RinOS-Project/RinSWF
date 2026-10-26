// SPDX-License-Identifier: MIT
#include "rinswf/fonts.hpp"
#include <algorithm>
namespace rinswf {
namespace {
std::uint16_t read_u16(const std::vector<std::uint8_t>& d,std::size_t p) {
    return std::uint16_t(d[p])|(std::uint16_t(d[p+1])<<8);
}
FontResult fail(Error code,const char* why) {return {code,why,{}};}
}
FontResult decode_define_font(const Tag& tag,std::size_t max_glyphs,
                             std::size_t max_glyph_bytes) {
    if(tag.code!=10)return fail(Error::malformed_tag,"not DefineFont");
    const auto& d=tag.data;
    if(d.size()<4)return fail(Error::truncated,"DefineFont header truncated");
    FontAsset font;
    font.character_id=read_u16(d,0);
    // The first offset is twice the number of glyphs and points at the
    // beginning of the SHAPE table relative to the offset table itself.
    const std::size_t table_bytes=read_u16(d,2);
    if((table_bytes&1)!=0)return fail(Error::malformed_tag,"odd DefineFont offset table");
    const std::size_t count=table_bytes/2;
    if(count>max_glyphs)return fail(Error::size_limit,"too many font glyphs");
    if(table_bytes>d.size()-2)return fail(Error::truncated,"font offset table truncated");
    const std::size_t shape_begin=2+table_bytes;
    if(d.size()-shape_begin>max_glyph_bytes)
        return fail(Error::size_limit,"font glyph data budget exceeded");
    std::size_t previous=table_bytes;
    font.glyphs.reserve(count);
    for(std::size_t i=0;i<count;++i) {
        const std::size_t off=read_u16(d,2+i*2);
        const std::size_t end=i+1<count?read_u16(d,2+(i+1)*2):d.size()-2;
        if(off<table_bytes||off<previous||end<off||end>d.size()-2)
            return fail(Error::malformed_tag,"invalid font glyph offset");
        FontGlyph glyph;
        glyph.shape_bytes.assign(d.begin()+2+off,d.begin()+2+end);
        font.glyphs.push_back(std::move(glyph));
        previous=off;
    }
    if(!count&&d.size()!=4)
        return fail(Error::malformed_tag,"empty font with trailing data");
    return {Error::none,{},std::move(font)};
}
}
