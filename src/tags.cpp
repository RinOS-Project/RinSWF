// SPDX-License-Identifier: MIT
#include "rinswf/tags.hpp"
namespace rinswf {
namespace {
std::uint16_t le16(const std::vector<std::uint8_t>& v, std::size_t p) { return std::uint16_t(v[p]) | (std::uint16_t(v[p+1])<<8); }
std::uint32_t le32(const std::vector<std::uint8_t>& v, std::size_t p) { return std::uint32_t(v[p]) | (std::uint32_t(v[p+1])<<8) | (std::uint32_t(v[p+2])<<16) | (std::uint32_t(v[p+3])<<24); }
TagDecodeResult malformed(const char* message) { return {Error::malformed_tag,message,{}}; }
bool read_string(const std::vector<std::uint8_t>& d, std::size_t& p, std::string& out) {
    const auto start=p;
    while (p<d.size() && d[p]!=0) ++p;
    if (p==d.size()) return false;
    out.assign(reinterpret_cast<const char*>(d.data()+start),p-start); ++p;
    return true;
}
}
TagDecodeResult decode_tag(const Tag& tag) {
    const auto& d=tag.data;
    switch (tag.code) {
        case 9:
            if (d.size()!=3) return malformed("SetBackgroundColor must contain three bytes");
            return {Error::none,{},TypedTag{BackgroundColor{Rgb{d[0],d[1],d[2],255}}}};
        case 69: {
            if (d.size()!=4) return malformed("FileAttributes must contain four bytes");
            const auto flags=le32(d,0);
            if ((flags & ~std::uint32_t(0x79))!=0) return malformed("reserved FileAttributes bits are nonzero");
            FileAttributes attr;
            attr.use_direct_blit=(flags & (1u<<6))!=0;
            attr.use_gpu=(flags & (1u<<5))!=0;
            attr.has_metadata=(flags & (1u<<4))!=0;
            attr.action_script3=(flags & (1u<<3))!=0;
            attr.use_network=(flags & 1u)!=0;
            return {Error::none,{},TypedTag{attr}};
        }
        case 43: {
            std::size_t p=0; std::string name;
            if (!read_string(d,p,name)) return malformed("FrameLabel missing NUL terminator");
            bool anchor=false;
            if (p<d.size()) { if (p+1!=d.size() || d[p]!=1) return malformed("invalid FrameLabel anchor"); anchor=true; }
            return {Error::none,{},TypedTag{FrameLabel{std::move(name),anchor}}};
        }
        case 76: {
            if (d.size()<2) return malformed("SymbolClass missing count");
            std::size_t p=2; const std::size_t count=le16(d,0);
            if (count>(d.size()-p)/3) return malformed("SymbolClass entries truncated");
            SymbolClass symbols; symbols.entries.reserve(count);
            for (std::size_t i=0;i<count;++i) {
                if (d.size()-p<2) return malformed("SymbolClass ID truncated");
                const auto id=le16(d,p); p+=2; std::string name;
                if (!read_string(d,p,name)) return malformed("SymbolClass name unterminated");
                symbols.entries.push_back({id,std::move(name)});
            }
            if (p!=d.size()) return malformed("SymbolClass trailing bytes");
            return {Error::none,{},TypedTag{std::move(symbols)}};
        }
        case 87: {
            if (d.size()<6) return malformed("DefineBinaryData header truncated");
            if (le32(d,2)!=0) return malformed("DefineBinaryData reserved field nonzero");
            BinaryData binary; binary.character_id=le16(d,0);
            binary.bytes.assign(d.begin()+6,d.end());
            return {Error::none,{},TypedTag{std::move(binary)}};
        }
        default: return {};
    }
}
}
