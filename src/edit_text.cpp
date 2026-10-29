// SPDX-License-Identifier: MIT
#include "rinswf/edit_text.hpp"
namespace rinswf {
namespace {
struct Reader {
    const std::vector<std::uint8_t>& d;
    std::size_t bit=0;
    bool read(unsigned n,std::uint32_t& v) {
        if(n>32 || bit>d.size()*8 || n>d.size()*8-bit)return false;
        v=0;
        for(unsigned i=0;i<n;++i,++bit)v=(v<<1)|((d[bit/8]>>(7-bit%8))&1u);
        return true;
    }
    bool signed_read(unsigned n,std::int32_t& v) {
        std::uint32_t raw=0;
        if(!read(n,raw))return false;
        if(!n){v=0;return true;}
        if(n==32){v=static_cast<std::int32_t>(raw);return true;}
        const auto sign=1u<<(n-1);
        v=static_cast<std::int32_t>((raw^sign)-sign);
        return true;
    }
    bool u8(std::uint8_t& v) {std::uint32_t x=0;if(!read(8,x))return false;v=std::uint8_t(x);return true;}
    bool u16(std::uint16_t& v) {std::uint8_t a=0,b=0;if(!u8(a)||!u8(b))return false;v=std::uint16_t(a|(unsigned(b)<<8));return true;}
    void align(){bit=(bit+7)&~std::size_t(7);}
    bool rect(EditTextAsset& out) {
        std::uint32_t n=0;
        if(!read(5,n)||!signed_read(n,out.xmin)||!signed_read(n,out.xmax)||
           !signed_read(n,out.ymin)||!signed_read(n,out.ymax))return false;
        align();return bit<=d.size()*8;
    }
    bool str(std::string& out,std::size_t max_length) {
        if(bit%8)return false;
        const std::size_t start=bit/8;
        std::size_t end=start;
        while(end<d.size() && d[end]!=0 && end-start<=max_length)++end;
        if(end==d.size() || end-start>max_length)return false;
        out.assign(reinterpret_cast<const char*>(d.data()+start),end-start);
        bit=(end+1)*8;return true;
    }
};
EditTextResult fail(Error e,const char* message){return {e,message,{}};}
}
EditTextResult decode_edit_text(const Tag& tag,std::size_t max_string_bytes) {
    if(tag.code!=37)return fail(Error::malformed_tag,"not DefineEditText");
    Reader r{tag.data};
    EditTextAsset text;
    if(!r.u16(text.character_id)||!r.rect(text)||!r.u16(text.flags))
        return fail(Error::truncated,"DefineEditText header truncated");
    if(text.xmin>text.xmax || text.ymin>text.ymax)
        return fail(Error::malformed_tag,"invalid edit text bounds");
    const bool has_text=(text.flags&0x8000)!=0;
    const bool word_wrap=(text.flags&0x4000)!=0;
    const bool multiline=(text.flags&0x2000)!=0;
    const bool has_color=(text.flags&0x0400)!=0;
    const bool has_max=(text.flags&0x0200)!=0;
    const bool has_font=(text.flags&0x0100)!=0;
    const bool has_font_class=(text.flags&0x0080)!=0;
    const bool has_layout=(text.flags&0x0020)!=0;
    (void)word_wrap;(void)multiline;
    if(has_font && !r.u16(text.font_id))
        return fail(Error::truncated,"edit text font ID truncated");
    if(has_font_class && !r.str(text.font_class,max_string_bytes))
        return fail(Error::truncated,"edit text font class truncated");
    if((has_font||has_font_class) && !r.u16(text.font_height))
        return fail(Error::truncated,"edit text font height truncated");
    if(has_color && (!r.u8(text.color.r)||!r.u8(text.color.g)||
                     !r.u8(text.color.b)||!r.u8(text.color.a)))
        return fail(Error::truncated,"edit text color truncated");
    if(has_max && !r.u16(text.max_length))
        return fail(Error::truncated,"edit text maximum length truncated");
    if(has_layout) {
        std::uint16_t lead=0;
        if(!r.u8(text.align)||!r.u16(text.left_margin)||!r.u16(text.right_margin)||
           !r.u16(text.indent)||!r.u16(lead))
            return fail(Error::truncated,"edit text layout truncated");
        text.leading=static_cast<std::int16_t>(lead);
        if(text.align>3)return fail(Error::malformed_tag,"invalid edit text alignment");
    }
    if(!r.str(text.variable_name,max_string_bytes))
        return fail(Error::truncated,"edit text variable name truncated");
    if(has_text && !r.str(text.initial_text,max_string_bytes))
        return fail(Error::truncated,"edit text initial text truncated");
    if(r.bit!=tag.data.size()*8)
        return fail(Error::malformed_tag,"trailing DefineEditText bytes");
    return {Error::none,{},std::move(text)};
}
}
