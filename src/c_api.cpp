// SPDX-License-Identifier: MIT
#include "rinswf/c_api.h"
#include "rinswf/swf.hpp"
#include "rinswf/frame_renderer.hpp"
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <utility>
struct rinswf_document { rinswf::Document value; };
struct rinswf_movie {
    rinswf::TimelineResult timeline;
    rinswf::FrameAssets assets;
};
struct rinswf_player {
    rinswf_movie movie;
    rinswf::TimelinePlayer playback;
};
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
rinswf_status rinswf_movie_create(const rinswf_document* document,
    size_t max_asset_pixels,rinswf_movie** output) {
    if(!output)return RINSWF_INVALID_ARGUMENT;
    *output=nullptr;
    if(!document)return RINSWF_INVALID_ARGUMENT;
    try {
        auto movie=new rinswf_movie{};
        // Keep the allocation guarded until both timeline and assets succeed.
        std::unique_ptr<rinswf_movie> guard(movie);
        movie->timeline=rinswf::build_timeline(document->value);
        if(!movie->timeline)return convert(movie->timeline.error);
        const auto result=rinswf::collect_frame_assets(document->value,
            movie->assets,max_asset_pixels?max_asset_pixels:16*1024*1024);
        if(!result)return convert(result.error);
        *output=guard.release();
        return RINSWF_OK;
    } catch(const std::bad_alloc&) {return RINSWF_OUT_OF_MEMORY;}
      catch(...) {return RINSWF_INTERNAL_ERROR;}
}
void rinswf_movie_destroy(rinswf_movie* movie) {delete movie;}
size_t rinswf_movie_frame_count(const rinswf_movie* movie) {
    return movie?movie->timeline.frames.size():0;
}
rinswf_status rinswf_movie_render_rgba(const rinswf_movie* movie,
    size_t frame,uint32_t width,uint32_t height,
    uint8_t* pixels,size_t stride,size_t buffer_size) {
    if(!movie||!pixels||!width||!height)return RINSWF_INVALID_ARGUMENT;
    if(frame>=movie->timeline.frames.size())return RINSWF_INVALID_ARGUMENT;
    if(width>std::numeric_limits<size_t>::max()/4)return RINSWF_SIZE_LIMIT;
    const size_t row_bytes=size_t(width)*4;
    if(stride<row_bytes || size_t(height)-1>(std::numeric_limits<size_t>::max()-row_bytes)/stride)
        return RINSWF_SIZE_LIMIT;
    const size_t required=(size_t(height)-1)*stride+row_bytes;
    if(buffer_size<required)return RINSWF_INVALID_ARGUMENT;
    try {
        rinswf::Surface surface;
        auto result=rinswf::make_surface(width,height,surface);
        if(!result)return convert(result.error);
        // Explicit zeroed transparent canvas, with no hidden frame clock.
        result=rinswf::render_frame_at(movie->timeline.frames[frame],
            movie->assets,surface,frame);
        if(!result)return convert(result.error);
        for(size_t y=0;y<height;++y)
            std::memcpy(pixels+y*stride,surface.rgba.data()+y*row_bytes,row_bytes);
        return RINSWF_OK;
    } catch(const std::bad_alloc&) {return RINSWF_OUT_OF_MEMORY;}
      catch(...) {return RINSWF_INTERNAL_ERROR;}
}
rinswf_status rinswf_player_create(const rinswf_movie* movie,rinswf_player** output) {
    if(!output)return RINSWF_INVALID_ARGUMENT;
    *output=nullptr;
    if(!movie || movie->timeline.frames.empty())return RINSWF_INVALID_ARGUMENT;
    try {
        auto player=std::make_unique<rinswf_player>();
        // Take a private copy: destruction of the source movie cannot dangle
        // a running player. Each instance has independent sprite playback.
        player->movie=*movie;
        *output=player.release();
        return RINSWF_OK;
    } catch(const std::bad_alloc&) {return RINSWF_OUT_OF_MEMORY;}
      catch(...) {return RINSWF_INTERNAL_ERROR;}
}
void rinswf_player_destroy(rinswf_player* player) {delete player;}
rinswf_status rinswf_player_seek(rinswf_player* player,size_t frame) {
    if(!player || frame>=player->movie.timeline.frames.size())
        return RINSWF_INVALID_ARGUMENT;
    player->playback.seek(frame);
    return RINSWF_OK;
}
rinswf_status rinswf_player_advance(rinswf_player* player,size_t count) {
    if(!player)return RINSWF_INVALID_ARGUMENT;
    // Preserve monotonic frame counters without arithmetic wrapping.
    if(player->playback.clock.playing &&
       count>std::numeric_limits<size_t>::max()-player->playback.clock.frame)
        return RINSWF_SIZE_LIMIT;
    player->playback.advance(count);
    return RINSWF_OK;
}
rinswf_status rinswf_player_set_playing(rinswf_player* player,int playing) {
    if(!player)return RINSWF_INVALID_ARGUMENT;
    if(playing)player->playback.resume();
    else player->playback.pause();
    return RINSWF_OK;
}
rinswf_status rinswf_player_state(const rinswf_player* player,
    size_t* frame,int* playing) {
    if(!player || (!frame && !playing))return RINSWF_INVALID_ARGUMENT;
    if(frame)*frame=player->playback.clock.frame;
    if(playing)*playing=player->playback.clock.playing?1:0;
    return RINSWF_OK;
}
rinswf_status rinswf_player_render_rgba(rinswf_player* player,
    uint32_t width,uint32_t height,uint8_t* pixels,
    size_t stride,size_t buffer_size) {
    if(!player||!pixels||!width||!height)return RINSWF_INVALID_ARGUMENT;
    if(size_t(width)>std::numeric_limits<size_t>::max()/4)return RINSWF_SIZE_LIMIT;
    const size_t row_bytes=size_t(width)*4;
    if(stride<row_bytes || size_t(height)-1>
       (std::numeric_limits<size_t>::max()-row_bytes)/stride)
        return RINSWF_SIZE_LIMIT;
    if(buffer_size<(size_t(height)-1)*stride+row_bytes)
        return RINSWF_INVALID_ARGUMENT;
    try {
        rinswf::Surface surface;
        auto result=rinswf::make_surface(width,height,surface);
        if(!result)return convert(result.error);
        // Commit player state only if rendering succeeds.
        auto next=player->playback;
        result=next.render(player->movie.timeline,player->movie.assets,surface);
        if(!result)return convert(result.error);
        for(size_t y=0;y<height;++y)
            std::memcpy(pixels+y*stride,surface.rgba.data()+y*row_bytes,row_bytes);
        player->playback=std::move(next);
        return RINSWF_OK;
    } catch(const std::bad_alloc&) {return RINSWF_OUT_OF_MEMORY;}
      catch(...) {return RINSWF_INTERNAL_ERROR;}
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
