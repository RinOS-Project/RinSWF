// SPDX-License-Identifier: MIT
#include "rinswf/frame_renderer.hpp"
#include "rinswf/jpeg.hpp"
#include <cmath>
#include <algorithm>
#include <limits>
namespace rinswf {
static RenderResult affine_bitmap(Surface& dst,const Bitmap& bmp,const Matrix& m) {
    const double a=m.scale_x,b=m.rotate_skew_0,c=m.rotate_skew_1,d=m.scale_y;
    const double det=a*d-b*c;
    if(!std::isfinite(det)||std::abs(det)<1e-12)
        return {Error::malformed_tag,"singular bitmap matrix"};
    const double tx=m.translate_x/20.0,ty=m.translate_y/20.0;
    auto transform=[&](double x,double y){return std::pair<double,double>{a*x+b*y+tx,c*x+d*y+ty};};
    const auto p0=transform(0,0),p1=transform(bmp.width,0);
    const auto p2=transform(0,bmp.height),p3=transform(bmp.width,bmp.height);
    const double left=std::min({p0.first,p1.first,p2.first,p3.first});
    const double right=std::max({p0.first,p1.first,p2.first,p3.first});
    const double top=std::min({p0.second,p1.second,p2.second,p3.second});
    const double bottom=std::max({p0.second,p1.second,p2.second,p3.second});
    if(!std::isfinite(left)||!std::isfinite(right)||!std::isfinite(top)||!std::isfinite(bottom))
        return {Error::malformed_tag,"nonfinite bitmap bounds"};
    const auto x0=std::int64_t(std::max(0.0,std::floor(left)));
    const auto x1=std::int64_t(std::min(double(dst.width),std::ceil(right)));
    const auto y0=std::int64_t(std::max(0.0,std::floor(top)));
    const auto y1=std::int64_t(std::min(double(dst.height),std::ceil(bottom)));
    if(bmp.rgba.size()!=std::size_t(bmp.width)*bmp.height*4)
        return {Error::invalid_length,"invalid bitmap"};
    for(auto y=y0;y<y1;++y) for(auto x=x0;x<x1;++x) {
        const double px=x+0.5-tx,py=y+0.5-ty;
        const double sx=(d*px-b*py)/det,sy=(-c*px+a*py)/det;
        if(sx<0||sy<0||sx>=bmp.width||sy>=bmp.height)continue;
        const auto source=(std::size_t(sy)*bmp.width+std::size_t(sx))*4;
        const auto out=(std::size_t(y)*dst.width+std::size_t(x))*4;
        const unsigned alpha=bmp.rgba[source+3],dstalpha=dst.rgba[out+3];
        const unsigned total=alpha+(dstalpha*(255-alpha)+127)/255;
        if(!total)continue;
        for(unsigned k=0;k<3;++k) {
            const unsigned premul=unsigned(bmp.rgba[source+k])*alpha+
                (unsigned(dst.rgba[out+k])*dstalpha*(255-alpha)+127)/255;
            dst.rgba[out+k]=std::uint8_t((premul+total/2)/total);
        }
        dst.rgba[out+3]=std::uint8_t(total);
    }
    return {};
}
RenderResult render_frame(const FrameSnapshot& frame,const FrameAssets& assets,
                          Surface& target,StrokeLimits limits) {
    Surface stage=target;
    for(const auto& entry:frame.objects) {
        const auto& m=entry.matrix;
        const bool affine=entry.has_matrix && (m.scale_x!=1 || m.scale_y!=1 ||
            m.rotate_skew_0!=0 || m.rotate_skew_1!=0);
        const auto tx=entry.has_matrix?m.translate_x:0;
        const auto ty=entry.has_matrix?m.translate_y:0;
        auto it=assets.shapes.find(entry.character_id);
        if(it!=assets.shapes.end()) {
            if(affine) return {Error::malformed_tag,"shape affine transform unsupported"};
            auto a=draw_shape_fills(stage,it->second,tx,ty,&assets.bitmaps);
            if(!a)return a;
            auto b=draw_shape_strokes(stage,it->second,tx,ty,limits);
            if(!b)return b;
        } else if(const auto* bmp=assets.bitmaps.find(entry.character_id)) {
            auto a=affine || tx%20 || ty%20
                ? affine_bitmap(stage,*bmp,m) : blit_bitmap(stage,*bmp,tx/20,ty/20);
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
