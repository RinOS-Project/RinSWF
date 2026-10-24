// SPDX-License-Identifier: MIT
#include "rinswf/jpeg.hpp"
#include <limits>
#include <zlib.h>
namespace rinswf {
namespace {
std::uint16_t le16(const std::vector<std::uint8_t>& d,std::size_t p) {
    return std::uint16_t(d[p]) | (std::uint16_t(d[p+1])<<8);
}
std::uint32_t le32(const std::vector<std::uint8_t>& d,std::size_t p) {
    return std::uint32_t(d[p]) | (std::uint32_t(d[p+1])<<8) |
           (std::uint32_t(d[p+2])<<16) | (std::uint32_t(d[p+3])<<24);
}
JpegResult fail(Error e,const char* s) { return {e,s,{}}; }
}
JpegResult parse_jpeg_asset(const Tag& tag) {
    if(tag.code!=21 && tag.code!=35 && tag.code!=90)
        return fail(Error::malformed_tag,"not a DefineBitsJPEG2/3/4 tag");
    const auto& d=tag.data;
    const std::size_t prefix=tag.code==21?2:(tag.code==35?6:8);
    if(d.size()<prefix) return fail(Error::truncated,"JPEG tag header truncated");
    JpegAsset asset;
    asset.character_id=le16(d,0);
    if(tag.code==21) {
        asset.image_data.assign(d.begin()+2,d.end());
    } else {
        const std::uint32_t alpha_offset=le32(d,2);
        if(alpha_offset>d.size()-prefix)
            return fail(Error::malformed_tag,"JPEG alpha offset outside payload");
        if(tag.code==90) asset.deblock_fixed8=static_cast<std::int16_t>(le16(d,6));
        asset.has_alpha=true;
        asset.image_data.assign(d.begin()+prefix,d.begin()+prefix+alpha_offset);
        asset.compressed_alpha.assign(d.begin()+prefix+alpha_offset,d.end());
        if(asset.compressed_alpha.empty())
            return fail(Error::truncated,"JPEG alpha stream missing");
    }
    if(asset.image_data.empty()) return fail(Error::truncated,"JPEG image data missing");
    return {Error::none,{},std::move(asset)};
}
Error decode_jpeg_alpha(const JpegAsset& asset,std::size_t width,std::size_t height,
                        std::vector<std::uint8_t>& alpha,std::size_t max_pixels) {
    alpha.clear();
    if(!asset.has_alpha) return Error::malformed_tag;
    if(!width||!height||width>max_pixels/height) return Error::size_limit;
    const auto expected=width*height;
    if(expected>std::numeric_limits<uInt>::max() ||
       asset.compressed_alpha.size()>std::numeric_limits<uInt>::max())
        return Error::size_limit;
    std::vector<std::uint8_t> buffer(expected);
    uLongf size=static_cast<uLongf>(expected);
    if(uncompress(buffer.data(),&size,asset.compressed_alpha.data(),
                  static_cast<uLong>(asset.compressed_alpha.size()))!=Z_OK || size!=expected)
        return Error::decompression_failed;
    alpha=std::move(buffer);
    return Error::none;
}
}
