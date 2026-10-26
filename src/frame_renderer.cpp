// SPDX-License-Identifier: MIT
#include "rinswf/frame_renderer.hpp"
#include "rinswf/jpeg.hpp"
#include <cmath>
#include <algorithm>
#include <limits>
#include <array>
#include <bit>
#include <unordered_map>
#include <unordered_set>
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
        // Near-singular finite matrices can still overflow inverse coordinates.
        // Never convert infinity or NaN to an integer pixel index.
        if(!std::isfinite(sx)||!std::isfinite(sy)||
           sx<0||sy<0||sx>=bmp.width||sy>=bmp.height)continue;
        // Bilinear filtering uses premultiplied alpha to prevent edge halos.
        const double fx=sx-0.5,fy=sy-0.5;
        const auto ix=std::int64_t(std::floor(fx)),iy=std::int64_t(std::floor(fy));
        const double ux=fx-ix,uy=fy-iy;
        const double weights[4]={(1-ux)*(1-uy),ux*(1-uy),(1-ux)*uy,ux*uy};
        const std::int64_t xs[4]={ix,ix+1,ix,ix+1};
        const std::int64_t ys[4]={iy,iy,iy+1,iy+1};
        double alpha_f=0,premul[3]={};
        for(unsigned i=0;i<4;++i) {
            if(xs[i]<0||ys[i]<0||xs[i]>=bmp.width||ys[i]>=bmp.height)continue;
            const auto pos=(std::size_t(ys[i])*bmp.width+std::size_t(xs[i]))*4;
            const double a=double(bmp.rgba[pos+3])*weights[i]/255.0;
            alpha_f+=a;
            for(unsigned k=0;k<3;++k) premul[k]+=a*bmp.rgba[pos+k];
        }
        const auto out=(std::size_t(y)*dst.width+std::size_t(x))*4;
        const unsigned alpha=unsigned(std::clamp(std::lround(alpha_f*255),0L,255L));
        const unsigned dstalpha=dst.rgba[out+3];
        const unsigned total=alpha+(dstalpha*(255-alpha)+127)/255;
        if(!total)continue;
        for(unsigned k=0;k<3;++k) {
            const unsigned source_premul=unsigned(std::clamp(std::lround(premul[k]*255.0),0L,65025L));
            const unsigned combined=source_premul+
                (unsigned(dst.rgba[out+k])*dstalpha*(255-alpha)+127)/255;
            dst.rgba[out+k]=std::uint8_t((combined+total/2)/total);
        }
        dst.rgba[out+3]=std::uint8_t(total);
    }
    return {};
}
static RenderResult vector_shape(Surface& dst,const ShapeGeometry& shape,
                                 const BitmapLibrary& bitmaps,const Matrix& m,
                                 StrokeLimits limits) {
    // Stroke widths remain isotropic only under a similarity transform.
    const double sx2=m.scale_x*m.scale_x+m.rotate_skew_1*m.rotate_skew_1;
    const double sy2=m.rotate_skew_0*m.rotate_skew_0+m.scale_y*m.scale_y;
    const double dot=m.scale_x*m.rotate_skew_0+m.rotate_skew_1*m.scale_y;
    if(!std::isfinite(sx2)||!std::isfinite(sy2)||!std::isfinite(dot))
        return {Error::malformed_tag,"invalid shape transform"};
    const double scale=std::sqrt(sx2);
    const double tolerance=1e-7*std::max(1.0,std::max(sx2,sy2));
    if(!shape.lines.empty() && (std::abs(sx2-sy2)>tolerance||
       std::abs(dot)>tolerance||scale<1e-8))
        return {Error::malformed_tag,"anisotropic vector stroke unsupported"};
    for(const auto& fill:shape.fills)
        if(fill.kind!=FillKind::solid)
            return {Error::malformed_tag,"vector gradient or bitmap fill unsupported"};
    ShapeGeometry transformed=shape;
    auto apply=[&](ShapePoint& p)->bool {
        const double x=double(p.x),y=double(p.y);
        const double nx=m.scale_x*x+m.rotate_skew_0*y+m.translate_x;
        const double ny=m.rotate_skew_1*x+m.scale_y*y+m.translate_y;
        constexpr double bound=double(std::numeric_limits<std::int32_t>::max())-1;
        if(!std::isfinite(nx)||!std::isfinite(ny)||
           std::abs(nx)>bound||std::abs(ny)>bound) return false;
        p.x=std::int32_t(std::llround(nx));
        p.y=std::int32_t(std::llround(ny));
        return true;
    };
    for(auto& cmd:transformed.commands) {
        if(!apply(cmd.endpoint))return {Error::size_limit,"transformed point overflow"};
        if(cmd.kind==ShapeCommandKind::quadratic_to && !apply(cmd.control))
            return {Error::size_limit,"transformed control point overflow"};
    }
    for(auto& line:transformed.lines) {
        const auto width=std::round(double(line.width)*scale);
        if(!std::isfinite(width)||width>65535.0)
            return {Error::size_limit,"transformed line width out of range"};
        line.width=std::uint16_t(width);
    }
    auto fill=draw_shape_fills(dst,transformed,0,0,&bitmaps);
    if(!fill)return fill;
    return draw_shape_strokes(dst,transformed,0,0,limits);
}
// Flatten quadratic centerlines before building affine stroke contours.
// The tolerance is measured after the placement transform in screen pixels.
static RenderResult flatten_affine_stroke(const ShapeGeometry& input,const Matrix& m,
                                          ShapeGeometry& output) {
    output=input;
    output.commands.clear();
    ShapePoint cursor{};
    std::size_t segments=0;
    for(const auto& cmd:input.commands) {
        if(cmd.kind==ShapeCommandKind::move_to) {
            output.commands.push_back(cmd);
            cursor=cmd.endpoint;
            continue;
        }
        if(cmd.kind==ShapeCommandKind::line_to) {
            output.commands.push_back(cmd);
            cursor=cmd.endpoint;
            if(++segments>100000) return {Error::size_limit,"stroke segment limit"};
            continue;
        }
        if(cmd.kind!=ShapeCommandKind::quadratic_to)
            return {Error::malformed_tag,"unsupported stroke command"};
        const double ax=double(cursor.x),ay=double(cursor.y);
        const double bx=double(cmd.control.x),by=double(cmd.control.y);
        const double cx=double(cmd.endpoint.x),cy=double(cmd.endpoint.y);
        const double dx=ax-2*bx+cx,dy=ay-2*by+cy;
        const double bend=std::hypot(m.scale_x*dx+m.rotate_skew_0*dy,
                                     m.rotate_skew_1*dx+m.scale_y*dy)/20.0;
        // For a quadratic Bezier, the maximum chord deviation of each
        // uniform subdivision is bounded by bend/(4*N*N) pixels.
        // Choose N for <= 0.125 px geometric deviation.
        if(!std::isfinite(bend)) return {Error::size_limit,"nonfinite stroke curvature"};
        const double requested=std::ceil(std::sqrt(std::max(0.0,bend)*2.0));
        if(requested>128.0)
            return {Error::size_limit,"stroke curvature exceeds subdivision limit"};
        const unsigned steps=std::max(1u,unsigned(requested));
        if(segments+steps>100000) return {Error::size_limit,"stroke segment limit"};
        for(unsigned j=1;j<=steps;++j) {
            const double t=double(j)/steps,u=1-t;
            const double px=u*u*ax+2*u*t*bx+t*t*cx;
            const double py=u*u*ay+2*u*t*by+t*t*cy;
            ShapeCommand segment=cmd;
            segment.kind=ShapeCommandKind::line_to;
            segment.control={};
            if(!std::isfinite(px)||!std::isfinite(py) ||
               px<std::numeric_limits<std::int32_t>::min() ||
               px>std::numeric_limits<std::int32_t>::max() ||
               py<std::numeric_limits<std::int32_t>::min() ||
               py>std::numeric_limits<std::int32_t>::max())
                return {Error::size_limit,"flattened stroke coordinate overflow"};
            segment.endpoint={std::int32_t(std::llround(px)),std::int32_t(std::llround(py))};
            if(j==steps)segment.endpoint=cmd.endpoint;
            // Rounding to twips can produce coincident subdivision points.
            // Omit them to avoid zero-length edges and spurious joins.
            const auto& last=output.commands.back().endpoint;
            if(segment.endpoint.x!=last.x || segment.endpoint.y!=last.y)
                output.commands.push_back(segment);
        }
        segments+=steps;
        cursor=cmd.endpoint;
    }
    return {};
}
static RenderResult affine_straight_stroke(Surface& dst,const ShapeGeometry& shape,
                                           const Matrix& m,StrokeLimits limits) {
    // Transform each straight edge's rectangular outline into one fill geometry.
    // The common nonzero-winding pass keeps opaque segment overlaps solid.
    if(shape.lines.size()!=1 || !shape.fills.empty() ||
       shape.commands.size()<2)
        return {Error::malformed_tag,"unsupported affine stroke geometry"};
    // Track the open/closed state of each subpath for cap handling.
    std::vector<bool> closed_commands(shape.commands.size(),false);
    for(std::size_t i=0;i<shape.commands.size();) {
        if(shape.commands[i].kind!=ShapeCommandKind::move_to) {++i;continue;}
        std::size_t end=i+1;
        while(end<shape.commands.size() &&
              shape.commands[end].kind!=ShapeCommandKind::move_to)++end;
        if(end>i+2 && shape.commands[end-1].endpoint.x==shape.commands[i].endpoint.x &&
           shape.commands[end-1].endpoint.y==shape.commands[i].endpoint.y)
            for(std::size_t j=i+1;j<end;++j)closed_commands[j]=true;
        i=end;
    }
    ShapeGeometry quad;
    auto opaque=shape.lines[0].color;
    opaque.a=255;
    quad.fills.push_back({opaque});
    ShapePoint cursor{};
    bool have_cursor=false;
    for(std::size_t edge_index=0;edge_index<shape.commands.size();++edge_index) {
        const auto& edge=shape.commands[edge_index];
        if(edge.kind==ShapeCommandKind::move_to) {
            cursor=edge.endpoint;have_cursor=true;continue;
        }
        if(!have_cursor || edge.kind!=ShapeCommandKind::line_to || edge.line!=1)
            return {Error::malformed_tag,"unsupported affine stroke edge"};
        const auto& start=cursor;
        const auto& end=edge.endpoint;
    const double dx=double(end.x)-start.x,dy=double(end.y)-start.y;
    const double length=std::hypot(dx,dy);
    if(length<1e-8) {cursor=end;continue;}
    const double radius=double(shape.lines[0].width?shape.lines[0].width:20)*0.5;
    const double nx=-dy/length*radius,ny=dx/length*radius;
    const bool first_edge=edge_index>0 && shape.commands[edge_index-1].kind==ShapeCommandKind::move_to;
    const bool last_edge=edge_index+1==shape.commands.size() ||
        shape.commands[edge_index+1].kind==ShapeCommandKind::move_to;
    const double extension_start=first_edge && !closed_commands[edge_index] && shape.lines[0].start_cap==2?radius:0;
    const double extension_end=last_edge && !closed_commands[edge_index] && shape.lines[0].end_cap==2?radius:0;
    const double tx=dx/length,ty=dy/length;
    const double corners[4][2]={{start.x+nx-tx*extension_start,start.y+ny-ty*extension_start},
                                {end.x+nx+tx*extension_end,end.y+ny+ty*extension_end},
                                {end.x-nx+tx*extension_end,end.y-ny+ty*extension_end},
                                {start.x-nx-tx*extension_start,start.y-ny-ty*extension_start}};
    for(unsigned i=0;i<5;++i) {
        const auto& v=corners[i%4];
        const double x=m.scale_x*v[0]+m.rotate_skew_0*v[1]+m.translate_x;
        const double y=m.rotate_skew_1*v[0]+m.scale_y*v[1]+m.translate_y;
        constexpr double bound=double(std::numeric_limits<std::int32_t>::max())-1;
        if(!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>bound||std::abs(y)>bound)
            return {Error::size_limit,"stroke corner overflow"};
        ShapeCommand cmd;
        cmd.kind=i==0?ShapeCommandKind::move_to:ShapeCommandKind::line_to;
        cmd.endpoint={std::int32_t(std::llround(x)),std::int32_t(std::llround(y))};
        cmd.fill1=i==0?0:1;
        quad.commands.push_back(cmd);
    }
        cursor=end;
    }
    // Connect consecutive straight segments with an outer bevel wedge.
    // Degenerate and non-finite joins are skipped instead of inventing geometry.
    if(shape.lines[0].join<=2 || shape.lines[0].start_cap==0 ||
       shape.lines[0].end_cap==0) {
        auto add_triangle=[&](double ax,double ay,double bx,double by,
                              double cx,double cy)->bool {
            const double points[4][2]={{ax,ay},{bx,by},{cx,cy},{ax,ay}};
            for(unsigned j=0;j<4;++j) {
                const double x=m.scale_x*points[j][0]+m.rotate_skew_0*points[j][1]+m.translate_x;
                const double y=m.rotate_skew_1*points[j][0]+m.scale_y*points[j][1]+m.translate_y;
                constexpr double bound=double(std::numeric_limits<std::int32_t>::max())-1;
                if(!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>bound||std::abs(y)>bound)
                    return false;
                ShapeCommand cmd;
                cmd.kind=j==0?ShapeCommandKind::move_to:ShapeCommandKind::line_to;
                cmd.endpoint={std::int32_t(std::llround(x)),std::int32_t(std::llround(y))};
                cmd.fill1=j==0?0:1;
                quad.commands.push_back(cmd);
            }
            return true;
        };
        // The caps belong only to the endpoints of each open subpath.
        constexpr double pi=3.14159265358979323846;
        for(std::size_t i=1;i<shape.commands.size();++i) {
            const auto& edge=shape.commands[i];
            if(edge.kind!=ShapeCommandKind::line_to ||
               shape.commands[i-1].kind!=ShapeCommandKind::move_to) continue;
            std::size_t end=i;
            while(end+1<shape.commands.size() &&
                  shape.commands[end+1].kind==ShapeCommandKind::line_to) ++end;
            const auto& start=shape.commands[i-1].endpoint;
            const auto& finish=shape.commands[end].endpoint;
            if(closed_commands[i] || (start.x==finish.x && start.y==finish.y)) continue;
            const auto emit_cap=[&](ShapePoint center,ShapePoint toward,bool beginning)->bool {
                const double dx=double(toward.x)-center.x;
                const double dy=double(toward.y)-center.y;
                const double length=std::hypot(dx,dy);
                if(length<1e-8)return true;
                const double base=std::atan2(dy,dx)+(beginning?pi:0);
                const double radius=double(shape.lines[0].width?shape.lines[0].width:20)*0.5;
                for(unsigned j=0;j<24;++j) {
                    const double a0=base-pi/2+pi*j/24;
                    const double a1=base-pi/2+pi*(j+1)/24;
                    if(!add_triangle(center.x,center.y,
                        center.x+radius*std::cos(a0),center.y+radius*std::sin(a0),
                        center.x+radius*std::cos(a1),center.y+radius*std::sin(a1)))
                        return false;
                }
                return true;
            };
            if(shape.lines[0].start_cap==0 && !emit_cap(start,shape.commands[i].endpoint,true))
                return {Error::size_limit,"round start cap overflow"};
            if(shape.lines[0].end_cap==0 && !emit_cap(finish,shape.commands[end-1].endpoint,true))
                return {Error::size_limit,"round end cap overflow"};
            i=end;
        }
        auto join_corner=[&](const ShapePoint& a,const ShapePoint& b,
                             const ShapePoint& c)->RenderResult {
            const double ux=double(b.x)-a.x,uy=double(b.y)-a.y;
            const double vx=double(c.x)-b.x,vy=double(c.y)-b.y;
            const double ul=std::hypot(ux,uy),vl=std::hypot(vx,vy);
            if(ul<1e-8||vl<1e-8) return {};
            const double cross=ux*vy-uy*vx;
            // Nearly collinear subdivisions of the same curve do not
            // need an explicit join wedge; the edge rectangles already meet.
            if(std::abs(cross)<=ul*vl*1e-4) return {};
            const double sign=cross>0?-1.0:1.0;
            const double radius=double(shape.lines[0].width?shape.lines[0].width:20)*0.5;
            const double p1x=b.x-sign*uy/ul*radius,p1y=b.y+sign*ux/ul*radius;
            const double p2x=b.x-sign*vy/vl*radius,p2y=b.y+sign*vx/vl*radius;
            if(shape.lines[0].join==0) {
                constexpr double pi=3.14159265358979323846;
                const double angle=std::atan2(p1y-b.y,p1x-b.x);
                double sweep=std::remainder(std::atan2(p2y-b.y,p2x-b.x)-angle,2*pi);
                const unsigned steps=std::clamp(unsigned(std::ceil(std::abs(sweep)/(pi/24))),1u,64u);
                for(unsigned j=0;j<steps;++j) {
                    const double a0=angle+sweep*double(j)/steps;
                    const double a1=angle+sweep*double(j+1)/steps;
                    if(!add_triangle(b.x,b.y,b.x+radius*std::cos(a0),
                        b.y+radius*std::sin(a0),b.x+radius*std::cos(a1),
                        b.y+radius*std::sin(a1)))
                        return {Error::size_limit,"round join coordinate overflow"};
                }
                return {};
            }
            if(shape.lines[0].join==2) {
                const double d1x=ux/ul,d1y=uy/ul,d2x=vx/vl,d2y=vy/vl;
                const double det=d1x*d2y-d1y*d2x;
                if(std::abs(det)>1e-12) {
                    const double rx=p2x-p1x,ry=p2y-p1y;
                    const double t=(rx*d2y-ry*d2x)/det;
                    const double mx=p1x+t*d1x,my=p1y+t*d1y;
                    const double ratio=std::hypot(mx-b.x,my-b.y)/radius;
                    if(std::isfinite(ratio) &&
                       ratio<=std::max(1.0,shape.lines[0].miter_limit)) {
                        if(!add_triangle(p1x,p1y,mx,my,p2x,p2y))
                            return {Error::size_limit,"miter join coordinate overflow"};
                        return {};
                    }
                }
            }
            if(!add_triangle(b.x,b.y,p1x,p1y,p2x,p2y))
                return {Error::size_limit,"bevel join coordinate overflow"};
            return {};
        };
        for(std::size_t i=2;i<shape.commands.size();++i) {
            const auto& before=shape.commands[i-1];
            const auto& next=shape.commands[i];
            if(before.kind!=ShapeCommandKind::line_to ||
               next.kind!=ShapeCommandKind::line_to)continue;
            const auto& earlier=shape.commands[i-2];
            if(earlier.kind!=ShapeCommandKind::move_to &&
               earlier.kind!=ShapeCommandKind::line_to)continue;
            auto result=join_corner(earlier.endpoint,before.endpoint,next.endpoint);
            if(!result)return result;
        }
        // Each explicitly closed subpath needs the final-to-first join too.
        for(std::size_t i=0;i<shape.commands.size();) {
            if(shape.commands[i].kind!=ShapeCommandKind::move_to) {++i;continue;}
            std::size_t end=i+1;
            while(end<shape.commands.size() &&
                  shape.commands[end].kind!=ShapeCommandKind::move_to)++end;
            if(end>=i+4 &&
               shape.commands[end-1].endpoint.x==shape.commands[i].endpoint.x &&
               shape.commands[end-1].endpoint.y==shape.commands[i].endpoint.y) {
                auto result=join_corner(shape.commands[end-2].endpoint,
                                        shape.commands[i].endpoint,
                                        shape.commands[i+1].endpoint);
                if(!result)return result;
            }
            i=end;
        }
    }
    // Union all generated contour masks. A single nonzero-winding pass may
    // cancel two identical contours traversed in opposite directions.
    // Keep a single union surface; each contour is rasterized in its own
    // clipped bounding rectangle rather than clearing a full-frame scratch.
    // Union surface is clipped to the union of transformed contour bounds.
    double minx=double(dst.width),miny=double(dst.height),maxx=0,maxy=0;
    for(const auto& cmd:quad.commands) {
        const double x=double(cmd.endpoint.x)/20.0,y=double(cmd.endpoint.y)/20.0;
        minx=std::min(minx,x);miny=std::min(miny,y);
        maxx=std::max(maxx,x);maxy=std::max(maxy,y);
    }
    const auto mask_left=std::int64_t(std::clamp(std::floor(minx)-1.0,0.0,double(dst.width)));
    const auto mask_top=std::int64_t(std::clamp(std::floor(miny)-1.0,0.0,double(dst.height)));
    const auto mask_right=std::int64_t(std::clamp(std::ceil(maxx)+1.0,0.0,double(dst.width)));
    const auto mask_bottom=std::int64_t(std::clamp(std::ceil(maxy)+1.0,0.0,double(dst.height)));
    if(mask_right<=mask_left || mask_bottom<=mask_top)return {};
    const auto mask_width=std::size_t(mask_right-mask_left);
    const auto mask_height=std::size_t(mask_bottom-mask_top);
    constexpr std::size_t tile_side=64;
    using CoverageTile=std::array<std::uint16_t,tile_side*tile_side>;
    std::unordered_map<std::uint64_t,CoverageTile> coverage;
    // Apply caller-selected limits to the affine union path too.
    const std::size_t max_tiles=std::min(limits.max_tiles,
        limits.max_mask_bytes/(tile_side*tile_side*sizeof(std::uint16_t)));
    // Empty and fully clipped contours need no mask allocation.
    auto tile_key=[](std::uint32_t x,std::uint32_t y)->std::uint64_t {
        return (std::uint64_t(y)<<32)|x;
    };
    ShapeGeometry one;
    one.fills=quad.fills;
    std::size_t contours=0;
    auto flush=[&]()->RenderResult {
        if(one.commands.empty())return {};
        if(++contours>8192)return {Error::size_limit,"stroke contour limit exceeded"};
        if(one.commands.size()<4) {one.commands.clear();return {};}
        double minx=double(dst.width),miny=double(dst.height),maxx=0,maxy=0;
        for(const auto& cmd:one.commands) {
            const double x=double(cmd.endpoint.x)/20.0,y=double(cmd.endpoint.y)/20.0;
            minx=std::min(minx,x);maxx=std::max(maxx,x);
            miny=std::min(miny,y);maxy=std::max(maxy,y);
        }
        const auto left=std::int64_t(std::clamp(std::floor(minx)-1.0,0.0,double(dst.width)));
        const auto top=std::int64_t(std::clamp(std::floor(miny)-1.0,0.0,double(dst.height)));
        const auto right=std::int64_t(std::clamp(std::ceil(maxx)+1.0,0.0,double(dst.width)));
        const auto bottom=std::int64_t(std::clamp(std::ceil(maxy)+1.0,0.0,double(dst.height)));
        // Most generated pieces of an offscreen stroke never reach the union.
        // Skip the AET allocation and scanline setup for these contours.
        if(right<=left || bottom<=top) {
            one.commands.clear();
            return {};
        }
        struct WindingEdge {double ax,ay,bx,by,dx,dy;};
        std::vector<WindingEdge> edges;
        edges.reserve(one.commands.size());
        for(std::size_t j=1;j<one.commands.size();++j) {
            const auto& a=one.commands[j-1].endpoint;
            const auto& b=one.commands[j].endpoint;
            const double ax=double(a.x)/20.0,ay=double(a.y)/20.0;
            const double bx=double(b.x)/20.0,by=double(b.y)/20.0;
            if(ay==by)continue;
            edges.push_back({ax,ay,bx,by,bx-ax,by-ay});
        }
        // Evaluate the same 4x4 sample grid for every contour and OR masks.
        // This preserves two disjoint partial coverages within one pixel.
        std::uint64_t cached_key=std::numeric_limits<std::uint64_t>::max();
        CoverageTile* cached_tile=nullptr;
        // Edge events are indexed in quarter-pixel rows. Each edge enters
        // exactly once and leaves exactly once as the scanline advances.
        using EdgePtr=const WindingEdge*;
        const auto row_count=std::size_t(bottom-top)*4;
        // Only rows containing events consume memory, regardless of surface height.
        std::unordered_map<std::size_t,std::vector<EdgePtr>> starts,ends;
        starts.reserve(edges.size());
        ends.reserve(edges.size());
        for(const auto& e:edges) {
            const double low=std::min(e.ay,e.by),high=std::max(e.ay,e.by);
            const auto first=std::int64_t(std::ceil(4.0*(low-double(top))-0.5));
            const auto last=std::int64_t(std::ceil(4.0*(high-double(top))-0.5));
            const auto begin=std::size_t(std::clamp(first,std::int64_t(0),std::int64_t(row_count)));
            const auto finish=std::size_t(std::clamp(last,std::int64_t(0),std::int64_t(row_count)));
            if(begin<finish) {
                starts[begin].push_back(&e);
                ends[finish].push_back(&e);
            }
        }
        std::vector<EdgePtr> active_edges;
        std::unordered_map<EdgePtr,std::size_t> active_index;
        auto remove_active=[&](EdgePtr edge) {
            const auto it=active_index.find(edge);
            if(it==active_index.end())return;
            const auto index=it->second;
            const auto last=active_edges.size()-1;
            if(index!=last) {
                const auto tail=active_edges[last];
                active_edges[index]=tail;
                active_index[tail]=index;
            }
            active_edges.pop_back();
            active_index.erase(it);
        };
        auto insert_active=[&](EdgePtr edge) {
            if(active_index.contains(edge))return;
            active_index.emplace(edge,active_edges.size());
            active_edges.push_back(edge);
        };
        using Intersection=std::pair<EdgePtr,double>;
        std::vector<Intersection> ordered;
        double previous_py=0;
        bool have_previous_row=false;
        std::size_t incremental_rows=0;
        std::vector<Intersection> positioned;
        std::unordered_set<EdgePtr> active_set;
        std::array<std::vector<std::pair<double,int>>,4> crossings;
        std::array<std::vector<int>,4> suffix;
        for(auto y=top;y<bottom;++y) {
            for(auto& row:crossings) row.clear();
            for(auto& row:suffix) row.clear();
            for(unsigned sy=0;sy<4;++sy) {
                const auto row=std::size_t(y-top)*4+sy;
                if(const auto expired=ends.find(row);expired!=ends.end()) {
                    for(auto e:expired->second)remove_active(e);
                }
                if(const auto arriving=starts.find(row);arriving!=starts.end())
                    for(auto edge:arriving->second)insert_active(edge);
                // Process the event-updated live set immediately, avoiding
                // four copies of the active-edge vector per pixel row.
                const double py=double(y)+(sy+0.5)/4.0;
                // No active edges: invalidate the previous ordering and
                // avoid rebuilding hash tables for this empty sample row.
                if(active_edges.empty()) {
                    ordered.clear();
                    have_previous_row=false;
                    incremental_rows=0;
                    continue;
                }
                // Retain survivors in their prior order, append new edges,
                // then re-sort the actual X intersections to handle crossings.
                active_set.clear();
                active_set.reserve(active_edges.size());
                active_set.insert(active_edges.begin(),active_edges.end());
                positioned.clear();
                positioned.reserve(active_edges.size());
                for(const auto& entry:ordered) {
                    if(!active_set.erase(entry.first))continue;
                    const auto* e=entry.first;
                    // Intersections near a quarter-pixel sample boundary
                    // are re-evaluated from the original edge to avoid drift
                    // flipping antialiasing coverage after many rows.
                    const double slope=e->dx/e->dy;
                    double x=have_previous_row && incremental_rows<64
                        ?std::fma(py-previous_py,slope,entry.second)
                        :std::fma(py-e->ay,slope,e->ax);
                    // Compare against actual sample X positions (1/8, 3/8,
                    // 5/8, 7/8), not the quarter-pixel boundaries.
                    // Limit the phase check to visible intersections:
                    // very large offscreen coordinates cannot affect samples.
                    if(x>=double(left)-1.0 && x<=double(right)+1.0) {
                        const double sample_phase=4.0*x-0.5;
                        if(std::isfinite(sample_phase) &&
                           std::abs(sample_phase-std::round(sample_phase))<1e-6)
                            x=std::fma(py-e->ay,slope,e->ax);
                    }
                    if(!std::isfinite(x))
                        return {Error::size_limit,"nonfinite affine stroke intersection"};
                    positioned.emplace_back(e,x);
                }
                for(auto* e:active_edges) {
                    if(!active_set.erase(e))continue;
                    const double x=std::fma(py-e->ay,e->dx/e->dy,e->ax);
                    if(!std::isfinite(x))
                        return {Error::size_limit,"nonfinite affine stroke intersection"};
                    positioned.emplace_back(e,x);
                }
                auto less=[](const Intersection& a,const Intersection& b) {
                    return a.second<b.second;
                };
                if(positioned.size()>64) {
                    std::sort(positioned.begin(),positioned.end(),less);
                } else {
                    for(std::size_t k=1;k<positioned.size();++k) {
                        const auto value=positioned[k];
                        auto j=k;
                        while(j>0 && less(value,positioned[j-1])) {
                            positioned[j]=positioned[j-1];
                            --j;
                        }
                        positioned[j]=value;
                    }
                }
                ordered.swap(positioned);
                auto& row=crossings[sy];
                for(const auto& entry:ordered)
                    row.emplace_back(entry.second,entry.first->dy>0?1:-1);
                previous_py=py;
                have_previous_row=true;
                incremental_rows=(incremental_rows+1)%65;
                std::size_t write=0;
                for(std::size_t read=0;read<row.size();) {
                    const double x=row[read].first;
                    int delta=0;
                    do {delta+=row[read++].second;}
                    while(read<row.size() && row[read].first==x);
                    if(delta)row[write++]={x,delta};
                }
                row.resize(write);
                suffix[sy].assign(row.size()+1,0);
                for(std::size_t k=row.size();k>0;--k)
                    suffix[sy][k-1]=suffix[sy][k]+row[k-1].second;
            }
            // No coverage exists outside the extreme scanline intersections.
            // Restrict pixel work to rows with crossings and their X envelope.
            double row_min=std::numeric_limits<double>::infinity();
            double row_max=-std::numeric_limits<double>::infinity();
            bool has_crossings=false;
            for(const auto& sample_row:crossings) {
                if(sample_row.empty())continue;
                has_crossings=true;
                row_min=std::min(row_min,sample_row.front().first);
                row_max=std::max(row_max,sample_row.back().first);
            }
            if(!has_crossings)continue;
            const auto x_begin=std::int64_t(std::clamp(
                std::floor(row_min)-1.0,double(left),double(right)));
            const auto x_end=std::int64_t(std::clamp(
                std::ceil(row_max)+1.0,double(left),double(right)));
            if(x_end<=x_begin)continue;
            // X samples advance monotonically; each intersection is crossed
            // at most once per sample row instead of binary searching 4x/pixel.
            std::array<std::size_t,4> crossing_cursor{};
            for(unsigned sy=0;sy<4;++sy) {
                const auto& row=crossings[sy];
                crossing_cursor[sy]=std::size_t(std::upper_bound(
                    row.begin(),row.end(),double(x_begin)+0.125,
                    [](double x,const auto& item){return x<item.first;})-row.begin());
            }
            for(auto x=x_begin;x<x_end;++x) {
            std::uint16_t bits=0;
            for(unsigned sy=0;sy<4;++sy) for(unsigned sx=0;sx<4;++sx) {
                const double px=double(x)+(sx+0.5)/4.0;
                const auto& row=crossings[sy];
                auto& cursor=crossing_cursor[sy];
                while(cursor<row.size() && row[cursor].first<=px)++cursor;
                if(suffix[sy][cursor])bits|=std::uint16_t(1u<<(sy*4+sx));
            }
            if(!bits)continue;
            const auto ux=std::uint32_t(x-mask_left),uy=std::uint32_t(y-mask_top);
            const auto key=tile_key(ux/tile_side,uy/tile_side);
            if(key!=cached_key || !cached_tile) {
                auto it=coverage.find(key);
                if(it==coverage.end()) {
                    if(coverage.size()>=max_tiles)
                        return {Error::size_limit,"stroke union tile limit"};
                    it=coverage.emplace(key,CoverageTile{}).first;
                }
                cached_key=key;
                cached_tile=&it->second;
            }
            auto& sample_mask=(*cached_tile)[std::size_t(uy%tile_side)*tile_side+ux%tile_side];
            sample_mask|=bits;
            }
        }
        one.commands.clear();
        return {};
    };
    for(const auto& cmd:quad.commands) {
        if(cmd.kind==ShapeCommandKind::move_to) {
            auto result=flush();
            if(!result)return result;
        }
        one.commands.push_back(cmd);
    }
    auto result=flush();
    if(!result)return result;
    const unsigned opacity=shape.lines[0].color.a;
    const auto color=shape.lines[0].color;
    for(const auto& [key,tile]:coverage) {
        const auto base_x=std::size_t(std::uint32_t(key))*tile_side;
        const auto base_y=std::size_t(key>>32)*tile_side;
        for(std::size_t y=0;y<tile_side;++y) for(std::size_t x=0;x<tile_side;++x) {
            if(base_x+x>=mask_width || base_y+y>=mask_height)continue;
            const unsigned hits=std::popcount(tile[y*tile_side+x]);
            const unsigned sa=(hits*opacity+8)/16;
            if(!sa)continue;
            const auto xx=mask_left+std::int64_t(base_x+x);
            const auto yy=mask_top+std::int64_t(base_y+y);
            const auto di=(std::size_t(yy)*dst.width+std::size_t(xx))*4;
            const unsigned da=dst.rgba[di+3];
            const unsigned outa=sa+(da*(255-sa)+127)/255;
            for(unsigned k=0;k<3;++k) {
                const unsigned source=k==0?color.r:k==1?color.g:color.b;
                const unsigned premul=source*sa+
                    (unsigned(dst.rgba[di+k])*da*(255-sa)+127)/255;
                dst.rgba[di+k]=std::uint8_t((premul+outa/2)/outa);
            }
            dst.rgba[di+3]=std::uint8_t(outa);
        }
    }
    return {};
}
static RenderResult affine_shape(Surface& dst,const ShapeGeometry& shape,
                                 const BitmapLibrary& bitmaps,const Matrix& matrix,
                                 StrokeLimits limits) {
    // Rasterize in local coordinates, then transform the RGBA result.
    // This is an initial implementation; vector-space AA and stroke scaling
    // require a dedicated transformed-geometry rasterizer.
    if(shape.commands.empty()) return {};
    double xmin=0,ymin=0,xmax=0,ymax=0;
    bool first=true;
    auto include=[&](ShapePoint p) {
        const double x=double(p.x)/20.0,y=double(p.y)/20.0;
        if(first) {xmin=xmax=x;ymin=ymax=y;first=false;}
        else {xmin=std::min(xmin,x);xmax=std::max(xmax,x);
              ymin=std::min(ymin,y);ymax=std::max(ymax,y);}
    };
    for(const auto& cmd:shape.commands) {
        include(cmd.endpoint);
        if(cmd.kind==ShapeCommandKind::quadratic_to) include(cmd.control);
    }
    double margin=2;
    for(const auto& line:shape.lines) {
        const double radius=std::max(.75,double(line.width?line.width:20)/40.0);
        margin=std::max(margin,radius*std::clamp(
            line.join==2?line.miter_limit:1.0,1.0,1000.0)+2.0);
    }
    const double left=std::floor(xmin-margin),top=std::floor(ymin-margin);
    const double right=std::ceil(xmax+margin),bottom=std::ceil(ymax+margin);
    if(!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(right)||
       !std::isfinite(bottom)||left < -100000000||top < -100000000||
       right>100000000||bottom>100000000)
        return {Error::size_limit,"shape bounds out of range"};
    const auto width=right-left,height=bottom-top;
    if(width<1||height<1||width>65535||height>65535)
        return {Error::size_limit,"shape raster dimensions out of range"};
    Surface local;
    auto allocation=make_surface(std::uint32_t(width),std::uint32_t(height),local);
    if(!allocation) return allocation;
    const auto ox=std::int32_t(-left*20),oy=std::int32_t(-top*20);
    auto a=draw_shape_fills(local,shape,ox,oy,&bitmaps);
    if(!a)return a;
    auto b=draw_shape_strokes(local,shape,ox,oy,limits);
    if(!b)return b;
    Bitmap image;
    image.width=std::uint16_t(width);image.height=std::uint16_t(height);
    image.rgba=std::move(local.rgba);
    Matrix adjusted=matrix;
    const double tx=double(matrix.translate_x)+20.0*(matrix.scale_x*left+matrix.rotate_skew_0*top);
    const double ty=double(matrix.translate_y)+20.0*(matrix.rotate_skew_1*left+matrix.scale_y*top);
    if(!std::isfinite(tx)||!std::isfinite(ty)||
       tx<std::numeric_limits<std::int32_t>::min()||
       tx>std::numeric_limits<std::int32_t>::max()||
       ty<std::numeric_limits<std::int32_t>::min()||
       ty>std::numeric_limits<std::int32_t>::max())
        return {Error::size_limit,"transformed shape origin out of range"};
    adjusted.translate_x=std::int32_t(std::llround(tx));
    adjusted.translate_y=std::int32_t(std::llround(ty));
    return affine_bitmap(dst,image,adjusted);
}
// Combine child and parent SWF matrices (translation stays in twips).
static RenderResult combine_matrix(const Matrix& parent,const Matrix& child,
                                   Matrix& result) {
    result.scale_x=parent.scale_x*child.scale_x+
                   parent.rotate_skew_0*child.rotate_skew_1;
    result.rotate_skew_0=parent.scale_x*child.rotate_skew_0+
                         parent.rotate_skew_0*child.scale_y;
    result.rotate_skew_1=parent.rotate_skew_1*child.scale_x+
                         parent.scale_y*child.rotate_skew_1;
    result.scale_y=parent.rotate_skew_1*child.rotate_skew_0+
                   parent.scale_y*child.scale_y;
    const double tx=double(parent.translate_x)+
        parent.scale_x*child.translate_x+parent.rotate_skew_0*child.translate_y;
    const double ty=double(parent.translate_y)+
        parent.rotate_skew_1*child.translate_x+parent.scale_y*child.translate_y;
    if(!std::isfinite(tx)||!std::isfinite(ty)||
       tx<double(std::numeric_limits<std::int32_t>::min())||
       tx>double(std::numeric_limits<std::int32_t>::max())||
       ty<double(std::numeric_limits<std::int32_t>::min())||
       ty>double(std::numeric_limits<std::int32_t>::max())||
       !std::isfinite(result.scale_x)||!std::isfinite(result.scale_y)||
       !std::isfinite(result.rotate_skew_0)||!std::isfinite(result.rotate_skew_1))
        return {Error::size_limit,"sprite matrix overflow"};
    result.translate_x=std::int32_t(std::llround(tx));
    result.translate_y=std::int32_t(std::llround(ty));
    return {};
}
// Separable bounded box blur in premultiplied-alpha space; outside pixels
// contribute transparent black, so hard edges soften without color halos.
static void blur_surface(Surface& surface,const BlurFilter& filter) {
    for(unsigned pass=0;pass<filter.passes;++pass) {
        for(unsigned axis=0;axis<2;++axis) {
            const unsigned radius=axis==0?filter.radius_x:filter.radius_y;
            if(!radius)continue;
            auto source=surface.rgba;
            const unsigned divisor=2*radius+1;
            for(std::size_t y=0;y<surface.height;++y)
                for(std::size_t x=0;x<surface.width;++x) {
                    std::uint64_t alpha=0,rgb[3]={};
                    for(int delta=-int(radius);delta<=int(radius);++delta) {
                        const auto sx=std::int64_t(x)+(axis==0?delta:0);
                        const auto sy=std::int64_t(y)+(axis==1?delta:0);
                        if(sx<0 || sy<0 || sx>=surface.width || sy>=surface.height)
                            continue;
                        const auto index=(std::size_t(sy)*surface.width+std::size_t(sx))*4;
                        const unsigned a=source[index+3];
                        alpha+=a;
                        for(unsigned channel=0;channel<3;++channel)
                            rgb[channel]+=std::uint64_t(source[index+channel])*a;
                    }
                    const auto dest=(y*surface.width+x)*4;
                    const auto out_alpha=(alpha+divisor/2)/divisor;
                    for(unsigned channel=0;channel<3;++channel)
                        surface.rgba[dest+channel]=alpha?
                            std::uint8_t((rgb[channel]+alpha/2)/alpha):0;
                    surface.rgba[dest+3]=std::uint8_t(out_alpha);
                }
        }
    }
}
// Basic outer glow: blur the source silhouette, tint it, then place the
// unchanged source over it. The color channels remain straight-alpha.
static void glow_surface(Surface& surface,const GlowFilter& filter) {
    Surface halo=surface;
    blur_surface(halo,filter.blur);
    for(std::size_t i=0;i+3<surface.rgba.size();i+=4) {
        const unsigned ha=(unsigned(halo.rgba[i+3])*filter.alpha+127)/255;
        const unsigned sa=surface.rgba[i+3];
        const unsigned oa=sa+(ha*(255-sa)+127)/255;
        if(!oa)continue;
        const unsigned colors[3]={filter.red,filter.green,filter.blue};
        for(unsigned c=0;c<3;++c) {
            const unsigned numerator=unsigned(surface.rgba[i+c])*sa+
                (colors[c]*ha*(255-sa)+127)/255;
            surface.rgba[i+c]=std::uint8_t((numerator+oa/2)/oa);
        }
        surface.rgba[i+3]=std::uint8_t(oa);
    }
}
static RenderResult render_frame_impl(const FrameSnapshot& frame,
                                     const FrameAssets& assets,Surface& stage,
                                     StrokeLimits limits,unsigned nesting,std::size_t playback_frame,
                                     const SpritePlayback* sprite_controls=nullptr,
                                     std::vector<std::uint16_t> depth_path={}) {
    if(nesting>32)return {Error::size_limit,"sprite nesting limit exceeded"};
    for(const auto& entry:frame.objects) {
        // Transform the isolated source, never the already-composited backdrop.
        Surface isolated;
        Surface* output=&stage;
        if(entry.has_color_transform || entry.blend_mode!=1 || entry.has_blur || entry.has_glow) {
            isolated.width=stage.width;
            isolated.height=stage.height;
            isolated.rgba.assign(stage.rgba.size(),0);
            output=&isolated;
        }
        Surface& canvas=*output;
        const auto& m=entry.matrix;
        if(entry.has_matrix && (!std::isfinite(m.scale_x) ||
           !std::isfinite(m.scale_y) || !std::isfinite(m.rotate_skew_0) ||
           !std::isfinite(m.rotate_skew_1)))
            return {Error::malformed_tag,"nonfinite placement matrix"};
        const bool affine=entry.has_matrix && (m.scale_x!=1 || m.scale_y!=1 ||
            m.rotate_skew_0!=0 || m.rotate_skew_1!=0);
        const auto tx=entry.has_matrix?m.translate_x:0;
        const auto ty=entry.has_matrix?m.translate_y:0;
        auto it=assets.shapes.find(entry.character_id);
        if(it!=assets.shapes.end()) {
            if(affine) {
                bool direct=true;
                for(const auto& fill:it->second.fills)
                    if(fill.kind!=FillKind::solid) direct=false;
                if(!it->second.lines.empty()) {
                    const double sx2=m.scale_x*m.scale_x+m.rotate_skew_1*m.rotate_skew_1;
                    const double sy2=m.rotate_skew_0*m.rotate_skew_0+m.scale_y*m.scale_y;
                    const double dot=m.scale_x*m.rotate_skew_0+m.rotate_skew_1*m.scale_y;
                    const double tolerance=1e-7*std::max(1.0,std::max(sx2,sy2));
                    if(!std::isfinite(sx2)||!std::isfinite(sy2)||!std::isfinite(dot)||
                       sx2<1e-16||std::abs(sx2-sy2)>tolerance||
                       std::abs(dot)>tolerance) direct=false;
                }
                const auto& sh=it->second;
                bool stroke_outline=sh.lines.size()==1 && sh.fills.empty();
                bool has_move=false,has_edge=false;
                for(const auto& edge:sh.commands) {
                    if(edge.kind==ShapeCommandKind::move_to) has_move=true;
                    else if((edge.kind==ShapeCommandKind::line_to ||
                             edge.kind==ShapeCommandKind::quadratic_to) &&
                            edge.line==1 && has_move) has_edge=true;
                    else stroke_outline=false;
                }
                stroke_outline=stroke_outline && has_edge;
                RenderResult a;
                if(direct) a=vector_shape(canvas,sh,assets.bitmaps,m,limits);
                else if(stroke_outline) {
                    ShapeGeometry flattened;
                    a=flatten_affine_stroke(sh,m,flattened);
                    if(a) a=affine_straight_stroke(canvas,flattened,m,limits);
                } else a=affine_shape(canvas,sh,assets.bitmaps,m,limits);
                if(!a)return a;
            } else {
                auto a=draw_shape_fills(canvas,it->second,tx,ty,&assets.bitmaps);
                if(!a)return a;
                auto b=draw_shape_strokes(canvas,it->second,tx,ty,limits);
                if(!b)return b;
            }
        } else if(const auto sprite=assets.sprites.find(entry.character_id);
                  sprite!=assets.sprites.end()) {
            if(sprite->second.frames.empty())
                return {Error::malformed_tag,"sprite has no frames"};
            FrameSnapshot nested_frame;
            // Each placed sprite has its own local clock. A replacement with
            // a new character starts from its new placement frame.
            std::size_t local_frame=playback_frame>=entry.placed_frame
                ?playback_frame-entry.placed_frame:0;
            // An instance path distinguishes descendants belonging to
            // different parent clips, even when their depths are identical.
            auto child_path=depth_path;
            child_path.push_back(entry.depth);
            if(sprite_controls) {
                if(nesting==0) {
                    const auto control=sprite_controls->instances.find(entry.depth);
                    if(control!=sprite_controls->instances.end())
                        local_frame=control->second.frame;
                } else {
                    const auto control=sprite_controls->nested_instances.find(child_path);
                    if(control!=sprite_controls->nested_instances.end())
                        local_frame=control->second.frame;
                }
            }
            const auto& sprite_frame=sprite->second.frames[
                local_frame % sprite->second.frames.size()];
            nested_frame.objects.reserve(sprite_frame.objects.size());
            for(const auto& child:sprite_frame.objects) {
                DisplayEntry transformed=child;
                auto result=combine_matrix(m,child.matrix,transformed.matrix);
                if(!result)return result;
                transformed.has_matrix=true;
                nested_frame.objects.push_back(std::move(transformed));
            }
            auto result=render_frame_impl(nested_frame,assets,canvas,limits,nesting+1,
                                            local_frame,sprite_controls,std::move(child_path));
            if(!result)return result;
        } else if(const auto* bmp=assets.bitmaps.find(entry.character_id)) {
            auto a=affine || tx%20 || ty%20
                ? affine_bitmap(canvas,*bmp,m) : blit_bitmap(canvas,*bmp,tx/20,ty/20);
            if(!a)return a;
        } else return {Error::malformed_tag,"unknown placed character"};
        if(entry.has_blur)blur_surface(isolated,entry.blur);
        if(entry.has_glow)glow_surface(isolated,entry.glow);
        if(entry.has_color_transform || entry.blend_mode!=1 || entry.has_blur) {
            const auto& transform=entry.color_transform;
            for(std::size_t index=0;index+3<isolated.rgba.size();index+=4) {
                if(!isolated.rgba[index+3])continue;
                const auto out_channel=[&](unsigned channel)->unsigned {
                    if(!entry.has_color_transform)return isolated.rgba[index+channel];
                    const double value=std::clamp(
                        double(isolated.rgba[index+channel])*transform.multiply[channel]+
                        double(transform.add[channel]),0.0,255.0);
                    return unsigned(std::lround(value));
                };
                const unsigned sa=out_channel(3),da=stage.rgba[index+3];
                if(!sa)continue;
                const unsigned oa=sa+(da*(255-sa)+127)/255;
                for(unsigned channel=0;channel<3;++channel) {
                    const unsigned sc=out_channel(channel),dc=stage.rgba[index+channel];
                    unsigned blended=sc;
                    switch(entry.blend_mode) {
                        case 3: blended=(sc*dc+127)/255; break; // multiply
                        case 4: blended=255-((255-sc)*(255-dc)+127)/255; break; // screen
                        case 5: blended=std::max(sc,dc); break; // lighten
                        case 6: blended=std::min(sc,dc); break; // darken
                        case 7: blended=sc>dc?sc-dc:dc-sc; break; // difference
                        case 8: blended=std::min(255u,sc+dc); break; // add
                        case 9: blended=dc>sc?dc-sc:0; break; // subtract
                        case 10: blended=255u-dc; break; // invert backdrop
                        case 13: { // overlay: backdrop selects multiply or screen
                            blended=dc<128 ? (2*sc*dc+127)/255
                                :255-((2*(255-sc)*(255-dc)+127)/255);
                            break;
                        }
                        case 14: { // hard light: source selects multiply or screen
                            blended=sc<128 ? (2*sc*dc+127)/255
                                :255-((2*(255-sc)*(255-dc)+127)/255);
                            break;
                        }
                        case 13: blended=dc<128u
                            ?(2u*sc*dc+127u)/255u
                            :255u-(2u*(255u-sc)*(255u-dc)+127u)/255u;
                            break; // overlay: backdrop determines branch
                        case 14: blended=sc<128u
                            ?(2u*sc*dc+127u)/255u
                            :255u-(2u*(255u-sc)*(255u-dc)+127u)/255u;
                            break; // hardlight: source determines branch
                        default: break;
                    }
                    // W3C source-over blend: source-only, backdrop-only,
                    // and overlap contributions in premultiplied form.
                    const unsigned numerator=sc*sa*(255-da)+
                        dc*da*(255-sa)+blended*sa*da;
                    stage.rgba[index+channel]=std::uint8_t(
                        (numerator+oa*127)/(oa*255));
                }
                stage.rgba[index+3]=std::uint8_t(oa);
            }
        }
    }
    return {};
}
RenderResult render_frame_at(const FrameSnapshot& frame,const FrameAssets& assets,
                             Surface& target,std::size_t playback_frame,StrokeLimits limits) {
    Surface stage=target;
    auto result=render_frame_impl(frame,assets,stage,limits,0,playback_frame);
    if(result)target=std::move(stage);
    return result;
}
RenderResult render_frame(const FrameSnapshot& frame,const FrameAssets& assets,
                          Surface& target,StrokeLimits limits) {
    return render_frame_at(frame,assets,target,0,limits);
}
void TimelinePlayback::advance(std::size_t count) noexcept {
    if(!playing)return;
    const auto maximum=std::numeric_limits<std::size_t>::max();
    frame=count>maximum-frame?maximum:frame+count;
}
RenderResult render_playback(const FrameSnapshot& snapshot,
                             const FrameAssets& assets,Surface& target,
                             const TimelinePlayback& playback,StrokeLimits limits) {
    return render_frame_at(snapshot,assets,target,playback.frame,limits);
}
RenderResult render_timeline_playback(const TimelineResult& timeline,
                                      const FrameAssets& assets,Surface& target,
                                      const TimelinePlayback& playback,StrokeLimits limits) {
    if(!timeline)return {timeline.error,timeline.message};
    if(timeline.frames.empty())
        return {Error::malformed_tag,"root timeline has no frames"};
    // Use the frame within this root cycle for both the display list and
    // child sprite ages; placement offsets belong to this same cycle.
    const auto local_frame=playback.frame%timeline.frames.size();
    return render_frame_at(timeline.frames[local_frame],assets,target,local_frame,limits);
}
void SpritePlayback::synchronize(const FrameSnapshot& frame,const FrameAssets& assets) {
    std::map<std::uint16_t,std::pair<std::uint16_t,std::size_t>> visible;
    for(const auto& entry:frame.objects) {
        if(!assets.sprites.contains(entry.character_id))continue;
        const auto identity=std::make_pair(entry.character_id,entry.placed_frame);
        visible.emplace(entry.depth,identity);
        const auto previous=identities.find(entry.depth);
        if(previous!=identities.end() && previous->second!=identity) {
            instances.erase(entry.depth);
            for(auto it=nested_instances.begin();it!=nested_instances.end();) {
                if(!it->first.empty() && it->first.front()==entry.depth)
                    it=nested_instances.erase(it);
                else ++it;
            }
        }
    }
    for(auto it=instances.begin();it!=instances.end();) {
        if(!visible.contains(it->first))it=instances.erase(it);
        else ++it;
    }
    for(const auto& [depth,identity]:identities) {
        (void)identity;
        if(visible.contains(depth))continue;
        for(auto it=nested_instances.begin();it!=nested_instances.end();) {
            if(!it->first.empty() && it->first.front()==depth)
                it=nested_instances.erase(it);
            else ++it;
        }
    }
    identities=std::move(visible);
}
void SpritePlayback::synchronize_nested(const FrameSnapshot& frame,
                                        const FrameAssets& assets,
                                        std::size_t playback_frame) {
    using Path=std::vector<std::uint16_t>;
    using Identity=std::pair<std::uint16_t,std::size_t>;
    std::map<Path,Identity> visible;
    auto walk=[&](auto&& self,const FrameSnapshot& snapshot,
                  const Path& parent,std::size_t frame_index,unsigned depth)->void {
        if(depth>=32)return;
        for(const auto& entry:snapshot.objects) {
            const auto sprite=assets.sprites.find(entry.character_id);
            if(sprite==assets.sprites.end() || sprite->second.frames.empty())continue;
            Path path=parent;
            path.push_back(entry.depth);
            const Identity identity={entry.character_id,entry.placed_frame};
            if(depth>0)visible.emplace(path,identity);
            std::size_t local_frame=frame_index>=entry.placed_frame
                ?frame_index-entry.placed_frame:0;
            if(depth==0) {
                const auto it=instances.find(entry.depth);
                if(it!=instances.end())local_frame=it->second.frame;
            } else {
                const auto it=nested_instances.find(path);
                // Do not reuse a child's playback clock when any ancestor
                // at the same depth path was replaced since the last frame.
                bool ancestors_unchanged=true;
                for(std::size_t n=2;n<=path.size();++n) {
                    Path ancestor(path.begin(),path.begin()+n);
                    const auto old_id=nested_identities.find(ancestor);
                    const auto new_id=visible.find(ancestor);
                    if(old_id!=nested_identities.end() &&
                       (new_id==visible.end() || old_id->second!=new_id->second)) {
                        ancestors_unchanged=false;
                        break;
                    }
                }
                if(it!=nested_instances.end() && ancestors_unchanged &&
                   (!nested_identities.contains(path) || nested_identities.at(path)==identity))
                    local_frame=it->second.frame;
            }
            const auto& frames=sprite->second.frames;
            self(self,frames[local_frame%frames.size()],path,local_frame,depth+1);
        }
    };
    walk(walk,frame,{},playback_frame,0);
    // A child path can look unchanged after its parent is replaced. Compare
    // every ancestor identity too, not just the leaf's own character/frame.
    auto changed_or_missing=[&](const Path& path)->bool {
        for(std::size_t length=1;length<=path.size();++length) {
            Path ancestor(path.begin(),path.begin()+length);
            if(length==1) {
                const auto depth=ancestor.front();
                // Root replacement is handled by synchronize() before this
                // walk, while the previous root identity is still available.
                if(!identities.contains(depth))return true;
            } else {
                const auto old_id=nested_identities.find(ancestor);
                const auto new_id=visible.find(ancestor);
                if(new_id==visible.end() ||
                   (old_id!=nested_identities.end() && old_id->second!=new_id->second))
                    return true;
            }
        }
        return false;
    };
    for(auto it=nested_instances.begin();it!=nested_instances.end();) {
        if(changed_or_missing(it->first))it=nested_instances.erase(it);
        else ++it;
    }
    nested_identities=std::move(visible);
}
void SpritePlayback::advance(std::size_t count) noexcept {
    const auto maximum=std::numeric_limits<std::size_t>::max();
    for(auto& [depth,instance]:instances) {
        (void)depth;
        if(instance.playing)
            instance.frame=count>maximum-instance.frame?maximum:instance.frame+count;
    }
    for(auto& [path,instance]:nested_instances) {
        (void)path;
        if(instance.playing)
            instance.frame=count>maximum-instance.frame?maximum:instance.frame+count;
    }
}
RenderResult render_timeline_playback(const TimelineResult& timeline,
                                      const FrameAssets& assets,Surface& target,
                                      const TimelinePlayback& playback,
                                      const SpritePlayback& sprites,
                                      StrokeLimits limits) {
    if(!timeline)return {timeline.error,timeline.message};
    if(timeline.frames.empty())
        return {Error::malformed_tag,"root timeline has no frames"};
    const auto root_frame=playback.frame%timeline.frames.size();
    Surface stage=target;
    auto result=render_frame_impl(timeline.frames[root_frame],assets,stage,
                                  limits,0,root_frame,&sprites);
    if(result)target=std::move(stage);
    return result;
}
RenderResult TimelinePlayer::render(const TimelineResult& timeline,
                                    const FrameAssets& assets,Surface& target,
                                    StrokeLimits limits) {
    if(!timeline)return {timeline.error,timeline.message};
    if(timeline.frames.empty())
        return {Error::malformed_tag,"root timeline has no frames"};
    const auto cycle=clock.frame/timeline.frames.size();
    const auto root_frame=clock.frame%timeline.frames.size();
    SpritePlayback updated=sprites;
    if(initialized && cycle!=last_cycle) {
        updated.instances.clear();
        updated.identities.clear();
        updated.nested_instances.clear();
        updated.nested_identities.clear();
    }
    updated.synchronize(timeline.frames[root_frame],assets);
    updated.synchronize_nested(timeline.frames[root_frame],assets,root_frame);
    Surface stage=target;
    auto result=render_frame_impl(timeline.frames[root_frame],assets,stage,
                                  limits,0,root_frame,&updated);
    if(!result)return result;
    target=std::move(stage);
    sprites=std::move(updated);
    last_cycle=cycle;
    initialized=true;
    return {};
}
RenderResult collect_frame_assets(const Document& document,FrameAssets& output,
                                  std::size_t max_pixels) {
    FrameAssets stage;
    auto unique_id=[&](std::uint16_t id) {
        return !stage.shapes.contains(id) && !stage.bitmaps.images.contains(id) &&
               !stage.sprites.contains(id);
    };
    for(const auto& tag:document.tags) {
        if(tag.code==39) {
            auto sprite=build_sprite_timeline(tag);
            if(!sprite)return {sprite.error,sprite.message};
            if(!unique_id(sprite.sprite.character_id))
                return {Error::malformed_tag,"duplicate character id"};
            stage.sprites.emplace(sprite.sprite.character_id,std::move(sprite.sprite));
        } else if(tag.code==2||tag.code==22||tag.code==32||tag.code==83) {
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
