// SPDX-License-Identifier: MIT
#include "rinswf/display_list.hpp"
#include <utility>
namespace rinswf {
namespace {
std::uint16_t u16(const std::vector<std::uint8_t>& d,std::size_t p) {
    return std::uint16_t(d[p]) | (std::uint16_t(d[p+1])<<8);
}
TimelineResult fail(const char* why) { return {Error::malformed_tag,why,{}}; }
struct Reader {
    const std::vector<std::uint8_t>& d;
    std::size_t bit=0;
    bool take(unsigned count,std::uint32_t& out) {
        if (count>32 || bit>d.size()*8 || count>d.size()*8-bit) return false;
        out=0;
        for(unsigned j=0;j<count;++j) {
            out=(out<<1)|((d[bit/8]>>(7-(bit%8)))&1u); ++bit;
        }
        return true;
    }
    bool skip(std::size_t n) {
        if (bit>d.size()*8 || n>d.size()*8-bit) return false;
        bit+=n;return true;
    }
    void align() { bit=(bit+7)&~std::size_t(7); }
    std::size_t pos() const { return bit/8; }
    bool signed_value(unsigned n, std::int32_t& value) {
        std::uint32_t raw=0;
        if (!take(n,raw)) return false;
        if (n==0) { value=0; return true; }
        if (n==32) { value=static_cast<std::int32_t>(raw); return true; }
        const auto sign=std::uint32_t(1)<<(n-1);
        value=static_cast<std::int32_t>((raw ^ sign)-sign);
        return true;
    }
    bool matrix(Matrix& out) {
        std::uint32_t has=0,n=0;
        if(!take(1,has)) return false;
        std::int32_t a=0,b=0;
        if(has) {
            if(!take(5,n)||!signed_value(n,a)||!signed_value(n,b)) return false;
            out.scale_x=static_cast<double>(a)/65536.0;
            out.scale_y=static_cast<double>(b)/65536.0;
        }
        if(!take(1,has)) return false;
        if(has) {
            if(!take(5,n)||!signed_value(n,a)||!signed_value(n,b)) return false;
            out.rotate_skew_0=static_cast<double>(a)/65536.0;
            out.rotate_skew_1=static_cast<double>(b)/65536.0;
        }
        if(!take(5,n)||!signed_value(n,out.translate_x)||!signed_value(n,out.translate_y)) return false;
        align(); return bit<=d.size()*8;
    }
    bool cxform_alpha(ColorTransform& out) {
        std::uint32_t has_add=0,has_mult=0,n=0;
        if(!take(1,has_add)||!take(1,has_mult)||!take(4,n))return false;
        for(unsigned c=0;c<4;++c) {
            if(has_mult) {
                std::int32_t raw=0;
                if(!signed_value(n,raw))return false;
                out.multiply[c]=double(raw)/256.0;
            }
        }
        for(unsigned c=0;c<4;++c) {
            if(has_add && !signed_value(n,out.add[c]))return false;
        }
        align();return bit<=d.size()*8;
    }
    bool cstring(std::string& s) {
        if(bit%8) return false;
        const auto start=pos();
        while (bit<d.size()*8 && d[pos()]!=0) bit+=8;
        if(bit>=d.size()*8) return false;
        s.assign(reinterpret_cast<const char*>(d.data()+start),pos()-start);
        bit+=8; return true;
    }
};
}
TimelineResult build_timeline(const Document& document) {
    std::map<std::uint16_t,DisplayEntry> display;
    std::vector<FrameSnapshot> frames;
    for(const auto& tag: document.tags) {
        const auto& d=tag.data;
        if(tag.code==4) { // PlaceObject: CharacterId, Depth, MATRIX, optional CXFORM
            if(d.size()<5) return fail("PlaceObject truncated");
            const auto id=u16(d,0),depth=u16(d,2);
            Reader reader{d,32};
            Matrix matrix;
            if(!reader.matrix(matrix)) return fail("PlaceObject MATRIX truncated");
            if(reader.pos()<d.size()) {
                // PlaceObject contains CXFORM (RGB), not CXFORMWITHALPHA;
                // defer color-transform support rather than misinterpret it.
                return fail("PlaceObject CXFORM not supported");
            }
            DisplayEntry entry;
            entry.depth=depth; entry.character_id=id; entry.placed_frame=frames.size(); entry.has_matrix=true; entry.matrix=matrix;
            display[depth]=std::move(entry);
        } else if(tag.code==70) { // PlaceObject3
            if(d.size()<4)return fail("PlaceObject3 truncated");
            const std::uint8_t flags=d[0],flags2=d[1];
            const std::uint16_t depth=u16(d,2);
            const bool move=(flags&0x01)!=0;
            const bool has_character=(flags&0x02)!=0;
            const bool has_matrix=(flags&0x04)!=0;
            const bool has_color=(flags&0x08)!=0;
            const bool has_ratio=(flags&0x10)!=0;
            const bool has_name=(flags&0x20)!=0;
            // PlaceObject3 extends PlaceObject2 with filter/blend/cache,
            // class-name, image and visibility/opaque-background options.
            // Reject unsupported visual features instead of losing them.
            if((flags&0xc0)!=0 || (flags2&~std::uint8_t(0x02))!=0)
                return fail("PlaceObject3 clip/color/extended features unsupported");
            Reader reader{d,32};
            std::uint16_t id=0;
            if(has_character) {
                if(d.size()-reader.pos()<2)
                    return fail("PlaceObject3 character ID truncated");
                id=u16(d,reader.pos());
                reader.bit+=16;
            }
            Matrix matrix;
            if(has_matrix && !reader.matrix(matrix))
                return fail("PlaceObject3 MATRIX truncated");
            ColorTransform color;
            if(has_color && !reader.cxform_alpha(color))
                return fail("PlaceObject3 CXFORMWITHALPHA truncated");
            if(has_ratio) {
                if(d.size()-reader.pos()<2)
                    return fail("PlaceObject3 ratio truncated");
                reader.bit+=16;
            }
            std::string name;
            if(has_name && !reader.cstring(name))
                return fail("PlaceObject3 name truncated");
            std::uint8_t blend_mode=1;
            if(flags2&0x02) {
                if(d.size()-reader.pos()<1)return fail("PlaceObject3 blend mode truncated");
                blend_mode=d[reader.pos()];
                reader.bit+=8;
                if(blend_mode!=1 && blend_mode!=3 && blend_mode!=4 &&
                   blend_mode!=7 && blend_mode!=8 && blend_mode!=9 &&
                   blend_mode!=10 && blend_mode!=11 && blend_mode!=12)
                    return fail("PlaceObject3 blend mode unsupported");
            }
            if(reader.pos()!=d.size())
                return fail("PlaceObject3 trailing bytes");
            const auto it=display.find(depth);
            if(move) {
                if(it==display.end())
                    return fail("PlaceObject3 move to unoccupied depth");
                auto entry=it->second;
                if(has_character) {
                    entry.character_id=id;
                    entry.placed_frame=frames.size();
                }
                if(has_matrix) {
                    entry.has_matrix=true;
                    entry.matrix=matrix;
                }
                if(has_color) {entry.has_color_transform=true;entry.color_transform=color;}
                if(flags2&0x02)entry.blend_mode=blend_mode;
                if(has_name)entry.name=std::move(name);
                display[depth]=std::move(entry);
            } else {
                if(!has_character)
                    return fail("PlaceObject3 insertion missing character");
                if(it!=display.end())
                    return fail("PlaceObject3 insertion into occupied depth");
                DisplayEntry entry;
                entry.depth=depth;
                entry.character_id=id;
                entry.placed_frame=frames.size();
                entry.has_matrix=has_matrix;
                entry.matrix=matrix;
                entry.name=std::move(name);
                entry.has_color_transform=has_color;
                entry.color_transform=color;
                entry.blend_mode=blend_mode;
                display[depth]=std::move(entry);
            }
        } else if(tag.code==26) { // PlaceObject2
            if(d.size()<3) return fail("PlaceObject2 truncated");
            const std::uint8_t flags=d[0];
            const std::uint16_t depth=u16(d,1);
            if(flags&0x80) return fail("PlaceObject2 reserved flag set");
            const bool has_clip_actions=flags&0x80;
            const bool has_clip_depth=flags&0x40;
            const bool has_name=flags&0x20;
            const bool has_ratio=flags&0x10;
            const bool has_color=flags&0x08;
            const bool has_matrix=flags&0x04;
            const bool has_character=flags&0x02;
            const bool move=flags&0x01;
            if(has_clip_actions || has_clip_depth) return fail("PlaceObject2 clip features not implemented");
            Reader reader{d,24};
            std::uint16_t id=0;
            if(has_character) {
                if(d.size()-reader.pos()<2) return fail("PlaceObject2 missing character ID");
                id=u16(d,reader.pos());reader.bit+=16;
            }
            Matrix matrix;
            if(has_matrix && !reader.matrix(matrix)) return fail("PlaceObject2 MATRIX truncated");
            ColorTransform color;
            if(has_color && !reader.cxform_alpha(color)) return fail("PlaceObject2 CXFORM truncated");
            if(has_ratio) {
                if(d.size()-reader.pos()<2) return fail("PlaceObject2 ratio truncated");
                reader.bit+=16;
            }
            std::string name;
            if(has_name && !reader.cstring(name)) return fail("PlaceObject2 name truncated");
            if(reader.pos()!=d.size()) return fail("PlaceObject2 trailing bytes");
            const auto it=display.find(depth);
            if(move) {
                if(it==display.end()) return fail("PlaceObject2 move to unoccupied depth");
                auto entry=it->second;
                if(has_character) { entry.character_id=id; entry.placed_frame=frames.size(); }
                if(has_matrix) { entry.has_matrix=true; entry.matrix=matrix; }
                if(has_color) { entry.has_color_transform=true; entry.color_transform=color; }
                if(has_name) entry.name=std::move(name);
                display[depth]=std::move(entry);
            } else {
                if(!has_character) return fail("PlaceObject2 insertion missing character");
                if(it!=display.end()) return fail("PlaceObject2 insertion into occupied depth");
                DisplayEntry entry;
                entry.depth=depth; entry.character_id=id; entry.placed_frame=frames.size(); entry.has_matrix=has_matrix;
                entry.matrix=matrix; entry.name=std::move(name);
                entry.has_color_transform=has_color;entry.color_transform=color;
                display[depth]=std::move(entry);
            }
        } else if(tag.code==5) { // RemoveObject: CharacterId, Depth
            if(d.size()!=4) return fail("RemoveObject invalid size");
            const auto id=u16(d,0),depth=u16(d,2);
            const auto it=display.find(depth);
            if(it==display.end() || it->second.character_id!=id)
                return fail("RemoveObject did not match displayed character");
            display.erase(it);
        } else if(tag.code==28) { // RemoveObject2: Depth
            if(d.size()!=2) return fail("RemoveObject2 invalid size");
            display.erase(u16(d,0));
        } else if(tag.code==1) { // ShowFrame
            if(!d.empty()) return fail("ShowFrame must be empty");
            FrameSnapshot frame;
            for(const auto& [depth,entry]:display) frame.objects.push_back(entry);
            frames.push_back(std::move(frame));
            if(frames.size()>document.header.frame_count)
                return fail("ShowFrame exceeds declared frame count");
        }
    }
    if(frames.size()!=document.header.frame_count)
        return fail("ShowFrame count differs from header");
    return {Error::none,{},std::move(frames)};
}
// DefineSprite contains a UI16 ID, UI16 frame count, then an independent
// sequence of regular SWF tag records terminated by End (code 0).
SpriteTimelineResult build_sprite_timeline(const Tag& tag,Limits limits) {
    auto fail_sprite=[](Error error,const char* message)->SpriteTimelineResult {
        return {error,message,{}};
    };
    if(tag.code!=39)return fail_sprite(Error::malformed_tag,"not a DefineSprite tag");
    const auto& data=tag.data;
    if(data.size()<6)return fail_sprite(Error::truncated,"DefineSprite header truncated");
    if(data.size()>limits.max_tag_bytes || data.size()>limits.max_uncompressed_bytes)
        return fail_sprite(Error::size_limit,"DefineSprite byte limit exceeded");
    Document nested;
    const auto id=u16(data,0);
    nested.header.frame_count=u16(data,2);
    std::size_t pos=4;
    bool saw_end=false;
    while(pos<data.size()) {
        if(data.size()-pos<2)return fail_sprite(Error::truncated,"sprite tag header truncated");
        const std::size_t offset=pos;
        const auto header=u16(data,pos);
        pos+=2;
        const std::uint16_t code=header>>6;
        std::size_t length=header&63u;
        if(length==63u) {
            if(data.size()-pos<4)return fail_sprite(Error::truncated,"sprite long tag header truncated");
            length=std::size_t(data[pos]) | (std::size_t(data[pos+1])<<8) |
                   (std::size_t(data[pos+2])<<16) | (std::size_t(data[pos+3])<<24);
            pos+=4;
        }
        if(length>limits.max_tag_bytes)return fail_sprite(Error::size_limit,"sprite tag size limit exceeded");
        if(length>data.size()-pos)return fail_sprite(Error::truncated,"sprite tag data truncated");
        if(code==0) {
            if(length!=0 || pos!=data.size())
                return fail_sprite(Error::malformed_tag,"invalid sprite End tag");
            saw_end=true;
            break;
        }
        if(nested.tags.size()>=limits.max_tags)
            return fail_sprite(Error::size_limit,"sprite tag count limit exceeded");
        Tag child;
        child.code=code;
        child.offset=tag.offset+offset;
        child.data.assign(data.begin()+pos,data.begin()+pos+length);
        nested.tags.push_back(std::move(child));
        pos+=length;
    }
    if(!saw_end)return fail_sprite(Error::malformed_tag,"DefineSprite missing End tag");
    auto timeline=build_timeline(nested);
    if(!timeline)return {timeline.error,timeline.message,{}};
    return {Error::none,{},SpriteTimeline{id,std::move(timeline.frames)}};
}

}
