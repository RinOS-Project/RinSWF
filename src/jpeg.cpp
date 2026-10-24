// SPDX-License-Identifier: MIT
#include "rinswf/jpeg.hpp"
#include <limits>
#include <csetjmp>
#ifdef RINSWF_HAVE_LIBJPEG
#include <jpeglib.h>
#endif
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
#ifdef RINSWF_HAVE_LIBJPEG
struct JpegError {
    jpeg_error_mgr base;
    jmp_buf jump;
};
void jpeg_error_exit(j_common_ptr info) {
    auto* err=reinterpret_cast<JpegError*>(info->err);
    longjmp(err->jump,1);
}
#endif
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

namespace rinswf {
BitmapResult decode_jpeg_bitmap(const JpegAsset& asset,std::size_t max_pixels) {
#ifndef RINSWF_HAVE_LIBJPEG
    (void)asset; (void)max_pixels;
    return {Error::unsupported_compression,"libjpeg dependency unavailable",{}};
#else
    if(asset.image_data.empty() || asset.image_data.size()>std::numeric_limits<unsigned long>::max())
        return {Error::invalid_length,"invalid JPEG image length",{}};
    jpeg_decompress_struct info{};
    JpegError error{};
    info.err=jpeg_std_error(&error.base);
    error.base.error_exit=jpeg_error_exit;
    jpeg_create_decompress(&info);
    if(setjmp(error.jump)) {
        jpeg_destroy_decompress(&info);
        return {Error::malformed_tag,"JPEG decoder rejected input",{}};
    }
    jpeg_mem_src(&info,asset.image_data.data(),
                 static_cast<unsigned long>(asset.image_data.size()));
    if(jpeg_read_header(&info,TRUE)!=JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&info);
        return {Error::malformed_tag,"JPEG header invalid",{}};
    }
    if(!info.image_width || !info.image_height ||
       info.image_width>65535 || info.image_height>65535 ||
       std::size_t(info.image_width)>max_pixels/std::size_t(info.image_height)) {
        jpeg_destroy_decompress(&info);
        return {Error::size_limit,"JPEG dimensions exceed limits",{}};
    }
    info.out_color_space=JCS_RGB;
    jpeg_start_decompress(&info);
    const std::size_t width=info.output_width,height=info.output_height;
    if(info.output_components!=3 || !width || !height ||
       width>max_pixels/height) {
        jpeg_destroy_decompress(&info);
        return {Error::malformed_tag,"unsupported JPEG output",{}};
    }
    Bitmap bitmap;
    bitmap.character_id=asset.character_id;
    bitmap.width=static_cast<std::uint16_t>(width);
    bitmap.height=static_cast<std::uint16_t>(height);
    bitmap.rgba.resize(width*height*4);
    std::vector<std::uint8_t> scanline(width*3);
    while(info.output_scanline<info.output_height) {
        JSAMPROW row=scanline.data();
        const std::size_t y=info.output_scanline;
        if(jpeg_read_scanlines(&info,&row,1)!=1) {
            jpeg_destroy_decompress(&info);
            return {Error::malformed_tag,"JPEG scanline decode failed",{}};
        }
        for(std::size_t x=0;x<width;++x) {
            const auto src=x*3,dst=(y*width+x)*4;
            bitmap.rgba[dst]=scanline[src];
            bitmap.rgba[dst+1]=scanline[src+1];
            bitmap.rgba[dst+2]=scanline[src+2];
            bitmap.rgba[dst+3]=255;
        }
    }
    jpeg_finish_decompress(&info);
    jpeg_destroy_decompress(&info);
    if(asset.has_alpha) {
        std::vector<std::uint8_t> alpha;
        const auto e=decode_jpeg_alpha(asset,width,height,alpha,max_pixels);
        if(e!=Error::none) return {e,"JPEG alpha channel decoding failed",{}};
        for(std::size_t i=0;i<alpha.size();++i) bitmap.rgba[i*4+3]=alpha[i];
    }
    return {Error::none,{},std::move(bitmap)};
#endif
}
Error collect_jpeg_bitmaps(const Document& document,BitmapLibrary& library,std::size_t max_pixels) {
    for(const auto& tag:document.tags) {
        if(tag.code!=21 && tag.code!=35 && tag.code!=90) continue;
        auto asset=parse_jpeg_asset(tag);
        if(!asset) return asset.error;
        auto decoded=decode_jpeg_bitmap(asset.asset,max_pixels);
        if(!decoded) return decoded.error;
        if(!library.images.emplace(decoded.bitmap.character_id,std::move(decoded.bitmap)).second)
            return Error::malformed_tag;
    }
    return Error::none;
}
}
