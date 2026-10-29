// SPDX-License-Identifier: MIT
#include "rinswf/avm1.hpp"
#include <cmath>
#include <cstring>
#include <limits>
namespace rinswf {
namespace {
Avm1Result fail(Error error,const char* why){return {error,why,{}};}
std::uint32_t le32(std::span<const std::uint8_t> s,std::size_t p) {
    return std::uint32_t(s[p])|(std::uint32_t(s[p+1])<<8)|
        (std::uint32_t(s[p+2])<<16)|(std::uint32_t(s[p+3])<<24);
}
}
Avm1Result execute_avm1(std::span<const std::uint8_t> data,Avm1Limits limits) {
    if(data.size()>limits.max_bytes)
        return fail(Error::size_limit,"AVM1 bytecode limit");
    std::vector<double> stack;
    std::size_t pc=0,actions=0;
    while(pc<data.size()) {
        if(++actions>limits.max_actions)
            return fail(Error::size_limit,"AVM1 instruction budget");
        const auto opcode=data[pc++];
        if(opcode==0) {
            if(pc!=data.size())return fail(Error::malformed_tag,"AVM1 bytes after End");
            return {Error::none,{},std::move(stack)};
        }
        std::size_t length=0;
        if(opcode>=0x80) {
            if(data.size()-pc<2)return fail(Error::truncated,"AVM1 action header");
            length=std::size_t(data[pc])|(std::size_t(data[pc+1])<<8);
            pc+=2;
            if(length>data.size()-pc)return fail(Error::truncated,"AVM1 action payload");
        }
        const auto payload=data.subspan(pc,length);
        pc+=length;
        if(opcode==0x96) {
            std::size_t i=0;
            while(i<payload.size()) {
                const auto type=payload[i++];
                double value=0;
                if(type==7) {
                    if(payload.size()-i<4)return fail(Error::truncated,"AVM1 integer");
                    value=static_cast<std::int32_t>(le32(payload,i));i+=4;
                } else if(type==1) {
                    if(payload.size()-i<4)return fail(Error::truncated,"AVM1 float");
                    const auto bits=le32(payload,i);i+=4;
                    float f=0;std::memcpy(&f,&bits,4);value=f;
                } else if(type==6) {
                    if(payload.size()-i<8)return fail(Error::truncated,"AVM1 double");
                    // AVM1 doubles encode high and low DWORDs in swapped order.
                    const std::uint64_t bits=(std::uint64_t(le32(payload,i))<<32)|
                        le32(payload,i+4);
                    i+=8;std::memcpy(&value,&bits,8);
                } else return fail(Error::malformed_tag,"AVM1 Push type unsupported");
                if(!std::isfinite(value))return fail(Error::malformed_tag,"AVM1 nonfinite number");
                if(stack.size()>=limits.max_stack)
                    return fail(Error::size_limit,"AVM1 stack limit");
                stack.push_back(value);
            }
        } else if(opcode==0x17) {
            if(stack.empty())return fail(Error::malformed_tag,"AVM1 stack underflow");
            stack.pop_back();
        } else if(opcode==0x0a||opcode==0x0b||opcode==0x0c||opcode==0x0d) {
            if(stack.size()<2)return fail(Error::malformed_tag,"AVM1 stack underflow");
            const double b=stack.back();stack.pop_back();
            const double a=stack.back();stack.pop_back();
            const double result=opcode==0x0a?a+b:opcode==0x0b?a-b:
                opcode==0x0c?a*b:a/b;
            if(!std::isfinite(result))
                return fail(Error::malformed_tag,"AVM1 nonfinite result");
            stack.push_back(result);
        } else return fail(Error::malformed_tag,"AVM1 opcode unsupported");
    }
    return fail(Error::truncated,"AVM1 End action missing");
}
}
