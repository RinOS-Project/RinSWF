// SPDX-License-Identifier: MIT
#include "rinswf/avm1.hpp"
#include <array>
#include <string>
#include <cmath>
#include <limits>
#include <vector>
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
    // Concatenate two AVM1 strings and compare the result.
    constexpr std::array<std::uint8_t,22> strings={
        0x96,3,0,0,'a',0,0x96,3,0,0,'b',0,
        0x21,0x96,4,0,0,'a','b',0,0x13,0
    };
    const auto joined=rinswf::execute_avm1_typed(strings,stack_context);
    if(!joined || joined.stack.size()!=1 ||
       !std::holds_alternative<bool>(joined.stack[0]) ||
       !std::get<bool>(joined.stack[0]))return 13;
    constexpr std::array<std::uint8_t,13> integer_program={
        0x96,5,0,7,4,0,0,0,0x50,0x18,0x4c,0x0e,0
    };
    const auto integer=rinswf::execute_avm1_typed(integer_program,stack_context);
    if(!integer || integer.stack.size()!=1 ||
       !std::holds_alternative<double>(integer.stack[0]) ||
       std::get<double>(integer.stack[0])!=1)return 14;
    // Push "12", ToNumber, Increment, ToString, End -> "13".
    constexpr std::array<std::uint8_t,11> convert_program={
        0x96,4,0,0,'1','2',0,0x4a,0x50,0x4b,0
    };
    const auto converted=rinswf::execute_avm1_typed(convert_program,stack_context);
    if(!converted || converted.stack.size()!=1 ||
       !std::holds_alternative<std::string>(converted.stack[0]) ||
       std::get<std::string>(converted.stack[0])!="13")return 15;
    // Invalid numeric text must reject without mutating variables.
    constexpr std::array<std::uint8_t,9> invalid_number={
        0x96,4,0,0,'x','y',0,0x4a,0
    };
    if(rinswf::execute_avm1_typed(invalid_number,stack_context))return 16;
    // SWF Push type 2 is Null; type 3 is Undefined.
    constexpr std::array<std::uint8_t,6> null_push={
        0x96,2,0,2,3,0
    };
    const auto nulls=rinswf::execute_avm1_typed(null_push,stack_context);
    if(!nulls || nulls.stack.size()!=2 ||
       !std::holds_alternative<rinswf::Avm1Null>(nulls.stack[0]) ||
       !std::holds_alternative<std::monostate>(nulls.stack[1]))return 17;
    constexpr std::array<std::uint8_t,6> null_number={
        0x96,1,0,2,0x4a,0
    };
    const auto num_null=rinswf::execute_avm1_typed(null_number,stack_context);
    if(!num_null || num_null.stack.size()!=1 ||
       !std::holds_alternative<double>(num_null.stack[0]) ||
       std::get<double>(num_null.stack[0])!=0)return 18;
    constexpr std::array<std::uint8_t,6> undefined_number={
        0x96,1,0,3,0x4a,0
    };
    const auto nan_value=rinswf::execute_avm1_typed(undefined_number,stack_context);
    if(!nan_value || nan_value.stack.size()!=1 ||
       !std::holds_alternative<double>(nan_value.stack[0]) ||
       !std::isnan(std::get<double>(nan_value.stack[0])))return 19;
    constexpr std::array<std::uint8_t,7> nan_equal={
        0x96,2,0,3,3,0x0e,0
    };
    const auto equality=rinswf::execute_avm1_typed(nan_equal,stack_context);
    if(!equality || equality.stack.size()!=1 ||
       !std::holds_alternative<double>(equality.stack[0]) ||
       std::get<double>(equality.stack[0])!=0)return 20;
    // Undefined + 1 yields NaN without halting script execution.
    constexpr std::array<std::uint8_t,11> nan_add={
        0x96,1,0,3,0x96,5,0,7,1,0,0
    };
    // Build a complete stream using a vector to append the missing bytes.
    std::vector<std::uint8_t> nan_program(nan_add.begin(),nan_add.end());
    nan_program.push_back(0);nan_program.push_back(0x0a);nan_program.push_back(0);
    const auto nan_added=rinswf::execute_avm1_typed(nan_program,stack_context);
    if(!nan_added || nan_added.stack.size()!=1 ||
       !std::holds_alternative<double>(nan_added.stack[0]) ||
       !std::isnan(std::get<double>(nan_added.stack[0])))return 21;
    // 1 / 0 produces positive Infinity; 0 / 0 produces NaN.
    constexpr std::array<std::uint8_t,18> infinity_program={
        0x96,5,0,7,1,0,0,0,0x96,5,0,7,0,0,0,0,0x0d,0
    };
    const auto infinity=rinswf::execute_avm1_typed(infinity_program,stack_context);
    if(!infinity || infinity.stack.size()!=1 ||
       !std::isinf(std::get<double>(infinity.stack[0])) ||
       std::signbit(std::get<double>(infinity.stack[0])))return 22;
    return 0;
}
