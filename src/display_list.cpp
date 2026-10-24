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
    bool matrix() {
        std::uint32_t has=0,n=0;
        if(!take(1,has)) return false;
        if(has && (!take(5,n)||!skip(std::size_t(2)*n))) return false;
        if(!take(1,has)) return false;
        if(has && (!take(5,n)||!skip(std::size_t(2)*n))) return false;
        if(!take(5,n)||!skip(std::size_t(2)*n)) return false;
        align(); return bit<=d.size()*8;
    }
    bool cxform_alpha() {
        std::uint32_t add=0,mult=0,n=0;
        if(!take(1,add)||!take(1,mult)||!take(4,n)) return false;
        if(mult && !skip(std::size_t(4)*n)) return false;
        if(add && !skip(std::size_t(4)*n)) return false;
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
            if(!reader.matrix()) return fail("PlaceObject MATRIX truncated");
            if(reader.pos()<d.size()) {
                // PlaceObject contains CXFORM (RGB), not CXFORMWITHALPHA;
                // defer color-transform support rather than misinterpret it.
                return fail("PlaceObject CXFORM not supported");
            }
            display[depth]={depth,id,true,{}};
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
            if(has_matrix && !reader.matrix()) return fail("PlaceObject2 MATRIX truncated");
            if(has_color && !reader.cxform_alpha()) return fail("PlaceObject2 CXFORM truncated");
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
                if(has_character) entry.character_id=id;
                if(has_matrix) entry.has_matrix=true;
                if(has_name) entry.name=std::move(name);
                display[depth]=std::move(entry);
            } else {
                if(!has_character) return fail("PlaceObject2 insertion missing character");
                if(it!=display.end()) return fail("PlaceObject2 insertion into occupied depth");
                display[depth]={depth,id,has_matrix,std::move(name)};
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
}
