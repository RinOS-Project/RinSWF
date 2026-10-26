// SPDX-License-Identifier: MIT
#include "rinswf/swf.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>
#include <zlib.h>
#ifdef RINSWF_HAVE_LZMA
#include <lzma.h>
#endif

namespace rinswf {
namespace {
std::uint16_t u16(std::span<const std::uint8_t> s, std::size_t p) {
    return static_cast<std::uint16_t>(s[p] | (std::uint16_t(s[p+1]) << 8));
}
std::uint32_t u32(std::span<const std::uint8_t> s, std::size_t p) {
    return std::uint32_t(s[p]) | (std::uint32_t(s[p+1]) << 8)
         | (std::uint32_t(s[p+2]) << 16) | (std::uint32_t(s[p+3]) << 24);
}
std::uint32_t bits(std::span<const std::uint8_t> s, std::size_t bit, unsigned n) {
    std::uint32_t value = 0;
    for (unsigned i=0;i<n;++i) {
        const auto p = bit+i;
        value = (value << 1) | ((s[p/8] >> (7-(p%8))) & 1u);
    }
    return value;
}
std::int32_t signed_bits(std::span<const std::uint8_t> s, std::size_t bit, unsigned n) {
    const std::uint32_t v = bits(s, bit, n);
    if (n == 0) return 0;
    if (n == 32) return static_cast<std::int32_t>(v);
    return static_cast<std::int32_t>((v ^ (1u << (n-1))) - (1u << (n-1)));
}
Result failure(Error e, const char* message) { return {e, message, {}}; }
}
const char* error_string(Error e) noexcept {
    switch (e) {
        case Error::none: return "none";
        case Error::truncated: return "truncated input";
        case Error::invalid_signature: return "invalid SWF signature";
        case Error::unsupported_compression: return "unsupported compression";
        case Error::invalid_length: return "invalid SWF length";
        case Error::size_limit: return "resource limit exceeded";
        case Error::malformed_tag: return "malformed tag";
        case Error::decompression_failed: return "decompression failed";
    }
    return "unknown";
}
Result parse(std::span<const std::uint8_t> input, Limits limits) {
    if (input.size() < 8) return failure(Error::truncated, "SWF header requires 8 bytes");
    if (input[1] != 'W' || input[2] != 'S') return failure(Error::invalid_signature, "not SWF");
    const auto marker = input[0];
    if (marker != 'F' && marker != 'C' && marker != 'Z')
        return failure(Error::invalid_signature, "unknown SWF signature");
    const std::uint32_t declared = u32(input, 4);
    if (declared < 8) return failure(Error::invalid_length, "SWF length < 8");
    if (marker == 'Z' && input[3] < 13)
        return failure(Error::invalid_length,"ZWS requires SWF version 13 or later");
    if (declared > limits.max_uncompressed_bytes) return failure(Error::size_limit, "SWF exceeds size limit");
    std::vector<std::uint8_t> storage;
    std::span<const std::uint8_t> src = input;
    if (marker == 'F') {
        if (input.size() != declared) return failure(Error::invalid_length, "FWS size mismatch");
    } else if (marker == 'C') {
        storage.resize(declared);
        std::copy_n(input.begin(), 8, storage.begin());
        uLongf output_size = declared - 8;
        if (input.size()-8 > std::numeric_limits<uInt>::max())
            return failure(Error::size_limit, "compressed data too large");
        if (uncompress(storage.data()+8, &output_size, input.data()+8,
                       static_cast<uLong>(input.size()-8)) != Z_OK || output_size != declared-8)
            return failure(Error::decompression_failed, "CWS zlib decode failed or length mismatch");
        src = storage;
    } else {
#ifndef RINSWF_HAVE_LZMA
        return failure(Error::unsupported_compression,"ZWS requires liblzma");
#else
        // ZWS: 8-byte SWF header, compressed length, 5-byte LZMA1
        // properties, then the raw LZMA stream. The advertised length is
        // the compressed payload size excluding its five properties bytes.
        if(input.size()<17)return failure(Error::truncated,"truncated ZWS header");
        const std::size_t compressed_size=u32(input,8);
        if(compressed_size!=input.size()-17)
            return failure(Error::invalid_length,"ZWS compressed length mismatch");
        lzma_filter filters[]={{LZMA_FILTER_LZMA1,nullptr},{LZMA_VLI_UNKNOWN,nullptr}};
        if(lzma_properties_decode(&filters[0],nullptr,input.data()+12,5)!=LZMA_OK)
            return failure(Error::decompression_failed,"invalid ZWS LZMA properties");
        // liblzma allocates the decoded filter options; release on all paths.
        const auto* options=static_cast<const lzma_options_lzma*>(filters[0].options);
        if(options->dict_size>limits.max_uncompressed_bytes) {
            std::free(filters[0].options);
            return failure(Error::size_limit,"ZWS dictionary exceeds resource limit");
        }
        lzma_stream decoder=LZMA_STREAM_INIT;
        const auto init=lzma_raw_decoder(&decoder,filters);
        std::free(filters[0].options);
        if(init!=LZMA_OK) {
            lzma_end(&decoder);
            return failure(Error::decompression_failed,"ZWS decoder initialization failed");
        }
        // Decoder state must be released even if the output allocation throws.
        try {storage.resize(declared);}
        catch(...) {lzma_end(&decoder);throw;}
        std::copy_n(input.begin(),8,storage.begin());
        decoder.next_in=input.data()+17;
        decoder.avail_in=compressed_size;
        decoder.next_out=storage.data()+8;
        decoder.avail_out=declared-8;
        const lzma_ret decoded=lzma_code(&decoder,LZMA_FINISH);
        const bool complete=decoded==LZMA_STREAM_END &&
            decoder.total_out==declared-8 && decoder.avail_in==0;
        lzma_end(&decoder);
        if(!complete)
            return failure(Error::decompression_failed,"ZWS LZMA decode or length mismatch");
        src=storage;
#endif
    }
    // RECT: five high bits declare number of signed bits in each coordinate.
    if (src.size() < 9) return failure(Error::truncated, "missing RECT");
    const unsigned n = src[8] >> 3;
    const std::size_t rect_bits = 5u + 4u*n;
    const std::size_t rect_bytes = (rect_bits+7)/8;
    if (8+rect_bytes+4 > src.size()) return failure(Error::truncated, "truncated frame header");
    std::span<const std::uint8_t> rect = src.subspan(8, rect_bytes);
    Header h;
    h.version = src[3]; h.declared_size = declared;
    std::size_t bit = 5;
    h.xmin = signed_bits(rect, bit, n); bit += n;
    h.xmax = signed_bits(rect, bit, n); bit += n;
    h.ymin = signed_bits(rect, bit, n); bit += n;
    h.ymax = signed_bits(rect, bit, n);
    if (h.xmin > h.xmax || h.ymin > h.ymax)
        return failure(Error::invalid_length, "invalid stage rectangle");
    std::size_t p = 8+rect_bytes;
    h.frame_rate_fixed8 = u16(src, p); p += 2;
    h.frame_count = u16(src, p); p += 2;
    Document doc; doc.header = h;
    bool ended = false;
    while (p < src.size()) {
        if (src.size()-p < 2) return failure(Error::truncated, "truncated tag header");
        const std::size_t tag_offset = p;
        const std::uint16_t packed = u16(src, p); p += 2;
        const std::uint16_t code = packed >> 6;
        std::size_t length = packed & 0x3f;
        if (length == 0x3f) {
            if (src.size()-p < 4) return failure(Error::truncated, "truncated long tag length");
            length = u32(src, p); p += 4;
        }
        if (length > limits.max_tag_bytes || doc.tags.size() >= limits.max_tags)
            return failure(Error::size_limit, "tag limit exceeded");
        if (length > src.size()-p) return failure(Error::malformed_tag, "tag length exceeds file");
        Tag tag; tag.code = code; tag.offset = tag_offset;
        tag.data.assign(src.begin()+p, src.begin()+p+length);
        doc.tags.push_back(std::move(tag));
        p += length;
        if (code == 0) {
            if (length != 0 || p != src.size())
                return failure(Error::malformed_tag, "invalid End tag or trailing data");
            ended = true;
            break;
        }
    }
    if (!ended) return failure(Error::truncated, "missing End tag");
    return {Error::none, {}, std::move(doc)};
}
} // namespace rinswf
