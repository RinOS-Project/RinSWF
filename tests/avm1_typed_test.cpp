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
        0x96,7,0,0,'s','c','o','r','e',0,
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
    // A Push string cannot exceed the VM's value budget.
    constexpr std::array<std::uint8_t,10> string_program={
        0x96,6,0,0,'a','b','c','d',0,0
    };
    rinswf::Avm1Context memory_context;
    rinswf::Avm1Limits memory_limits;
    memory_limits.max_value_bytes=2;
    if(rinswf::execute_avm1_typed(string_program,memory_context,memory_limits))
        return 7;
    if(!memory_context.variables.empty())return 8;
    // AVM1 double 1.5 uses DWORD-swapped encoding.
    constexpr std::array<std::uint8_t,13> double_program={
        0x96,9,0,6,0,0,0xf8,0x3f,0,0,0,0,0
    };
    rinswf::Avm1Context double_context;
    const auto doubled=rinswf::execute_avm1_typed(double_program,double_context);
    if(!doubled || doubled.stack.size()!=1 ||
       !std::holds_alternative<double>(doubled.stack[0]) ||
       std::get<double>(doubled.stack[0])!=1.5)return 9;
    // Push 4, Duplicate, Increment, Swap, Decrement, Not, End.
    constexpr std::array<std::uint8_t,14> stack_program={
        0x96,5,0,7,4,0,0,0,0x4c,0x50,0x4d,0x51,0x12,0
    };
    rinswf::Avm1Context stack_context;
    const auto stacked=rinswf::execute_avm1_typed(stack_program,stack_context);
    if(!stacked || stacked.stack.size()!=2 ||
       !std::holds_alternative<double>(stacked.stack[0]) ||
       std::get<double>(stacked.stack[0])!=5 ||
       !std::holds_alternative<bool>(stacked.stack[1]) ||
       std::get<bool>(stacked.stack[1]))return 10;
    constexpr std::array<std::uint8_t,2> duplicate_underflow={0x4c,0};
    if(rinswf::execute_avm1_typed(duplicate_underflow,stack_context))return 11;
    constexpr std::array<std::uint8_t,2> swap_underflow={0x4d,0};
    if(rinswf::execute_avm1_typed(swap_underflow,stack_context))return 12;
    return 0;
}
