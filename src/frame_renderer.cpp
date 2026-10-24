// SPDX-License-Identifier: MIT
#include "rinswf/frame_renderer.hpp"
#include "rinswf/jpeg.hpp"
#include <cmath>
namespace rinswf {
RenderResult render_frame(const FrameSnapshot& frame,const FrameAssets& assets,
                          Surface& target,StrokeLimits limits) {
    Surface stage=target;
    for(const auto& entry:frame.objects) {
        const auto& m=entry.matrix;
        if(entry.has_matrix && (m.scale_x!=1 || m.scale_y!=1 ||
            m.rotate_skew_0!=0 || m.rotate_skew_1!=0))
            return {Error::malformed_tag,"unsupported transform"};
        const auto tx=entry.has_matrix?m.translate_x:0;
        const auto ty=entry.has_matrix?m.translate_y:0;
        auto it=assets.shapes.find(entry.character_id);
        if(it!=assets.shapes.end()) {
            auto a=draw_shape_fills(stage,it->second,tx,ty,&assets.bitmaps);
            if(!a)return a;
            auto b=draw_shape_strokes(stage,it->second,tx,ty,limits);
            if(!b)return b;
        } else if(const auto* bmp=assets.bitmaps.find(entry.character_id)) {
            if(tx%20 || ty%20) return {Error::malformed_tag,"fractional bitmap offset"};
            auto a=blit_bitmap(stage,*bmp,tx/20,ty/20);
            if(!a)return a;
        } else return {Error::malformed_tag,"unknown placed character"};
    }
    target=std::move(stage);
    return {};
}
RenderResult collect_frame_assets(const Document& document,FrameAssets& output,
                                  std::size_t max_pixels) {
    FrameAssets stage;
    auto unique_id=[&](std::uint16_t id) {
        return !stage.shapes.contains(id) && !stage.bitmaps.images.contains(id);
    };
    for(const auto& tag:document.tags) {
        if(tag.code==2||tag.code==22||tag.code==32||tag.code==83) {
            auto r=decode_shape(tag);
            if(!r)return {r.error,r.message};
            if(!unique_id(r.shape.character_id))
                return {Error::malformed_tag,"duplicate character id"};
            stage.shapes.emplace(r.shape.character_id,std::move(r.shape));
        } else if(tag.code==20||tag.code==36) {
            auto r=decode_lossless(tag,max_pixels);
            if(!r)return {r.error,r.message};
            if(!unique_id(r.bitmap.character_id))
                return {Error::malformed_tag,"duplicate character id"};
            stage.bitmaps.images.emplace(r.bitmap.character_id,std::move(r.bitmap));
        } else if(tag.code==21||tag.code==35||tag.code==90) {
            auto asset=parse_jpeg_asset(tag);
            if(!asset)return {asset.error,asset.message};
            auto r=decode_jpeg_bitmap(asset.asset,max_pixels);
            if(!r)return {r.error,r.message};
            if(!unique_id(r.bitmap.character_id))
                return {Error::malformed_tag,"duplicate character id"};
            stage.bitmaps.images.emplace(r.bitmap.character_id,std::move(r.bitmap));
        }
    }
    output=std::move(stage);
    return {};
}
}
