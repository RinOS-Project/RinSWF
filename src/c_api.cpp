// SPDX-License-Identifier: MIT
#include "rinswf/c_api.h"
#include "rinswf/swf.hpp"
#include <new>
#include <utility>
struct rinswf_document { rinswf::Document value; };
namespace {
rinswf_status convert(rinswf::Error code) noexcept {
    switch(code) {
        case rinswf::Error::none: return RINSWF_OK;
        case rinswf::Error::truncated: return RINSWF_TRUNCATED;
        case rinswf::Error::invalid_signature: return RINSWF_INVALID_SIGNATURE;
        case rinswf::Error::unsupported_compression: return RINSWF_UNSUPPORTED_COMPRESSION;
        case rinswf::Error::invalid_length: return RINSWF_INVALID_LENGTH;
        case rinswf::Error::size_limit: return RINSWF_SIZE_LIMIT;
        case rinswf::Error::malformed_tag: return RINSWF_MALFORMED_TAG;
        case rinswf::Error::decompression_failed: return RINSWF_DECOMPRESSION_FAILED;
    }
    return RINSWF_INTERNAL_ERROR;
}
}
extern "C" {
rinswf_status rinswf_parse(const uint8_t* bytes,size_t size,
    const rinswf_limits* limits,rinswf_document** output) {
    if(!output)return RINSWF_INVALID_ARGUMENT;
    *output=nullptr;
    if(!bytes && size)return RINSWF_INVALID_ARGUMENT;
    try {
        const rinswf::Limits bounds=limits?
            rinswf::Limits{limits->max_uncompressed_bytes,limits->max_tags,limits->max_tag_bytes}:
            rinswf::Limits{};
        auto result=rinswf::parse(std::span<const uint8_t>(bytes,size),bounds);
        if(!result)return convert(result.error);
        auto* document=new rinswf_document{std::move(result.document)};
        *output=document;
        return RINSWF_OK;
    } catch(const std::bad_alloc&) {
        return RINSWF_OUT_OF_MEMORY;
    } catch(...) {
        return RINSWF_INTERNAL_ERROR;
    }
}
void rinswf_document_destroy(rinswf_document* document) {delete document;}
rinswf_status rinswf_document_header(const rinswf_document* document,
    rinswf_header* header) {
    if(!document||!header)return RINSWF_INVALID_ARGUMENT;
    const auto& h=document->value.header;
    *header={h.version,h.declared_size,h.xmin,h.xmax,h.ymin,h.ymax,
             h.frame_rate_fixed8,h.frame_count};
    return RINSWF_OK;
}
size_t rinswf_document_tag_count(const rinswf_document* document) {
    return document?document->value.tags.size():0;
}
rinswf_status rinswf_document_tag(const rinswf_document* document,
    size_t index,rinswf_tag_view* tag) {
    if(!document||!tag)return RINSWF_INVALID_ARGUMENT;
    *tag={};
    if(index>=document->value.tags.size())return RINSWF_INVALID_ARGUMENT;
    const auto& item=document->value.tags[index];
    *tag={item.code,item.offset,item.data.empty()?nullptr:item.data.data(),item.data.size()};
    return RINSWF_OK;
}
const char* rinswf_status_string(rinswf_status status) {
    switch(status) {
        case RINSWF_OK: return "success";
        case RINSWF_INVALID_ARGUMENT: return "invalid argument";
        case RINSWF_TRUNCATED: return "truncated input";
        case RINSWF_INVALID_SIGNATURE: return "invalid SWF signature";
        case RINSWF_UNSUPPORTED_COMPRESSION: return "unsupported compression";
        case RINSWF_INVALID_LENGTH: return "invalid SWF length";
        case RINSWF_SIZE_LIMIT: return "resource limit exceeded";
        case RINSWF_MALFORMED_TAG: return "malformed SWF tag";
        case RINSWF_DECOMPRESSION_FAILED: return "decompression failed";
        case RINSWF_OUT_OF_MEMORY: return "out of memory";
        case RINSWF_INTERNAL_ERROR: return "internal error";
    }
    return "unknown status";
}
}
