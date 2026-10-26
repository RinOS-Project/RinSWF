// SPDX-License-Identifier: MIT
#include "rinswf/fonts.hpp"
#include <algorithm>
#include <limits>
namespace rinswf {
namespace {
std::uint16_t read_u16(const std::vector<std::uint8_t>& d,std::size_t p) {
    return std::uint16_t(d[p])|(std::uint16_t(d[p+1])<<8);
}
FontResult fail(Error code,const char* why) {return {code,why,{}};}
struct GlyphBits {
    const std::vector<std::uint8_t>& bytes;
    std::size_t bit=0;
    bool read(unsigned n,std::uint32_t& v) {
        if(n>32 || bit>bytes.size()*8 || n>bytes.size()*8-bit)return false;
        v=0;
        for(unsigned i=0;i<n;++i,++bit)v=(v<<1)|((bytes[bit/8]>>(7-bit%8))&1);
        return true;
    }
    bool signed_read(unsigned n,std::int32_t& v) {
        std::uint32_t raw;
        if(!read(n,raw))return false;
        if(!n){v=0;return true;}
        if(n==32){v=static_cast<std::int32_t>(raw);return true;}
        const auto sign=1u<<(n-1);
        v=static_cast<std::int32_t>((raw^sign)-sign);
        return true;
    }
};
bool add(std::int32_t& a,std::int32_t b) {
    const auto sum=std::int64_t(a)+b;
    if(sum<std::numeric_limits<std::int32_t>::min()||
       sum>std::numeric_limits<std::int32_t>::max())return false;
    a=static_cast<std::int32_t>(sum);
    return true;
}
FontResult decode_glyph(FontGlyph& glyph,std::size_t max_commands) {
    GlyphBits b{glyph.shape_bytes};
    std::uint32_t fill_bits=0,line_bits=0;
    if(!b.read(4,fill_bits)||!b.read(4,line_bits))
        return fail(Error::truncated,"glyph style bits truncated");
    if(fill_bits>1 || line_bits!=0)
        return fail(Error::malformed_tag,"unsupported glyph style indices");
    ShapeGeometry outline;
    outline.fills.push_back(ShapeFill{});
    std::int32_t x=0,y=0;
    std::uint32_t fill0=0,fill1=0;
    bool ended=false;
    while(b.bit<glyph.shape_bytes.size()*8) {
        std::uint32_t edge=0,flags=0;
        if(!b.read(1,edge)||!b.read(5,flags))
            return fail(Error::truncated,"glyph record truncated");
        if(!edge) {
            if(!flags){ended=true;break;}
            if(flags&0x18)
                return fail(Error::malformed_tag,"glyph line/NewStyles unsupported");
            if(flags&1) {
                std::uint32_t bits=0;
                if(!b.read(5,bits)||!b.signed_read(bits,x)||!b.signed_read(bits,y))
                    return fail(Error::truncated,"glyph move truncated");
                if(outline.commands.size()>=max_commands)
                    return fail(Error::size_limit,"glyph command limit");
                outline.commands.push_back({ShapeCommandKind::move_to,{}, {x,y},fill0,fill1,0});
            }
            std::uint32_t index=0;
            if(flags&2) {
                if(!b.read(fill_bits,index)||index>1)
                    return fail(Error::malformed_tag,"glyph fill0 invalid");
                fill0=index;
            }
            if(flags&4) {
                if(!b.read(fill_bits,index)||index>1)
                    return fail(Error::malformed_tag,"glyph fill1 invalid");
                fill1=index;
            }
        } else {
            if(outline.commands.size()>=max_commands)
                return fail(Error::size_limit,"glyph command limit");
            const bool straight=(flags&16)!=0;
            const unsigned n=(flags&15)+2;
            if(straight) {
                std::uint32_t general=0,vertical=0;
                std::int32_t dx=0,dy=0;
                if(!b.read(1,general))
                    return fail(Error::truncated,"glyph straight edge truncated");
                if(general) {
                    if(!b.signed_read(n,dx)||!b.signed_read(n,dy))
                        return fail(Error::truncated,"glyph straight edge truncated");
                } else {
                    if(!b.read(1,vertical))
                        return fail(Error::truncated,"glyph axis edge truncated");
                    if(vertical) {
                        if(!b.signed_read(n,dy))return fail(Error::truncated,"glyph vertical edge truncated");
                    } else if(!b.signed_read(n,dx))
                        return fail(Error::truncated,"glyph horizontal edge truncated");
                }
                if(!add(x,dx)||!add(y,dy))
                    return fail(Error::invalid_length,"glyph coordinate overflow");
                outline.commands.push_back({ShapeCommandKind::line_to,{}, {x,y},fill0,fill1,0});
            } else {
                std::int32_t dx=0,dy=0,ax=0,ay=0;
                if(!b.signed_read(n,dx)||!b.signed_read(n,dy)||
                   !b.signed_read(n,ax)||!b.signed_read(n,ay))
                    return fail(Error::truncated,"glyph curve truncated");
                if(!add(x,dx)||!add(y,dy))
                    return fail(Error::invalid_length,"glyph control overflow");
                const ShapePoint control{x,y};
                if(!add(x,ax)||!add(y,ay))
                    return fail(Error::invalid_length,"glyph endpoint overflow");
                outline.commands.push_back({ShapeCommandKind::quadratic_to,control,{x,y},fill0,fill1,0});
            }
        }
    }
    if(!ended)return fail(Error::truncated,"glyph EndShape missing");
    for(std::size_t i=b.bit;i<glyph.shape_bytes.size()*8;++i)
        if((glyph.shape_bytes[i/8]>>(7-i%8))&1)
            return fail(Error::malformed_tag,"glyph trailing data");
    glyph.outline=std::move(outline);
    return {};
}

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
        auto result=decode_glyph(glyph,100000);
        if(!result)return result;
        font.glyphs.push_back(std::move(glyph));
        previous=off;
    }
    if(!count&&d.size()!=4)
        return fail(Error::malformed_tag,"empty font with trailing data");
    return {Error::none,{},std::move(font)};
}
}
