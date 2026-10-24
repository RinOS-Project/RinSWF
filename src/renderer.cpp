// SPDX-License-Identifier: MIT
#include "rinswf/renderer.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
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
    double t=fill.kind==FillKind::linear_gradient?(gx/16384.0+1.0)*0.5:
             std::hypot(gx,gy)/16384.0;
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
void segment(Surface& s,double x0,double y0,double x1,double y1,ShapeColor c) {
    // Basic aliased stroke; clip endpoints before rasterizing to avoid unbounded loops.
    const double dx=x1-x0,dy=y1-y0;
    const double count=std::max(std::abs(dx),std::abs(dy));
    if(!std::isfinite(count)||count>10000000.0) return;
    const auto steps=static_cast<std::uint32_t>(std::ceil(count));
    for(std::uint32_t i=0;i<=steps;++i) {
        const double t=steps?double(i)/steps:0.0;
        pixel(s,static_cast<std::int64_t>(std::llround(x0+dx*t)),
              static_cast<std::int64_t>(std::llround(y0+dy*t)),c);
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
                                std::int32_t offset_x,std::int32_t offset_y) {
    if(!valid(target)) return fail(Error::invalid_length,"invalid surface");
    ShapePoint previous{};
    for(const auto& command:shape.commands) {
        if(command.line>shape.lines.size()||command.fill0>shape.fills.size()||
           command.fill1>shape.fills.size()) return fail(Error::malformed_tag,"invalid style reference");
        auto to_x=[&](ShapePoint p){return (double(p.x)+offset_x)/20.0;};
        auto to_y=[&](ShapePoint p){return (double(p.y)+offset_y)/20.0;};
        if(command.kind!=ShapeCommandKind::move_to && command.line) {
            const auto color=shape.lines[command.line-1].color;
            if(command.kind==ShapeCommandKind::line_to) {
                segment(target,to_x(previous),to_y(previous),to_x(command.endpoint),to_y(command.endpoint),color);
            } else if(command.kind==ShapeCommandKind::quadratic_to) {
                ShapePoint start=previous;
                constexpr unsigned subdivisions=32;
                double lastx=to_x(start),lasty=to_y(start);
                for(unsigned i=1;i<=subdivisions;++i) {
                    const double t=double(i)/subdivisions,u=1.0-t;
                    const double cx=u*u*to_x(start)+2*u*t*to_x(command.control)+t*t*to_x(command.endpoint);
                    const double cy=u*u*to_y(start)+2*u*t*to_y(command.control)+t*t*to_y(command.endpoint);
                    segment(target,lastx,lasty,cx,cy,color);
                    lastx=cx;lasty=cy;
                }
            } else return fail(Error::malformed_tag,"unexpected shape command");
        }
        previous=command.endpoint;
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
            add(cmd.fill0,x0,y0,x1,y1);
            add(cmd.fill1,x0,y0,x1,y1);
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
           fill.kind!=FillKind::radial_gradient && !bitmap)
            return fail(Error::malformed_tag,"fill kind not yet rendered");
        if(bitmap) {
            const auto* bmp=bitmaps?bitmaps->find(fill.bitmap_id):nullptr;
            if(!bmp || !bmp->width || !bmp->height ||
               bmp->rgba.size()!=std::size_t(bmp->width)*bmp->height*4)
                return fail(Error::malformed_tag,"bitmap fill image unavailable");
        }
        if(fill.kind!=FillKind::solid) {
            if(!bitmap && (fill.stops.empty() || fill.interpolation!=0 || fill.spread>2))
                return fail(Error::malformed_tag,"unsupported gradient parameters");
            const auto& m=fill.matrix;
            if(!std::isfinite(m.scale_x)||!std::isfinite(m.scale_y)||
               !std::isfinite(m.skew_x)||!std::isfinite(m.skew_y)||
               std::abs(m.scale_x*m.scale_y-m.skew_x*m.skew_y)<1e-12)
                return fail(Error::malformed_tag,"singular gradient matrix");
        }
    }
    std::vector<double> intersections;
    for(std::size_t style=0;style<edges.size();++style) {
        if(edges[style].empty()) continue;
        for(std::uint32_t y=0;y<target.height;++y) {
            const double sy=double(y)+0.5;
            intersections.clear();
            for(const auto& edge:edges[style]) {
                if((edge.y0<=sy && sy<edge.y1)||(edge.y1<=sy && sy<edge.y0)) {
                    const double x=edge.x0+(sy-edge.y0)*(edge.x1-edge.x0)/(edge.y1-edge.y0);
                    if(std::isfinite(x)) intersections.push_back(x);
                }
            }
            std::sort(intersections.begin(),intersections.end());
            for(std::size_t i=0;i+1<intersections.size();i+=2) {
                const double left=std::max(0.0,intersections[i]);
                const double right=std::min(double(target.width),intersections[i+1]);
                if(left>=right) continue;
                const auto start=static_cast<std::uint32_t>(std::ceil(left-0.5));
                const auto end=static_cast<std::uint32_t>(std::ceil(right-0.5));
                for(std::uint32_t x=start;x<end;++x)
                    pixel(target,x,y,sample_fill(shape.fills[style],double(x)+0.5-double(offset_x)/20.0,double(y)+0.5-double(offset_y)/20.0,bitmaps));
            }
        }
    }
    return {};
}

}
