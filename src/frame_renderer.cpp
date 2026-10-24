// SPDX-License-Identifier: MIT
#include "rinswf/frame_renderer.hpp"
#include "rinswf/jpeg.hpp"
#include <cmath>
#include <algorithm>
#include <limits>
#include <array>
#include <unordered_map>
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
static RenderResult affine_straight_stroke(Surface& dst,const ShapeGeometry& shape,
                                           const Matrix& m) {
    // Transform each straight edge's rectangular outline into one fill geometry.
    // The common nonzero-winding pass keeps opaque segment overlaps solid.
    if(shape.lines.size()!=1 || !shape.fills.empty() ||
       shape.commands.size()<2)
        return {Error::malformed_tag,"unsupported affine stroke geometry"};
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
    const double extension_start=first_edge && shape.lines[0].start_cap==2?radius:0;
    const double extension_end=last_edge && shape.lines[0].end_cap==2?radius:0;
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
            if(start.x==finish.x && start.y==finish.y) continue;
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
        for(std::size_t i=2;i<shape.commands.size();++i) {
            const auto& before=shape.commands[i-1];
            const auto& next=shape.commands[i];
            if(before.kind!=ShapeCommandKind::line_to ||
               next.kind!=ShapeCommandKind::line_to) continue;
            // The previous segment must belong to the same uninterrupted subpath.
            const auto& earlier=shape.commands[i-2];
            if(earlier.kind!=ShapeCommandKind::move_to &&
               earlier.kind!=ShapeCommandKind::line_to) continue;
            const auto& a=earlier.endpoint;
            const auto& b=before.endpoint;
            const auto& c=next.endpoint;
            const double ux=double(b.x)-a.x,uy=double(b.y)-a.y;
            const double vx=double(c.x)-b.x,vy=double(c.y)-b.y;
            const double ul=std::hypot(ux,uy),vl=std::hypot(vx,vy);
            if(ul<1e-8||vl<1e-8) continue;
            const double cross=ux*vy-uy*vx;
            if(std::abs(cross)<1e-8) continue;
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
                continue;
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
                        continue;
                    }
                }
            }
            if(!add_triangle(b.x,b.y,p1x,p1y,p2x,p2y))
                return {Error::size_limit,"bevel join coordinate overflow"};
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
    using CoverageTile=std::array<std::uint8_t,tile_side*tile_side>;
    std::unordered_map<std::uint64_t,CoverageTile> coverage;
    constexpr std::size_t max_tiles=4096;
    auto tile_key=[](std::uint32_t x,std::uint32_t y)->std::uint64_t {
        return (std::uint64_t(y)<<32)|x;
    };
    ShapeGeometry one;
    one.fills=quad.fills;
    std::size_t contours=0;
    auto flush=[&]()->RenderResult {
        if(one.commands.empty())return {};
        if(++contours>8192)return {Error::size_limit,"stroke contour limit exceeded"};
        double minx=double(dst.width),miny=double(dst.height),maxx=0,maxy=0;
        for(const auto& cmd:one.commands) {
            const double x=double(cmd.endpoint.x)/20.0;
            const double y=double(cmd.endpoint.y)/20.0;
            minx=std::min(minx,x);maxx=std::max(maxx,x);
            miny=std::min(miny,y);maxy=std::max(maxy,y);
        }
        const auto left=std::int64_t(std::clamp(std::floor(minx)-1.0,0.0,double(dst.width)));
        const auto top=std::int64_t(std::clamp(std::floor(miny)-1.0,0.0,double(dst.height)));
        const auto right=std::int64_t(std::clamp(std::ceil(maxx)+1.0,0.0,double(dst.width)));
        const auto bottom=std::int64_t(std::clamp(std::ceil(maxy)+1.0,0.0,double(dst.height)));
        if(right>left && bottom>top) {
            Surface individual;
            auto result=make_surface(std::uint32_t(right-left),std::uint32_t(bottom-top),individual);
            if(!result)return result;
            const auto offsetx=-left*20,offsety=-top*20;
            if(offsetx<std::numeric_limits<std::int32_t>::min() ||
               offsety<std::numeric_limits<std::int32_t>::min())
                return {Error::size_limit,"contour offset overflow"};
            result=draw_shape_fills(individual,one,std::int32_t(offsetx),std::int32_t(offsety));
            if(!result)return result;
            for(std::int64_t y=0;y<bottom-top;++y)
                for(std::int64_t x=0;x<right-left;++x) {
                    const auto src=(std::size_t(y)*individual.width+std::size_t(x))*4+3;
                    const auto alpha=individual.rgba[src];
                    if(!alpha)continue;
                    const auto px=std::uint32_t(x+left-mask_left);
                    const auto py=std::uint32_t(y+top-mask_top);
                    const auto key=tile_key(px/tile_side,py/tile_side);
                    auto tile=coverage.find(key);
                    if(tile==coverage.end()) {
                        if(coverage.size()>=max_tiles)
                            return {Error::size_limit,"stroke union tile limit"};
                        tile=coverage.emplace(key,CoverageTile{}).first;
                    }
                    const auto index=std::size_t(py%tile_side)*tile_side+(px%tile_side);
                    tile->second[index]=std::max(tile->second[index],alpha);
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
            const unsigned sa=(unsigned(tile[y*tile_side+x])*opacity+127)/255;
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
                bool straight_butt=sh.lines.size()==1 && sh.fills.empty() &&
                    sh.lines[0].color.a==255;
                bool has_move=false,has_line=false;
                for(const auto& edge:sh.commands) {
                    if(edge.kind==ShapeCommandKind::move_to) has_move=true;
                    else if(edge.kind==ShapeCommandKind::line_to && edge.line==1 && has_move)
                        has_line=true;
                    else straight_butt=false;
                }
                straight_butt=straight_butt && has_line;
                auto a=direct
                    ? vector_shape(stage,sh,assets.bitmaps,m,limits)
                    : straight_butt ? affine_straight_stroke(stage,sh,m)
                                    : affine_shape(stage,sh,assets.bitmaps,m,limits);
                if(!a)return a;
            } else {
                auto a=draw_shape_fills(stage,it->second,tx,ty,&assets.bitmaps);
                if(!a)return a;
                auto b=draw_shape_strokes(stage,it->second,tx,ty,limits);
                if(!b)return b;
            }
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
