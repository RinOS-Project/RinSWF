// SPDX-License-Identifier: MIT
#include "rinswf/tags.hpp"
#include <cassert>
#include <variant>
using namespace rinswf;
int main() {
    Tag background{9,0,{12,34,56}};
    auto color=decode_tag(background);
    assert(color && color.value);
    assert(std::get<BackgroundColor>(*color.value).color.green==34);
    assert(decode_tag({9,0,{1}}).error==Error::malformed_tag);
    auto attr=decode_tag({69,0,{0x08,0,0,0}});
    assert(attr && std::get<FileAttributes>(*attr.value).action_script3);
    auto label=decode_tag({43,0,{'S','t','a','r','t',0}});
    assert(label && std::get<FrameLabel>(*label.value).name=="Start");
    assert(decode_tag({43,0,{'a'}}).error==Error::malformed_tag);
    auto symbol=decode_tag({76,0,{1,0,42,0,'M','a','i','n',0}});
    assert(symbol && std::get<SymbolClass>(*symbol.value).entries[0].character_id==42);
    assert(decode_tag({76,0,{1,0,42,0,'X'}}).error==Error::malformed_tag);
    auto binary=decode_tag({87,0,{7,0,0,0,0,0,0xaa}});
    assert(binary && std::get<BinaryData>(*binary.value).bytes[0]==0xaa);
    assert(!decode_tag({999,0,{}}).value);
}
