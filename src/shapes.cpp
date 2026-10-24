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
    std::uint32_t fills=0,lines=0;
    if(!b.byte(fills)||!b.byte(lines)) return bad(Error::truncated,"truncated style tables");
    if(fills || lines) return bad(Error::malformed_tag,"non-empty shape style tables not supported");
    std::uint32_t fill_bits=0,line_bits=0;
    if(!b.read(4,fill_bits)||!b.read(4,line_bits)) return bad(Error::truncated,"missing style indices");
    if(fill_bits || line_bits) return bad(Error::malformed_tag,"unexpected style index width");
    ShapeGeometry geometry;
    geometry.character_id=static_cast<std::uint16_t>(id);
    std::int32_t x=0,y=0;
    for(;;) {
        std::uint32_t type=0,flags5=0;
        if(!b.read(1,type)||!b.read(5,flags5)) return bad(Error::truncated,"truncated shape record");
        if(!type) {
            if(!flags5) break;
            if(flags5 & 0x10) return bad(Error::malformed_tag,"new style arrays not supported");
            if(flags5 & 0x0b) return bad(Error::malformed_tag,"style changes not supported");
            if(flags5 & 0x04) {
                std::uint32_t n=0;
                if(!b.read(5,n)||!b.signed_read(n,x)||!b.signed_read(n,y))
                    return bad(Error::truncated,"truncated move record");
                if(geometry.commands.size()>=max_commands) return bad(Error::size_limit,"shape command limit");
                geometry.commands.push_back({ShapeCommandKind::move_to,{}, {x,y},0,0,0});
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
                geometry.commands.push_back({ShapeCommandKind::line_to,{}, {x,y},0,0,0});
            } else {
                std::int32_t cdx=0,cdy=0,adx=0,ady=0;
                if(!b.signed_read(n,cdx)||!b.signed_read(n,cdy)||!b.signed_read(n,adx)||!b.signed_read(n,ady))
                    return bad(Error::truncated,"truncated curved edge");
                if(!advance(x,cdx)||!advance(y,cdy)) return bad(Error::invalid_length,"curve control overflow");
                ShapePoint control{x,y};
                if(!advance(x,adx)||!advance(y,ady)) return bad(Error::invalid_length,"curve endpoint overflow");
                geometry.commands.push_back({ShapeCommandKind::quadratic_to,control,{x,y},0,0,0});
            }
        }
    }
    b.align();
    if(b.pos!=tag.data.size()*8) return bad(Error::malformed_tag,"trailing shape bytes");
    return {Error::none,{},std::move(geometry)};
}
}
