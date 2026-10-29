// SPDX-License-Identifier: MIT
#include "rinswf/text.hpp"
#include <limits>
namespace rinswf {
namespace {
struct Reader {
    const std::vector<std::uint8_t>& data;
    std::size_t bit=0;
    bool take(unsigned n,std::uint32_t& out) {
        if(n>32||bit>data.size()*8||n>data.size()*8-bit)return false;
        out=0;
        for(unsigned i=0;i<n;++i,++bit)
            out=(out<<1)|((data[bit/8]>>(7-bit%8))&1u);
        return true;
    }
    bool signed_value(unsigned n,std::int32_t& out) {
        std::uint32_t value=0;
        if(!take(n,value))return false;
        if(!n){out=0;return true;}
        if(n==32){out=static_cast<std::int32_t>(value);return true;}
        const auto sign=1u<<(n-1);
        out=static_cast<std::int32_t>((value^sign)-sign);
        return true;
    }
    void align(){bit=(bit+7)&~std::size_t(7);}
    bool rect() {
        std::uint32_t n=0,value=0;
        if(!take(5,n))return false;
        for(int i=0;i<4;++i)if(!take(n,value))return false;
        align();return bit<=data.size()*8;
    }
    bool matrix(ShapeMatrix& m) {
        std::uint32_t has=0,n=0;
        std::int32_t a=0,b=0;
        if(!take(1,has))return false;
        if(has) {
            if(!take(5,n)||!signed_value(n,a)||!signed_value(n,b))return false;
            m.scale_x=double(a)/65536.0;m.scale_y=double(b)/65536.0;
        }
        if(!take(1,has))return false;
        if(has) {
            if(!take(5,n)||!signed_value(n,a)||!signed_value(n,b))return false;
            m.skew_x=double(a)/65536.0;m.skew_y=double(b)/65536.0;
        }
        if(!take(5,n)||!signed_value(n,m.translate_x)||!signed_value(n,m.translate_y))
            return false;
        align();return bit<=data.size()*8;
    }
};
TextResult fail(Error error,const char* message){return {error,message,{}};}
bool advance(std::int32_t& x,std::int32_t amount) {
    const auto sum=std::int64_t(x)+amount;
    if(sum<std::numeric_limits<std::int32_t>::min()||
       sum>std::numeric_limits<std::int32_t>::max())return false;
    x=static_cast<std::int32_t>(sum);return true;
}
}
TextResult decode_define_text(const Tag& tag,std::size_t max_glyphs) {
    if(tag.code!=11 && tag.code!=33)
        return fail(Error::malformed_tag,"not DefineText/DefineText2");
    Reader r{tag.data};
    std::uint32_t value=0;
    TextAsset result;
    if(!r.take(16,value)||!r.rect()||!r.matrix(result.matrix))
        return fail(Error::truncated,"text header truncated");
    result.character_id=static_cast<std::uint16_t>(value); // replaced below
    // Character ID is stored before RECT; preserve it independently.
    result.character_id=std::uint16_t(tag.data[0]|(std::uint16_t(tag.data[1])<<8));
    std::uint32_t glyph_bits=0,advance_bits=0;
    if(!r.take(8,glyph_bits)||!r.take(8,advance_bits))
        return fail(Error::truncated,"text glyph widths truncated");
    if(glyph_bits>32||advance_bits>32)
        return fail(Error::malformed_tag,"invalid glyph bit widths");
    std::uint16_t font=0,height=0;
    std::int32_t x=0,y=0;
    ShapeColor color{};
    while(r.bit<tag.data.size()*8) {
        r.align();
        std::uint32_t flags=0;
        if(!r.take(8,flags))return fail(Error::truncated,"text record truncated");
        if(!flags) {
            if(r.bit!=tag.data.size()*8)
                return fail(Error::malformed_tag,"trailing text bytes");
            return {Error::none,{},std::move(result)};
        }
        if(!(flags&0x80)||(flags&0x70))
            return fail(Error::malformed_tag,"invalid text record flags");
        if(flags&0x08) {
            if(!r.take(16,value))return fail(Error::truncated,"font ID truncated");
            font=std::uint16_t(value);
        }
        if(flags&0x04) {
            for(unsigned c=0;c<(tag.code==33?4u:3u);++c) {
                if(!r.take(8,value))return fail(Error::truncated,"text color truncated");
                if(c==0)color.r=value;
                else if(c==1)color.g=value;
                else if(c==2)color.b=value;
                else color.a=value;
            }
            if(tag.code==11)color.a=255;
        }
        if(flags&0x01) {
            if(!r.signed_value(16,x))return fail(Error::truncated,"text X offset truncated");
        }
        if(flags&0x02) {
            if(!r.signed_value(16,y))return fail(Error::truncated,"text Y offset truncated");
        }
        if(flags&0x08) {
            if(!r.take(16,value))return fail(Error::truncated,"text height truncated");
            height=std::uint16_t(value);
        }
        std::uint32_t count=0;
        if(!r.take(8,count))return fail(Error::truncated,"glyph count truncated");
        if(result.glyphs.size()+count>max_glyphs)
            return fail(Error::size_limit,"text glyph limit");
        if(count && (!font||!height))
            return fail(Error::malformed_tag,"glyph run missing font/height");
        for(unsigned i=0;i<count;++i) {
            std::uint32_t index=0;
            std::int32_t step=0;
            if(!r.take(glyph_bits,index)||!r.signed_value(advance_bits,step))
                return fail(Error::truncated,"glyph entry truncated");
            result.glyphs.push_back({font,index,x,y,height,color});
            if(!advance(x,step))return fail(Error::invalid_length,"glyph advance overflow");
        }
        r.align();
    }
    return fail(Error::truncated,"text terminator missing");
}
}
