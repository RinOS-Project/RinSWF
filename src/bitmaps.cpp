// SPDX-License-Identifier: MIT
#include "rinswf/bitmaps.hpp"
#include <limits>
#include <zlib.h>
namespace rinswf {
namespace {
BitmapResult fail(Error code, const char* msg) { return {code,msg,{}}; }
std::uint16_t le16(const std::vector<std::uint8_t>& d,std::size_t p) {
    return std::uint16_t(d[p])|(std::uint16_t(d[p+1])<<8);
}
std::uint8_t unpremultiply(std::uint8_t v,std::uint8_t a) {
    if(!a) return 0;
    const unsigned value=(unsigned(v)*255u+unsigned(a)/2u)/unsigned(a);
    return static_cast<std::uint8_t>(value>255u?255u:value);
}
}
BitmapResult decode_lossless(const Tag& tag,std::size_t max_pixels) {
    if(tag.code!=20 && tag.code!=36) return fail(Error::malformed_tag,"not DefineBitsLossless");
    const auto& d=tag.data;
    if(d.size()<7) return fail(Error::truncated,"lossless bitmap header truncated");
    const auto id=le16(d,0);
    const auto fmt=d[2];
    const std::size_t width=le16(d,3),height=le16(d,5);
    if(!width||!height) return fail(Error::invalid_length,"zero bitmap dimension");
    if(width>max_pixels/height) return fail(Error::size_limit,"bitmap pixel limit exceeded");
    if(fmt!=3 && fmt!=4 && fmt!=5) return fail(Error::malformed_tag,"unsupported lossless bitmap format");
    if(fmt==4 && tag.code==36) return fail(Error::malformed_tag,"lossless2 format 4 is unsupported");
    const bool alpha=tag.code==36;
    std::size_t p=7;
    std::size_t count=0;
    if(fmt==3) {
        if(p>=d.size()) return fail(Error::truncated,"palette count missing");
        count=std::size_t(d[p++])+1;
    }
    const std::size_t palette_stride=alpha?4:3;
    const std::size_t stride=fmt==3?((width+3)/4)*4:
                             fmt==4?((width*2+3)/4)*4:width*4;
    if(stride>std::numeric_limits<std::size_t>::max()/height)
        return fail(Error::size_limit,"bitmap stride overflow");
    const std::size_t expected=count*palette_stride+stride*height;
    if(expected>std::numeric_limits<uInt>::max() || d.size()-p>std::numeric_limits<uInt>::max())
        return fail(Error::size_limit,"zlib input exceeds limit");
    std::vector<std::uint8_t> raw(expected);
    uLongf decoded=static_cast<uLongf>(expected);
    if(uncompress(raw.data(),&decoded,d.data()+p,static_cast<uLong>(d.size()-p))!=Z_OK || decoded!=expected)
        return fail(Error::decompression_failed,"bitmap zlib expansion failed");
    Bitmap result;
    result.character_id=id;
    result.width=static_cast<std::uint16_t>(width);
    result.height=static_cast<std::uint16_t>(height);
    result.rgba.resize(width*height*4);
    const auto pixels=raw.data()+count*palette_stride;
    for(std::size_t y=0;y<height;++y) for(std::size_t x=0;x<width;++x) {
        const auto at=(y*width+x)*4;
        std::uint8_t r=0,g=0,b=0,a=255;
        if(fmt==3) {
            const std::size_t index=pixels[y*stride+x];
            if(index>=count) return fail(Error::malformed_tag,"palette index out of bounds");
            const auto q=raw.data()+index*palette_stride;
            r=q[0];g=q[1];b=q[2];if(alpha) a=q[3];
        } else if(fmt==4) {
            const auto offset=y*stride+x*2;
            const auto pixel=std::uint16_t(pixels[offset]<<8)|pixels[offset+1];
            r=std::uint8_t(((pixel>>10)&31u)*255u/31u);
            g=std::uint8_t(((pixel>>5)&31u)*255u/31u);
            b=std::uint8_t((pixel&31u)*255u/31u);
        } else {
            const auto q=pixels+y*stride+x*4;
            if(alpha) {
                a=q[0];
                r=unpremultiply(q[1],a);g=unpremultiply(q[2],a);b=unpremultiply(q[3],a);
            } else {r=q[1];g=q[2];b=q[3];}
        }
        result.rgba[at]=r;result.rgba[at+1]=g;result.rgba[at+2]=b;result.rgba[at+3]=a;
    }
    return {Error::none,{},std::move(result)};
}
Error collect_lossless(const Document& document,BitmapLibrary& library,std::size_t max_pixels) {
    for(const auto& tag:document.tags) {
        if(tag.code!=20 && tag.code!=36) continue;
        auto result=decode_lossless(tag,max_pixels);
        if(!result) return result.error;
        if(!library.images.emplace(result.bitmap.character_id,std::move(result.bitmap)).second)
            return Error::malformed_tag;
    }
    return Error::none;
}
}
