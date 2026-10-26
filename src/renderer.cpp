// SPDX-License-Identifier: MIT
#include "rinswf/renderer.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <array>
#include <unordered_map>
namespace rinswf {
namespace {
RenderResult fail(Error error,const char* message) {return {error,message};}
bool valid(const Surface& s) {
    return s.width && s.height && std::size_t(s.width)<=std::numeric_limits<std::size_t>::max()/s.height/4
        && s.rgba.size()==std::size_t(s.width)*s.height*4;
}
void pixel(Surface& s,std::int64_t x,std::int64_t y,ShapeColor c) {
    if(x<0||y<0||x>=s.width||y>=s.height||!c.a) return;
    const std::size_t p=(std::size_t(y)*s.width+std::size_t(x))*4;
    const unsigned sa=c.a,da=s.rgba[p+3];
    const unsigned outa=sa+(da*(255-sa)+127)/255;
    if(!outa) return;
    for(unsigned k=0;k<3;++k) {
        const unsigned src=k==0?c.r:k==1?c.g:c.b;
        const unsigned dst=s.rgba[p+k];
        const unsigned premul=src*sa+(dst*da*(255-sa)+127)/255;
        s.rgba[p+k]=static_cast<std::uint8_t>((premul+outa/2)/outa);
    }
    s.rgba[p+3]=static_cast<std::uint8_t>(outa);
}
ShapeColor sample_fill(const ShapeFill& fill,double px,double py,const BitmapLibrary* library) {
    if(fill.kind==FillKind::solid) return fill.color;
    // Gradient space is -16384..16384 in each axis; MATRIX translation is twips.
    const auto& m=fill.matrix;
    const double det=m.scale_x*m.scale_y-m.skew_x*m.skew_y;
    if(!std::isfinite(det)||std::abs(det)<1e-12) return {};
    const double x=px*20.0-m.translate_x, y=py*20.0-m.translate_y;
    const double gx=(m.scale_y*x-m.skew_x*y)/det;
    const double gy=(-m.skew_y*x+m.scale_x*y)/det;
    if(fill.kind==FillKind::repeating_bitmap || fill.kind==FillKind::clipped_bitmap ||
       fill.kind==FillKind::nonsmoothed_repeating_bitmap || fill.kind==FillKind::nonsmoothed_clipped_bitmap) {
        const Bitmap* bmp=library?library->find(fill.bitmap_id):nullptr;
        if(!bmp || !bmp->width || !bmp->height) return {};
        const bool repeat=fill.kind==FillKind::repeating_bitmap ||
                          fill.kind==FillKind::nonsmoothed_repeating_bitmap;
        const bool smooth=fill.kind==FillKind::repeating_bitmap ||
                          fill.kind==FillKind::clipped_bitmap;
        auto wrap=[](std::int64_t v,std::int64_t n){return (v%n+n)%n;};
        auto texel=[&](std::int64_t ix,std::int64_t iy)->ShapeColor {
            if(repeat) {ix=wrap(ix,bmp->width);iy=wrap(iy,bmp->height);}
            else if(ix<0 || iy<0 || ix>=bmp->width || iy>=bmp->height)
                return {0,0,0,0};
            const auto p=(std::size_t(iy)*bmp->width+std::size_t(ix))*4;
            return {bmp->rgba[p],bmp->rgba[p+1],bmp->rgba[p+2],bmp->rgba[p+3]};
        };
        if(!smooth) return texel(static_cast<std::int64_t>(std::floor(gx)),
                                 static_cast<std::int64_t>(std::floor(gy)));
        // Bilinear interpolation in premultiplied-alpha space.
        const double fx=gx-0.5,fy=gy-0.5;
        const auto ix=static_cast<std::int64_t>(std::floor(fx));
        const auto iy=static_cast<std::int64_t>(std::floor(fy));
        const double tx=fx-ix,ty=fy-iy;
        const ShapeColor c[4]={texel(ix,iy),texel(ix+1,iy),
                               texel(ix,iy+1),texel(ix+1,iy+1)};
        const double w[4]={(1-tx)*(1-ty),tx*(1-ty),(1-tx)*ty,tx*ty};
        double alpha=0,r=0,g=0,blue=0;
        for(unsigned i=0;i<4;++i) {
            const double a=double(c[i].a)/255.0;
            alpha+=w[i]*a;
            r+=w[i]*a*c[i].r;
            g+=w[i]*a*c[i].g;
            blue+=w[i]*a*c[i].b;
        }
        if(alpha<=0) return {0,0,0,0};
        auto channel=[](double v)->std::uint8_t {
            return static_cast<std::uint8_t>(std::clamp(std::lround(v),0L,255L));
        };
        return {channel(r/alpha),channel(g/alpha),channel(blue/alpha),
                channel(alpha*255.0)};
    }
    double t=0.0;
    if(fill.kind==FillKind::linear_gradient) {
        t=(gx/16384.0+1.0)*0.5;
    } else if(fill.kind==FillKind::focal_gradient) {
        const double f=fill.focal_point;
        const double px=gx/16384.0,py=gy/16384.0;
        const double vx=px-f,vy=py, vv=vx*vx+vy*vy;
        if(vv>1e-24) {
            const double dot=f*vx;
            const double discriminant=dot*dot+(1-f*f)*vv;
            const double denominator=-dot+std::sqrt(std::max(0.0,discriminant));
            t=denominator>1e-24?vv/denominator:1.0;
        }
    } else {
        t=std::hypot(gx,gy)/16384.0;
    }
    if(fill.spread==1) { t=std::fmod(t,2.0);if(t<0)t+=2.0;if(t>1)t=2-t; }
    else if(fill.spread==2) {t=std::fmod(t,1.0);if(t<0)t+=1.0;}
    else t=std::clamp(t,0.0,1.0);
    const double ratio=t*255.0;
    const auto& stops=fill.stops;
    if(ratio<=stops.front().ratio) return stops.front().color;
    for(std::size_t i=1;i<stops.size();++i) {
        if(ratio>stops[i].ratio) continue;
        const auto& a=stops[i-1];const auto& b=stops[i];
        const double d=b.ratio==a.ratio?1.0:(ratio-a.ratio)/(b.ratio-a.ratio);
        auto mix=[&](std::uint8_t av,std::uint8_t bv) {
            return static_cast<std::uint8_t>(std::clamp(std::lround(av+(bv-av)*d),0L,255L));
        };
        return {mix(a.color.r,b.color.r),mix(a.color.g,b.color.g),
                mix(a.color.b,b.color.b),mix(a.color.a,b.color.a)};
    }
    return stops.back().color;
}
struct CoverageMask {
    static constexpr std::uint32_t tile_size=64;
    std::uint32_t width=0,height=0;
    std::int64_t origin_x=0,origin_y=0;
    using Tile=std::array<std::uint8_t,tile_size*tile_size>;
    // Packed tile coordinates, with per-mask dimensions capped by uint32.
    std::unordered_map<std::uint64_t,Tile> tiles;
    std::size_t* allocated_tiles=nullptr;
    std::size_t max_tiles=0;
    bool exceeded=false;
};
void mask_pixel(CoverageMask& s,std::int64_t x,std::int64_t y,ShapeColor c) {
    if(!c.a||x<s.origin_x||y<s.origin_y||x>=s.origin_x+s.width||y>=s.origin_y+s.height) return;
    const auto lx=std::uint32_t(x-s.origin_x),ly=std::uint32_t(y-s.origin_y);
    const auto tx=lx/CoverageMask::tile_size,ty=ly/CoverageMask::tile_size;
    const auto key=(std::uint64_t(ty)<<32)|tx;
    auto it=s.tiles.find(key);
    if(it==s.tiles.end()) {
        if(!s.allocated_tiles || *s.allocated_tiles>=s.max_tiles) {s.exceeded=true;return;}
        it=s.tiles.emplace(key,CoverageMask::Tile{}).first;
        ++*s.allocated_tiles;
    }
    const auto p=std::size_t(ly%CoverageMask::tile_size)*CoverageMask::tile_size+(lx%CoverageMask::tile_size);
    it->second[p]=std::max(it->second[p],c.a);
}
void segment(CoverageMask& s,double x0,double y0,double x1,double y1,
             ShapeColor c,double width,unsigned start_cap,unsigned end_cap) {
    // Coverage-AA stroke with independent start/end cap clipping.
    // Evaluate distance within a clipped bounding box, not along an unbounded line.
    const double radius=std::max(0.75,width*0.5);
    if(!std::isfinite(x0)||!std::isfinite(y0)||!std::isfinite(x1)||
       !std::isfinite(y1)||!std::isfinite(radius)) return;
    const double lo_x=std::min(x0,x1)-radius-1.0;
    const double hi_x=std::max(x0,x1)+radius+1.0;
    const double lo_y=std::min(y0,y1)-radius-1.0;
    const double hi_y=std::max(y0,y1)+radius+1.0;
    if(hi_x<s.origin_x || hi_y<s.origin_y || lo_x>=s.origin_x+s.width || lo_y>=s.origin_y+s.height) return;
    const auto left=static_cast<std::int64_t>(std::max(double(s.origin_x),std::floor(lo_x)));
    const auto right=static_cast<std::int64_t>(std::min(double(s.origin_x+s.width),std::ceil(hi_x)));
    const auto top=static_cast<std::int64_t>(std::max(double(s.origin_y),std::floor(lo_y)));
    const auto bottom=static_cast<std::int64_t>(std::min(double(s.origin_y+s.height),std::ceil(hi_y)));
    const double dx=x1-x0,dy=y1-y0,denom=dx*dx+dy*dy;
    const double length=std::sqrt(denom);
    for(auto y=top;y<bottom;++y) for(auto x=left;x<right;++x) {
        // 4x4 coverage AA for each stroke segment, blended once per pixel.
        // Note: adjacent segments are still composited independently.
        unsigned covered=0;
        for(unsigned sy=0;sy<4;++sy) for(unsigned sx=0;sx<4;++sx) {
            const double px=double(x)+(double(sx)+0.5)/4.0;
            const double py=double(y)+(double(sy)+0.5)/4.0;
            if(length==0) {
                if(start_cap==0 && std::hypot(px-x0,py-y0)<=radius) ++covered;
                continue;
            }
            const double along=((px-x0)*dx+(py-y0)*dy)/length;
            const double perpendicular=std::abs((px-x0)*dy-(py-y0)*dx)/length;
            bool inside=false;
            if(along<0) {
                if(start_cap==0) inside=std::hypot(px-x0,py-y0)<=radius;
                else if(start_cap==2) inside=along>=-radius && perpendicular<=radius;
            } else if(along>length) {
                if(end_cap==0) inside=std::hypot(px-x1,py-y1)<=radius;
                else if(end_cap==2) inside=along<=length+radius && perpendicular<=radius;
            } else inside=perpendicular<=radius;
            if(inside) ++covered;
        }
        if(covered) {
            ShapeColor sample=c;
            sample.a=static_cast<std::uint8_t>((255u*covered+8)/16);
            mask_pixel(s,x,y,sample);
        }
    }
}
// Rasterize a join wedge with the same coverage scheme as line segments.
void join_wedge(CoverageMask& surface,double ax,double ay,double bx,double by,
                double cx,double cy,ShapeColor color) {
    const double minx=std::max(double(surface.origin_x),std::floor(std::min({ax,bx,cx})-1));
    const double maxx=std::min(double(surface.origin_x+surface.width),std::ceil(std::max({ax,bx,cx})+1));
    const double miny=std::max(double(surface.origin_y),std::floor(std::min({ay,by,cy})-1));
    const double maxy=std::min(double(surface.origin_y+surface.height),std::ceil(std::max({ay,by,cy})+1));
    const auto cross=[](double ux,double uy,double vx,double vy){return ux*vy-uy*vx;};
    const double area=cross(bx-ax,by-ay,cx-ax,cy-ay);
    if(std::abs(area)<1e-10) return;
    for(auto y=std::int64_t(miny);y<std::int64_t(maxy);++y)
        for(auto x=std::int64_t(minx);x<std::int64_t(maxx);++x) {
            unsigned hits=0;
            for(unsigned iy=0;iy<4;++iy) for(unsigned ix=0;ix<4;++ix) {
                const double px=x+(ix+0.5)/4,py=y+(iy+0.5)/4;
                const double u=cross(bx-ax,by-ay,px-ax,py-ay)/area;
                const double v=cross(cx-bx,cy-by,px-bx,py-by)/area;
                const double w=cross(ax-cx,ay-cy,px-cx,py-cy)/area;
                if(u>=0 && v>=0 && w>=0) ++hits;
            }
            if(hits) {
                auto c=color;
                c.a=std::uint8_t((255u*hits+8)/16);
                mask_pixel(surface,x,y,c);
            }
        }
}
void stroke_join(CoverageMask& surface,ShapePoint a,ShapePoint b,ShapePoint c,
                 std::int32_t offset_x,std::int32_t offset_y,const ShapeLine& style) {
    const double ax=(double(a.x)+offset_x)/20,ay=(double(a.y)+offset_y)/20;
    const double bx=(double(b.x)+offset_x)/20,by=(double(b.y)+offset_y)/20;
    const double cx=(double(c.x)+offset_x)/20,cy=(double(c.y)+offset_y)/20;
    const double ux=bx-ax,uy=by-ay,vx=cx-bx,vy=cy-by;
    const double ul=std::hypot(ux,uy),vl=std::hypot(vx,vy);
    if(ul<1e-10 || vl<1e-10) return;
    const double turn=ux*vy-uy*vx;
    if(std::abs(turn)<1e-10) return;
    const double sign=turn>0?-1.0:1.0;
    const double radius=std::max(.75,double(style.width?style.width:20)/40);
    const double nx=sign*-uy/ul,ny=sign*ux/ul;
    const double mx=sign*-vy/vl,my=sign*vx/vl;
    const double p1x=bx+radius*nx,p1y=by+radius*ny;
    const double p2x=bx+radius*mx,p2y=by+radius*my;
    if(style.join==1) {
        join_wedge(surface,bx,by,p1x,p1y,p2x,p2y,style.color);
        return;
    }
    if(style.join!=2) return;
    const double determinant=ux/ul*vy/vl-uy/ul*vx/vl;
    if(std::abs(determinant)<1e-10) return;
    const double d1x=ux/ul,d1y=uy/ul,d2x=vx/vl,d2y=vy/vl;
    const double rx=p2x-p1x,ry=p2y-p1y;
    const double t=(rx*d2y-ry*d2x)/determinant;
    const double tipx=p1x+t*d1x,tipy=p1y+t*d1y;
    const double ratio=std::hypot(tipx-bx,tipy-by)/radius;
    if(!std::isfinite(ratio)||ratio>std::max(1.0,style.miter_limit)) {
        join_wedge(surface,bx,by,p1x,p1y,p2x,p2y,style.color);
    } else {
        join_wedge(surface,p1x,p1y,tipx,tipy,p2x,p2y,style.color);
    }
}
}
RenderResult make_surface(std::uint32_t width,std::uint32_t height,Surface& output,std::size_t limit) {
    if(!width||!height||height>limit/width) return fail(Error::size_limit,"surface size limit");
    if(std::size_t(width)*height>std::numeric_limits<std::size_t>::max()/4)
        return fail(Error::size_limit,"surface allocation overflow");
    Surface next;
    next.width=width;next.height=height;
    next.rgba.resize(std::size_t(width)*height*4);
    output=std::move(next);
    return {};
}
void clear(Surface& s,ShapeColor c) noexcept {
    if(!valid(s)) return;
    for(std::size_t p=0;p<s.rgba.size();p+=4) {
        s.rgba[p]=c.r;s.rgba[p+1]=c.g;s.rgba[p+2]=c.b;s.rgba[p+3]=c.a;
    }
}
RenderResult blit_bitmap(Surface& target,const Bitmap& bitmap,std::int32_t x,std::int32_t y) {
    if(!valid(target)) return fail(Error::invalid_length,"invalid surface");
    if(!bitmap.width||!bitmap.height||bitmap.rgba.size()!=std::size_t(bitmap.width)*bitmap.height*4)
        return fail(Error::invalid_length,"invalid bitmap");
    for(std::size_t by=0;by<bitmap.height;++by) for(std::size_t bx=0;bx<bitmap.width;++bx) {
        const std::size_t p=(by*bitmap.width+bx)*4;
        pixel(target,std::int64_t(x)+bx,std::int64_t(y)+by,
              {bitmap.rgba[p],bitmap.rgba[p+1],bitmap.rgba[p+2],bitmap.rgba[p+3]});
    }
    return {};
}
RenderResult draw_shape_strokes(Surface& target,const ShapeGeometry& shape,
                                std::int32_t offset_x,std::int32_t offset_y,StrokeLimits limits,const BitmapLibrary* bitmaps) {
    if(!valid(target)) return fail(Error::invalid_length,"invalid surface");
    for(const auto& line:shape.lines) {
        if(line.has_fill && (line.fill.kind==FillKind::repeating_bitmap ||
            line.fill.kind==FillKind::clipped_bitmap ||
            line.fill.kind==FillKind::nonsmoothed_repeating_bitmap ||
            line.fill.kind==FillKind::nonsmoothed_clipped_bitmap) &&
            (!bitmaps || !bitmaps->find(line.fill.bitmap_id)))
            return fail(Error::malformed_tag,"bitmap stroke fill asset missing");
    }
    if(!limits.max_tiles || limits.max_mask_bytes<CoverageMask::tile_size*CoverageMask::tile_size)
        return fail(Error::size_limit,"stroke mask budget too small");
    std::size_t allocated_tiles=0;
    const std::size_t permitted_tiles=std::min(limits.max_tiles,
        limits.max_mask_bytes/(CoverageMask::tile_size*CoverageMask::tile_size));
    // A per-style coverage buffer prevents double-alpha compositing at joints.
    std::vector<CoverageMask> masks(shape.lines.size());
    // Conservative per-style bounds include endpoints, Bezier control points and joins.
    struct Bounds { double minx=0,miny=0,maxx=0,maxy=0; bool used=false; };
    std::vector<Bounds> bounds(shape.lines.size());
    ShapePoint cursor{};
    for(const auto& command:shape.commands) {
        if(command.line>shape.lines.size() || command.fill0>shape.fills.size() ||
           command.fill1>shape.fills.size())
            return fail(Error::malformed_tag,"invalid style reference");
        if(command.kind==ShapeCommandKind::move_to) {cursor=command.endpoint;continue;}
        if(command.kind!=ShapeCommandKind::line_to && command.kind!=ShapeCommandKind::quadratic_to)
            return fail(Error::malformed_tag,"unknown stroke command");
        if(command.line) {
            auto& bound=bounds[command.line-1];
            auto add=[&](ShapePoint p) {
                const double x=(double(p.x)+offset_x)/20.0;
                const double y=(double(p.y)+offset_y)/20.0;
                if(!bound.used) {bound={x,y,x,y,true};}
                else {bound.minx=std::min(bound.minx,x);bound.maxx=std::max(bound.maxx,x);
                      bound.miny=std::min(bound.miny,y);bound.maxy=std::max(bound.maxy,y);}
            };
            add(cursor);add(command.endpoint);
            if(command.kind==ShapeCommandKind::quadratic_to) add(command.control);
        }
        cursor=command.endpoint;
    }
    for(std::size_t i=0;i<bounds.size();++i) {
        const auto& bound=bounds[i];
        if(!bound.used) continue;
        const auto& line=shape.lines[i];
        const double radius=std::max(0.75,double(line.width?line.width:20)/40.0);
        const double extension=radius*std::clamp(line.join==2?line.miter_limit:1.0,1.0,1000.0)+2.0;
        const auto clampx=[&](double v){return std::clamp(v,0.0,double(target.width));};
        const auto clampy=[&](double v){return std::clamp(v,0.0,double(target.height));};
        const auto left=std::int64_t(clampx(std::floor(bound.minx-extension)));
        const auto top=std::int64_t(clampy(std::floor(bound.miny-extension)));
        const auto right=std::int64_t(clampx(std::ceil(bound.maxx+extension)));
        const auto bottom=std::int64_t(clampy(std::ceil(bound.maxy+extension)));
        if(right<=left || bottom<=top) continue;
        auto& mask=masks[i];
        mask.allocated_tiles=&allocated_tiles;
        mask.max_tiles=permitted_tiles;
        mask.origin_x=left;mask.origin_y=top;
        mask.width=std::uint32_t(right-left);mask.height=std::uint32_t(bottom-top);
        // Sparse 64x64 tiles are allocated only when a pixel is covered.
    }
    std::vector<bool> closed_edges(shape.commands.size(),false);
    for(std::size_t i=0;i<shape.commands.size();) {
        if(shape.commands[i].kind!=ShapeCommandKind::move_to) {++i;continue;}
        std::size_t end=i+1;
        while(end<shape.commands.size() && shape.commands[end].kind!=ShapeCommandKind::move_to) ++end;
        if(end>i+2) {
            const auto& first=shape.commands[i].endpoint;
            const auto& last=shape.commands[end-1].endpoint;
            const auto line=shape.commands[i+1].line;
            bool one_style=line!=0;
            for(std::size_t j=i+1;j<end;++j) if(shape.commands[j].line!=line) one_style=false;
            if(one_style && first.x==last.x && first.y==last.y)
                for(std::size_t j=i+1;j<end;++j) closed_edges[j]=true;
        }
        i=end;
    }
    // Join the last edge to the first edge of each explicitly closed line-only path.
    for(std::size_t i=0;i<shape.commands.size();) {
        if(shape.commands[i].kind!=ShapeCommandKind::move_to) {++i;continue;}
        std::size_t end=i+1;
        while(end<shape.commands.size() && shape.commands[end].kind!=ShapeCommandKind::move_to) ++end;
        if(end>=i+4 && closed_edges[i+1]) {
            const auto style_index=shape.commands[i+1].line-1;
            const auto& style=shape.lines[style_index];
            const auto& last=shape.commands[end-1];
            const auto& before_last=shape.commands[end-2];
            const auto& first=shape.commands[i];
            const auto& after_first=shape.commands[i+1];
            if(masks[style_index].width) {
                // The join helper expects one point on each incoming/outgoing tangent.
                const ShapePoint incoming=last.kind==ShapeCommandKind::quadratic_to
                    ? (last.control.x==first.endpoint.x && last.control.y==first.endpoint.y
                       ? before_last.endpoint : last.control)
                    : before_last.endpoint;
                const ShapePoint outgoing=after_first.kind==ShapeCommandKind::quadratic_to
                    ? (after_first.control.x==first.endpoint.x && after_first.control.y==first.endpoint.y
                       ? after_first.endpoint : after_first.control)
                    : after_first.endpoint;
                if(style.join==0) {
                    const double x=(double(first.endpoint.x)+offset_x)/20.0;
                    const double y=(double(first.endpoint.y)+offset_y)/20.0;
                    const double width=style.width?double(style.width)/20.0:1.0;
                    segment(masks[style_index],x,y,x,y,style.color,width,0,0);
                } else {
                    stroke_join(masks[style_index],incoming,
                                first.endpoint,outgoing,
                                offset_x,offset_y,style);
                }
            }
        }
        i=end;
    }
    // Internal vertices of a continuous subpath should not use external caps.
    ShapePoint previous{},previous_tangent{};
    bool have_tangent=false;
    std::uint32_t previous_line=0;
    bool have_prior_segment=false;
    for(const auto& command:shape.commands) {
        if(command.line>shape.lines.size()||command.fill0>shape.fills.size()||
           command.fill1>shape.fills.size()) return fail(Error::malformed_tag,"invalid style reference");
        auto to_x=[&](ShapePoint p){return (double(p.x)+offset_x)/20.0;};
        auto to_y=[&](ShapePoint p){return (double(p.y)+offset_y)/20.0;};
        if(command.kind==ShapeCommandKind::move_to) {
            have_prior_segment=false;
            previous_line=0;
            have_tangent=false;
        }
        if(command.kind!=ShapeCommandKind::move_to && command.line &&
           masks[command.line-1].width!=0) {
            const auto& line_style=shape.lines[command.line-1];
            const auto color=line_style.color;
            const double stroke_width=line_style.width?double(line_style.width)/20.0:1.0;
            if(have_prior_segment && have_tangent && previous_line==command.line && line_style.join!=0) {
                const auto index=std::size_t(&command-shape.commands.data());
                const auto& incoming=shape.commands[index-1];
                // For a quadratic edge, the end/start control point defines its tangent.
                // Fall back to the chord when the control point is coincident with the join.
                const ShapePoint before=incoming.kind==ShapeCommandKind::quadratic_to &&
                    (incoming.control.x!=previous.x || incoming.control.y!=previous.y)
                    ? incoming.control : previous_tangent;
                const ShapePoint after=command.kind==ShapeCommandKind::quadratic_to &&
                    (command.control.x!=previous.x || command.control.y!=previous.y)
                    ? command.control : command.endpoint;
                stroke_join(masks[command.line-1],before,previous,after,
                            offset_x,offset_y,line_style);
            }
            if(have_prior_segment && previous_line==command.line && line_style.join==0) {
                // Round join: bridge the adjacent stroked edges at their shared vertex.
                // A coverage union for the complete path is a separate follow-up.
                segment(masks[command.line-1],to_x(previous),to_y(previous),
                        to_x(previous),to_y(previous),color,stroke_width,0,0);
            }
            const auto index=std::size_t(&command-shape.commands.data());
            // An explicit move-to separates paths even when the style ID matches.
            const bool next_same=index+1<shape.commands.size() &&
                shape.commands[index+1].kind!=ShapeCommandKind::move_to &&
                shape.commands[index+1].line==command.line;
            const auto start_cap=(closed_edges[index] || (have_prior_segment && previous_line==command.line))?1u:unsigned(line_style.start_cap);
            const auto end_cap=(closed_edges[index] || next_same)?1u:unsigned(line_style.end_cap);
            if(command.kind==ShapeCommandKind::line_to) {
                segment(masks[command.line-1],to_x(previous),to_y(previous),to_x(command.endpoint),to_y(command.endpoint),color,stroke_width,start_cap,end_cap);
            } else if(command.kind==ShapeCommandKind::quadratic_to) {
                ShapePoint start=previous;
                constexpr unsigned subdivisions=32;
                double lastx=to_x(start),lasty=to_y(start);
                for(unsigned i=1;i<=subdivisions;++i) {
                    const double t=double(i)/subdivisions,u=1.0-t;
                    const double cx=u*u*to_x(start)+2*u*t*to_x(command.control)+t*t*to_x(command.endpoint);
                    const double cy=u*u*to_y(start)+2*u*t*to_y(command.control)+t*t*to_y(command.endpoint);
                    segment(masks[command.line-1],lastx,lasty,cx,cy,color,stroke_width,i==1?start_cap:1,i==subdivisions?end_cap:1);
                    lastx=cx;lasty=cy;
                }
            } else return fail(Error::malformed_tag,"unexpected shape command");
        }
        if(command.kind!=ShapeCommandKind::move_to) {
            have_prior_segment=command.line!=0;
            previous_line=command.line;
            previous_tangent=command.kind==ShapeCommandKind::quadratic_to
                ? command.control : previous;
            if(previous_tangent.x==command.endpoint.x && previous_tangent.y==command.endpoint.y)
                previous_tangent=previous;
            have_tangent=command.line!=0;
        }
        previous=command.endpoint;
    }
    for(std::size_t style=0;style<masks.size();++style) {
        const auto& mask=masks[style];
        if(mask.exceeded) return fail(Error::size_limit,"stroke mask tile limit exceeded");
    }
    for(std::size_t style=0;style<masks.size();++style) {
        const auto& mask=masks[style];
        const auto& line_style=shape.lines[style];
        const auto color=line_style.color;
        for(const auto& [key,tile]:mask.tiles) {
            for(std::uint32_t y=0;y<CoverageMask::tile_size;++y)
                for(std::uint32_t x=0;x<CoverageMask::tile_size;++x) {
                    const auto coverage=tile[std::size_t(y)*CoverageMask::tile_size+x];
                    if(!coverage) continue;
                    const auto gx=mask.origin_x+std::int64_t(std::uint32_t(key))*CoverageMask::tile_size+x;
                    const auto gy=mask.origin_y+std::int64_t(key>>32)*CoverageMask::tile_size+y;
                    if(gx>=target.width||gy>=target.height) continue;
                    auto sample=line_style.has_fill?
                        sample_fill(line_style.fill,
                            double(gx)+0.5-double(offset_x)/20.0,
                            double(gy)+0.5-double(offset_y)/20.0,bitmaps):color;
                    sample.a=std::uint8_t((unsigned(sample.a)*coverage+127)/255);
                    pixel(target,gx,gy,sample);
                }
        }
    }
    return {};
}

RenderResult draw_shape_fills(Surface& target,const ShapeGeometry& shape,
                              std::int32_t offset_x,std::int32_t offset_y,const BitmapLibrary* bitmaps) {
    if(!valid(target)) return fail(Error::invalid_length,"invalid surface");
    struct Edge {double x0,y0,x1,y1;};
    std::vector<std::vector<Edge>> edges(shape.fills.size());
    ShapePoint previous{};
    auto to_x=[&](ShapePoint p){return (double(p.x)+offset_x)/20.0;};
    auto to_y=[&](ShapePoint p){return (double(p.y)+offset_y)/20.0;};
    auto add=[&](std::uint32_t index,double x0,double y0,double x1,double y1) {
        if(index) edges[index-1].push_back({x0,y0,x1,y1});
    };
    for(const auto& cmd:shape.commands) {
        if(cmd.fill0>shape.fills.size() || cmd.fill1>shape.fills.size())
            return fail(Error::malformed_tag,"fill style index out of range");
        if(cmd.kind==ShapeCommandKind::move_to) {previous=cmd.endpoint;continue;}
        if(cmd.kind!=ShapeCommandKind::line_to && cmd.kind!=ShapeCommandKind::quadratic_to)
            return fail(Error::malformed_tag,"unknown shape command");
        const unsigned steps=cmd.kind==ShapeCommandKind::line_to?1:32;
        double x0=to_x(previous),y0=to_y(previous);
        for(unsigned i=1;i<=steps;++i) {
            const double t=double(i)/steps,u=1.0-t;
            const double x1=steps==1?to_x(cmd.endpoint):
                u*u*to_x(previous)+2*u*t*to_x(cmd.control)+t*t*to_x(cmd.endpoint);
            const double y1=steps==1?to_y(cmd.endpoint):
                u*u*to_y(previous)+2*u*t*to_y(cmd.control)+t*t*to_y(cmd.endpoint);
            add(cmd.fill0,x1,y1,x0,y0); // left fill has reversed orientation
            add(cmd.fill1,x0,y0,x1,y1); // right fill retains edge orientation
            x0=x1;y0=y1;
        }
        previous=cmd.endpoint;
    }
    for(std::size_t style=0;style<edges.size();++style) {
        if(edges[style].empty()) continue;
        const auto& fill=shape.fills[style];
        const bool bitmap=fill.kind==FillKind::repeating_bitmap ||
            fill.kind==FillKind::clipped_bitmap ||
            fill.kind==FillKind::nonsmoothed_repeating_bitmap ||
            fill.kind==FillKind::nonsmoothed_clipped_bitmap;
        if(fill.kind!=FillKind::solid && fill.kind!=FillKind::linear_gradient &&
           fill.kind!=FillKind::radial_gradient &&
           fill.kind!=FillKind::focal_gradient && !bitmap)
            return fail(Error::malformed_tag,"fill kind not yet rendered");
        if(bitmap) {
            const auto* bmp=bitmaps?bitmaps->find(fill.bitmap_id):nullptr;
            if(!bmp || !bmp->width || !bmp->height ||
               bmp->rgba.size()!=std::size_t(bmp->width)*bmp->height*4)
                return fail(Error::malformed_tag,"bitmap fill image unavailable");
        }
        if(fill.kind!=FillKind::solid) {
            if(!bitmap && (fill.stops.empty() || fill.interpolation!=0 || fill.spread>2 ||
                           (fill.kind==FillKind::focal_gradient &&
                            (!std::isfinite(fill.focal_point) || std::abs(fill.focal_point)>=1.0))))
                return fail(Error::malformed_tag,"unsupported gradient parameters");
            const auto& m=fill.matrix;
            if(!std::isfinite(m.scale_x)||!std::isfinite(m.scale_y)||
               !std::isfinite(m.skew_x)||!std::isfinite(m.skew_y)||
               std::abs(m.scale_x*m.scale_y-m.skew_x*m.skew_y)<1e-12)
                return fail(Error::malformed_tag,"singular gradient matrix");
        }
    }
    struct Crossing {double x; int delta;};
    std::vector<Crossing> intersections;
    for(std::size_t style=0;style<edges.size();++style) {
        if(edges[style].empty()) continue;
        // Limit work to scanlines intersecting the shape, rather than full-surface height.
        double min_y=double(target.height),max_y=0.0;
        for(const auto& edge:edges[style]) {
            min_y=std::min(min_y,std::min(edge.y0,edge.y1));
            max_y=std::max(max_y,std::max(edge.y0,edge.y1));
        }
        const auto ybegin=static_cast<std::uint32_t>(
            std::clamp(std::floor(min_y),0.0,double(target.height)));
        const auto yend=static_cast<std::uint32_t>(
            std::clamp(std::ceil(max_y),0.0,double(target.height)));
        for(std::uint32_t y=ybegin;y<yend;++y) {
            const double sy=double(y)+0.5;
            intersections.clear();
            for(const auto& edge:edges[style]) {
                if((edge.y0<=sy && sy<edge.y1)||(edge.y1<=sy && sy<edge.y0)) {
                    const double x=edge.x0+(sy-edge.y0)*(edge.x1-edge.x0)/(edge.y1-edge.y0);
                    if(std::isfinite(x)) intersections.push_back({x,edge.y1>edge.y0?1:-1});
                }
            }
            std::sort(intersections.begin(),intersections.end(),
                      [](const Crossing& a,const Crossing& b){return a.x<b.x;});
            int winding=0;
            for(std::size_t i=0;i<intersections.size();) {
                const double left=intersections[i].x;
                int change=0;
                // Group coincident intersections to avoid artificial one-pixel spans.
                while(i<intersections.size() && intersections[i].x==left)
                    change+=intersections[i++].delta;
                winding+=change;
                if(!winding || i==intersections.size()) continue;
                const double right=intersections[i].x;
                const double clipped_left=std::max(0.0,left);
                const double clipped_right=std::min(double(target.width),right);
                if(clipped_left>=clipped_right) continue;
                const auto start=static_cast<std::uint32_t>(std::ceil(clipped_left-0.5));
                const auto end=static_cast<std::uint32_t>(std::ceil(clipped_right-0.5));
                for(std::uint32_t x=start;x<end;++x)
                    pixel(target,x,y,sample_fill(shape.fills[style],double(x)+0.5-double(offset_x)/20.0,double(y)+0.5-double(offset_y)/20.0,bitmaps));
            }
        }
    }
    return {};
}

}
