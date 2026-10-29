// SPDX-License-Identifier: MIT
#include "rinswf/avm1.hpp"
#include <array>
#include <string>
int main() {
    rinswf::Avm1Context ctx;
    // Push "score", 42, SetVariable, Push "score", GetVariable, End.
    constexpr std::array<std::uint8_t,31> program={
        0x96,7,0,0,'s','c','o','r','e',0,
        0x96,5,0,7,42,0,0,0,
        0x1d,
        0x96,8,0,0,'s','c','o','r','e',0,
        0x1c,0
    };
    const auto result=rinswf::execute_avm1_typed(program,ctx);
    if(!result || result.stack.size()!=1 ||
       !std::holds_alternative<double>(result.stack[0]) ||
       std::get<double>(result.stack[0])!=42)return 1;
    if(!ctx.variables.contains("score"))return 2;
    constexpr std::array<std::uint8_t,8> bad={
        0x96,3,0,0,'x',0,0x1d,0
    };
    if(rinswf::execute_avm1_typed(bad,ctx))return 3;
    if(ctx.variables.size()!=1)return 4;
    rinswf::Avm1Context limited;limited.max_variables=0;
    if(rinswf::execute_avm1_typed(program,limited))return 5;
    if(!limited.variables.empty())return 6;
    return 0;
}
