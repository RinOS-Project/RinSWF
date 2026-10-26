// SPDX-License-Identifier: MIT
#include "rinswf/shapes.hpp"
#include <limits>
namespace rinswf {
namespace {
struct Bits {
    const std::vector<std::uint8_t>& d;
    std::size_t pos=0;
    bool read(unsigned n,std::uint32_t& v) {
        if(n>32 || pos>d.size()*8 || n>d.size()*8-pos) return false;
        v=0;
        for(unsigned i=0;i<n;++i) { v=(v<<1)|((d[pos/8]>>(7-(pos%8)))&1u);++pos; }
        return true;
    }
    bool signed_read(unsigned n,std::int32_t& v) {
        std::uint32_t raw=0;
        if(!read(n,raw)) return false;
        if(!n) {v=0;return true;}
        if(n==32) {v=static_cast<std::int32_t>(raw);return true;}
        const auto sign=1u<<(n-1);
        v=static_cast<std::int32_t>((raw^sign)-sign);
        return true;
    }
    bool skip(std::size_t n) {
        if(pos>d.size()*8 || n>d.size()*8-pos) return false;
        pos+=n;return true;
    }
    bool rect() {
        std::uint32_t n=0;
        if(!read(5,n)||!skip(std::size_t(4)*n)) return false;
        align(); return pos<=d.size()*8;
    }
    void align() { pos=(pos+7)&~std::size_t(7); }
    bool byte(std::uint32_t& v) { return read(8,v); }
};
bool matrix(Bits& b, ShapeMatrix& m) {
    std::uint32_t flag=0,n=0;
    std::int32_t a=0,c=0;
    if(!b.read(1,flag)) return false;
    if(flag) {
        if(!b.read(5,n)||!b.signed_read(n,a)||!b.signed_read(n,c)) return false;
        m.scale_x=double(a)/65536.0; m.scale_y=double(c)/65536.0;
    }
    if(!b.read(1,flag)) return false;
    if(flag) {
        if(!b.read(5,n)||!b.signed_read(n,a)||!b.signed_read(n,c)) return false;
        m.skew_x=double(a)/65536.0; m.skew_y=double(c)/65536.0;
    }
    if(!b.read(5,n)||!b.signed_read(n,m.translate_x)||!b.signed_read(n,m.translate_y)) return false;
    b.align();
    return b.pos<=b.d.size()*8;
}
bool color(Bits& b,bool alpha,ShapeColor& c) {
    std::uint32_t v=0;
    if(!b.byte(v)) return false; c.r=static_cast<std::uint8_t>(v);
    if(!b.byte(v)) return false; c.g=static_cast<std::uint8_t>(v);
    if(!b.byte(v)) return false; c.b=static_cast<std::uint8_t>(v);
    if(alpha) { if(!b.byte(v)) return false; c.a=static_cast<std::uint8_t>(v); }
    return true;
}
bool style_count(Bits& b,bool extended,std::uint32_t& count) {
    if(!b.byte(count)) return false;
    if(count==255 && extended) {
        std::uint32_t lo=0,hi=0;
        if(!b.byte(lo)||!b.byte(hi)) return false;
        count=lo|(hi<<8);
    }
    return true;
}
ShapeResult bad(Error e,const char* why) {return {e,why,{}};}
bool advance(std::int32_t& current,std::int32_t delta) {
    auto result=std::int64_t(current)+delta;
    if(result<std::numeric_limits<std::int32_t>::min() || result>std::numeric_limits<std::int32_t>::max()) return false;
    current=static_cast<std::int32_t>(result);return true;
}
}
ShapeResult decode_shape(const Tag& tag,std::size_t max_commands) {
    if(tag.code!=2 && tag.code!=22 && tag.code!=32 && tag.code!=83)
        return bad(Error::malformed_tag,"not a DefineShape tag");
    Bits b{tag.data};
    std::uint32_t id=0,flags=0;
    if(!b.read(16,id)||!b.rect()) return bad(Error::truncated,"truncated shape header");
    if(tag.code==83) {
        if(!b.rect() || !b.read(8,flags)) return bad(Error::truncated,"truncated DefineShape4 header");
    }
    ShapeGeometry geometry;
    geometry.character_id=static_cast<std::uint16_t>(id);
    const bool extended=tag.code!=2, alpha=tag.code>=32;
    std::uint32_t fills=0,lines=0;
    // Style arrays added by StateNewStyles use their own local indices.
    // Flatten them into the geometry's global arrays while retaining a
    // separate base for the current style array.
    std::uint32_t fill_base=0,line_base=0;
    const auto read_styles=[&]() -> ShapeResult {
        std::uint32_t new_fills=0,new_lines=0;
        if(!style_count(b,extended,new_fills))
            return bad(Error::truncated,"truncated fill count");
        if(new_fills>65535-geometry.fills.size())
            return bad(Error::size_limit,"too many fills");
    for(std::uint32_t i=0;i<new_fills;++i) {
        std::uint32_t type=0;
        if(!b.byte(type)) return bad(Error::truncated,"truncated fill type");
        ShapeFill fill;
        if(type==0) {
            if(!color(b,alpha,fill.color)) return bad(Error::truncated,"truncated solid fill");
        } else if(type==0x10 || type==0x12 || type==0x13) {
            fill.kind=type==0x10?FillKind::linear_gradient:
                      type==0x12?FillKind::radial_gradient:FillKind::focal_gradient;
            if(!matrix(b,fill.matrix)) return bad(Error::truncated,"truncated gradient matrix");
            std::uint32_t spread=0,interp=0,count=0;
            if(!b.read(2,spread)||!b.read(2,interp)||!b.read(4,count))
                return bad(Error::truncated,"truncated gradient header");
            if(count==0 || count>15) return bad(Error::malformed_tag,"invalid gradient count");
            if(tag.code==2 && type==0x13)
                return bad(Error::malformed_tag,"focal gradient not valid for DefineShape1");
            fill.spread=static_cast<std::uint8_t>(spread);
            fill.interpolation=static_cast<std::uint8_t>(interp);
            for(std::uint32_t k=0;k<count;++k) {
                std::uint32_t ratio=0;
                GradientStop stop;
                if(!b.byte(ratio)||!color(b,alpha,stop.color))
                    return bad(Error::truncated,"truncated gradient stop");
                stop.ratio=static_cast<std::uint8_t>(ratio);
                fill.stops.push_back(stop);
            }
            if(type==0x13) {
                std::uint32_t lo=0,hi=0;
                if(!b.byte(lo)||!b.byte(hi)) return bad(Error::truncated,"truncated focal point");
                const auto signed_value=static_cast<std::int16_t>(lo|(hi<<8));
                fill.focal_point=double(signed_value)/256.0;
            }
        } else if(type>=0x40 && type<=0x43) {
            std::uint32_t id_lo=0,id_hi=0;
            if(!b.byte(id_lo)||!b.byte(id_hi))
                return bad(Error::truncated,"truncated bitmap character ID");
            fill.bitmap_id=static_cast<std::uint16_t>(id_lo|(id_hi<<8));
            fill.kind=type==0x40?FillKind::repeating_bitmap:
                      type==0x41?FillKind::clipped_bitmap:
                      type==0x42?FillKind::nonsmoothed_repeating_bitmap:
                                 FillKind::nonsmoothed_clipped_bitmap;
            if(!matrix(b,fill.matrix)) return bad(Error::truncated,"truncated bitmap fill matrix");
        } else {
            return bad(Error::malformed_tag,"unsupported fill style type");
        }
        geometry.fills.push_back(std::move(fill));
    }
        if(!style_count(b,extended,new_lines))
            return bad(Error::truncated,"truncated line count");
        if(new_lines>65535-geometry.lines.size())
            return bad(Error::size_limit,"too many lines");
    for(std::uint32_t i=0;i<new_lines;++i) {
        std::uint32_t low=0,high=0;
        if(!b.byte(low)||!b.byte(high)) return bad(Error::truncated,"truncated line width");
        ShapeLine line;
        line.width=static_cast<std::uint16_t>(low|(high<<8));
        if(tag.code==83) {
            std::uint32_t start=0,join=0,has_fill=0,nh=0,nv=0,hint=0,reserved=0,noclose=0,end=0;
            if(!b.read(2,start)||!b.read(2,join)||!b.read(1,has_fill)||
               !b.read(1,nh)||!b.read(1,nv)||!b.read(1,hint)||
               !b.read(5,reserved)||!b.read(1,noclose)||!b.read(2,end))
                return bad(Error::truncated,"LINESTYLE2 flags truncated");
            if(reserved || start==3 || end==3 || join==3)
                return bad(Error::malformed_tag,"LINESTYLE2 invalid flags");
            line.start_cap=static_cast<std::uint8_t>(start);
            line.end_cap=static_cast<std::uint8_t>(end);
            line.join=static_cast<std::uint8_t>(join);
            line.no_hscale=nh!=0;line.no_vscale=nv!=0;
            line.pixel_hinting=hint!=0;line.no_close=noclose!=0;
            if(join==2) {
                std::uint32_t lo=0,hi=0;
                if(!b.byte(lo)||!b.byte(hi))
                    return bad(Error::truncated,"LINESTYLE2 miter limit truncated");
                line.miter_limit=double(lo|(hi<<8))/256.0;
            }
            if(has_fill) {
                // FillType 0 is a solid RGBA fill. Its pixels are identical
                // to a conventional LINESTYLE2 RGBA stroke, so the current
                // stroke rasterizer can render it without a separate path.
                // Other fill types require per-pixel stroke sampling.
                std::uint32_t fill_type=0;
                if(!b.byte(fill_type))
                    return bad(Error::truncated,"LINESTYLE2 fill type truncated");
                if(fill_type!=0)
                    return bad(Error::malformed_tag,"unsupported LINESTYLE2 stroke fill type");
                if(!color(b,true,line.color))
                    return bad(Error::truncated,"LINESTYLE2 solid stroke fill truncated");
            } else if(!color(b,true,line.color))
                return bad(Error::truncated,"truncated LINESTYLE2 line color");
        } else if(!color(b,alpha,line.color))
            return bad(Error::truncated,"truncated line color");
        geometry.lines.push_back(line);
    }
        fills=new_fills;lines=new_lines;
        return {};
    };
    {
        auto result=read_styles();
        if(!result)return result;
    }
    std::uint32_t fill_bits=0,line_bits=0;
    if(!b.read(4,fill_bits)||!b.read(4,line_bits)) return bad(Error::truncated,"missing style indices");
    std::uint32_t fill0=0,fill1=0,line=0;
    if((fills && !fill_bits)||(lines && !line_bits))
        return bad(Error::malformed_tag,"missing style index bits");
    std::int32_t x=0,y=0;
    for(;;) {
        std::uint32_t type=0,flags5=0;
        if(!b.read(1,type)||!b.read(5,flags5)) return bad(Error::truncated,"truncated shape record");
        if(!type) {
            if(!flags5) break;

            if(flags5 & 0x01) {
                std::uint32_t n=0;
                if(!b.read(5,n)||!b.signed_read(n,x)||!b.signed_read(n,y))
                    return bad(Error::truncated,"truncated move record");
                if(geometry.commands.size()>=max_commands) return bad(Error::size_limit,"shape command limit");
                geometry.commands.push_back({ShapeCommandKind::move_to,{}, {x,y},fill0,fill1,line});
            }
            std::uint32_t idx=0;
            if(flags5 & 0x02) {
                if(!b.read(fill_bits,idx)||idx>fills) return bad(Error::malformed_tag,"invalid fill0 index");
                fill0=idx?fill_base+idx:0;
            }
            if(flags5 & 0x04) {
                if(!b.read(fill_bits,idx)||idx>fills) return bad(Error::malformed_tag,"invalid fill1 index");
                fill1=idx?fill_base+idx:0;
            }
            if(flags5 & 0x08) {
                if(!b.read(line_bits,idx)||idx>lines) return bad(Error::malformed_tag,"invalid line index");
                line=idx?line_base+idx:0;
            }
            if(flags5 & 0x10) {
                if(tag.code==2)
                    return bad(Error::malformed_tag,"NewStyles not valid for DefineShape1");
                b.align();
                fill_base=static_cast<std::uint32_t>(geometry.fills.size());
                line_base=static_cast<std::uint32_t>(geometry.lines.size());
                auto result=read_styles();
                if(!result)return result;
                if(!b.read(4,fill_bits)||!b.read(4,line_bits))
                    return bad(Error::truncated,"truncated NewStyles index bits");
                if((fills&&!fill_bits)||(lines&&!line_bits))
                    return bad(Error::malformed_tag,"missing NewStyles index bits");
            }
        } else {
            const bool straight=(flags5 & 0x10)!=0;
            const unsigned n=(flags5 & 0x0f)+2;
            if(geometry.commands.size()>=max_commands) return bad(Error::size_limit,"shape command limit");
            if(straight) {
                std::uint32_t general=0,vertical=0;
                std::int32_t dx=0,dy=0;
                if(!b.read(1,general)) return bad(Error::truncated,"truncated straight edge");
                if(general) {
                    if(!b.signed_read(n,dx)||!b.signed_read(n,dy)) return bad(Error::truncated,"truncated line");
                } else {
                    if(!b.read(1,vertical)) return bad(Error::truncated,"truncated axis edge");
                    if(vertical) {if(!b.signed_read(n,dy)) return bad(Error::truncated,"truncated vertical line");}
                    else {if(!b.signed_read(n,dx)) return bad(Error::truncated,"truncated horizontal line");}
                }
                if(!advance(x,dx)||!advance(y,dy)) return bad(Error::invalid_length,"coordinate overflow");
                geometry.commands.push_back({ShapeCommandKind::line_to,{}, {x,y},fill0,fill1,line});
            } else {
                std::int32_t cdx=0,cdy=0,adx=0,ady=0;
                if(!b.signed_read(n,cdx)||!b.signed_read(n,cdy)||!b.signed_read(n,adx)||!b.signed_read(n,ady))
                    return bad(Error::truncated,"truncated curved edge");
                if(!advance(x,cdx)||!advance(y,cdy)) return bad(Error::invalid_length,"curve control overflow");
                ShapePoint control{x,y};
                if(!advance(x,adx)||!advance(y,ady)) return bad(Error::invalid_length,"curve endpoint overflow");
                geometry.commands.push_back({ShapeCommandKind::quadratic_to,control,{x,y},fill0,fill1,line});
            }
        }
    }
    b.align();
    if(b.pos!=tag.data.size()*8) return bad(Error::malformed_tag,"trailing shape bytes");
    return {Error::none,{},std::move(geometry)};
}
}
