// SPDX-License-Identifier: MIT
#include "rinswf/avm1.hpp"
#include <array>
#include <cstdint>
int main() {
    // Push integer 6; Push integer 7; Multiply; End => 42.
    constexpr std::array<std::uint8_t,18> program={
        0x96,0x05,0x00,0x07,6,0,0,0,
        0x96,0x05,0x00,0x07,7,0,0,0,0x0c,0
    };
    const auto result=rinswf::execute_avm1(program);
    if(!result || result.stack.size()!=1 || result.stack[0]!=42)return 1;
    constexpr std::array<std::uint8_t,2> underflow={0x0a,0};
    if(rinswf::execute_avm1(underflow))return 2;
    constexpr std::array<std::uint8_t,2> unsupported={0x99,0};
    if(rinswf::execute_avm1(unsupported))return 3;
    if(rinswf::execute_avm1(program,{1,1024,1024}))return 4;
    constexpr std::array<std::uint8_t,4> truncated={0x96,0x05,0x00,0x07};
    if(rinswf::execute_avm1(truncated))return 5;
    // Push 1; If +8 skips the Push 99 action; Push 7; End.
    constexpr std::array<std::uint8_t,29> branch={
        0x96,5,0,7,1,0,0,0, 0x9d,2,0,8,0,
        0x96,5,0,7,99,0,0,0, 0x96,5,0,7,7,0,0,0,0
    };
    const auto conditional=rinswf::execute_avm1(branch);
    if(!conditional || conditional.stack.size()!=1 || conditional.stack[0]!=7)return 8;
    // A backward jump must hit the instruction budget instead of looping.
    constexpr std::array<std::uint8_t,6> loop={0x99,2,0,0xfb,0xff,0};
    if(rinswf::execute_avm1(loop,{5,1024,1024}))return 6;
    // A branch into its own payload is rejected.
    constexpr std::array<std::uint8_t,6> invalid_branch={0x99,2,0,0xfe,0xff,0};
    if(rinswf::execute_avm1(invalid_branch))return 7;
    return 0;
}
