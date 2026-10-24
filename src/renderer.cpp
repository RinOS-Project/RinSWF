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
}
