// SPDX-License-Identifier: MIT
#include "rinswf/avm1.hpp"
#include <cmath>
#include <cstring>
#include <cstdint>
#include <utility>
#include <set>
#include <algorithm>
namespace rinswf {
namespace {
Avm1TypedResult fail(Error error,const char* message){return {error,message,{}};}
std::uint32_t get32(std::span<const std::uint8_t> b,std::size_t p) {
    return std::uint32_t(b[p])|(std::uint32_t(b[p+1])<<8)|
        (std::uint32_t(b[p+2])<<16)|(std::uint32_t(b[p+3])<<24);
}
bool truth(const Avm1Value& v) {
    if(auto n=std::get_if<double>(&v))return *n!=0&&!std::isnan(*n);
    if(auto b=std::get_if<bool>(&v))return *b;
    if(auto s=std::get_if<std::string>(&v))return !s->empty();
    return false;
}
bool number(const Avm1Value& v,double& out) {
    if(auto n=std::get_if<double>(&v)){out=*n;return true;}
    if(auto b=std::get_if<bool>(&v)){out=*b?1:0;return true;}
    if(std::holds_alternative<std::monostate>(v)){out=0;return true;}
    return false;
}
}
Avm1TypedResult execute_avm1_typed(std::span<const std::uint8_t> bytes,
                                   Avm1Context& context,Avm1Limits limits,Avm1TimelineCommand* timeline) {
    if(bytes.size()>limits.max_bytes)return fail(Error::size_limit,"AVM1 byte limit");
    std::set<std::size_t> boundaries;
    std::size_t pc=0;bool end=false;
    while(pc<bytes.size()) {
        boundaries.insert(pc);
        const auto op=bytes[pc++];
        if(op==0) {
            if(pc!=bytes.size())return fail(Error::malformed_tag,"trailing AVM1 data");
            end=true;break;
        }
        if(op>=128) {
            if(bytes.size()-pc<2)return fail(Error::truncated,"action length truncated");
            const auto n=std::size_t(bytes[pc])|(std::size_t(bytes[pc+1])<<8);
            pc+=2;
            if(n>bytes.size()-pc)return fail(Error::truncated,"action payload truncated");
            pc+=n;
        }
    }
    if(!end)return fail(Error::truncated,"AVM1 End missing");
    auto variables=context.variables;
    Avm1TimelineCommand pending=timeline?*timeline:Avm1TimelineCommand{};
    std::vector<Avm1Value> stack;
    pc=0;std::size_t steps=0;
    auto pop=[&]() {Avm1Value v=std::move(stack.back());stack.pop_back();return v;};
    while(pc<bytes.size()) {
        if(++steps>limits.max_actions)return fail(Error::size_limit,"AVM1 instruction limit");
        const auto op=bytes[pc++];
        if(op==0) {
            context.variables=std::move(variables);
            if(timeline)*timeline=pending;
            return {Error::none,{},std::move(stack)};
        }
        std::size_t length=0;
        if(op>=128) {
            length=std::size_t(bytes[pc])|(std::size_t(bytes[pc+1])<<8);
            pc+=2;
        }
        auto payload=bytes.subspan(pc,length);
        pc+=length;
        if(op==0x96) {
            for(std::size_t i=0;i<payload.size();) {
                const auto type=payload[i++];
                Avm1Value value;
                if(type==0) {
                    const auto start=i;
                    while(i<payload.size()&&payload[i])++i;
                    if(i==payload.size())return fail(Error::truncated,"Push string unterminated");
                    value=std::string(reinterpret_cast<const char*>(payload.data()+start),i-start);
                    ++i;
                } else if(type==1||type==7) {
                    if(payload.size()-i<4)return fail(Error::truncated,"Push number truncated");
                    auto bits=get32(payload,i);i+=4;
                    if(type==7)value=double(static_cast<std::int32_t>(bits));
                    else {float v;std::memcpy(&v,&bits,4);value=double(v);}
                } else if(type==5) {
                    if(i==payload.size())return fail(Error::truncated,"Push Boolean truncated");
                    value=bool(payload[i++]);
                } else if(type==2||type==3) {
                    value=std::monostate{}; // null/undefined retained as sentinel
                } else return fail(Error::malformed_tag,"unsupported Push type");
                if(stack.size()>=limits.max_stack)return fail(Error::size_limit,"AVM1 stack limit");
                stack.push_back(std::move(value));
            }
        } else if(op==0x06 || op==0x07) {
            if(!timeline)return fail(Error::malformed_tag,"timeline control not enabled");
            pending.playing=(op==0x06);
        } else if(op==0x81) {
            if(!timeline || length!=2)
                return fail(Error::malformed_tag,"invalid GotoFrame action");
            pending.frame=std::size_t(payload[0])|(std::size_t(payload[1])<<8);
            pending.has_frame=true;
            pending.has_label=false;
            pending.label.clear();
        } else if(op==0x8c) { // GoToLabel
            if(!timeline || payload.size()<2 || payload.back()!=0)
                return fail(Error::malformed_tag,"invalid GoToLabel action");
            const auto terminator=std::find(payload.begin(),payload.end(),std::uint8_t(0));
            if(terminator!=payload.end()-1 || terminator==payload.begin())
                return fail(Error::malformed_tag,"invalid GoToLabel string");
            pending.label.assign(reinterpret_cast<const char*>(payload.data()),payload.size()-1);
            pending.has_label=true;
            pending.has_frame=false;
        } else if(op==0x9f) { // GotoFrame2: stack frame/label, flags
            if(!timeline || length!=1 || (payload[0]&0xfe)!=0)
                return fail(Error::malformed_tag,"unsupported GotoFrame2 flags or scene bias");
            if(stack.empty())return fail(Error::malformed_tag,"GotoFrame2 stack underflow");
            auto destination=pop();
            if(const auto* label=std::get_if<std::string>(&destination)) {
                if(label->empty() || label->size()>256)
                    return fail(Error::malformed_tag,"invalid GotoFrame2 label");
                pending.label=*label;
                pending.has_label=true;
                pending.has_frame=false;
            } else if(const auto* frame=std::get_if<double>(&destination)) {
                // GotoFrame2's numeric argument is one-based.
                if(!std::isfinite(*frame) || *frame<1 ||
                   *frame>65535 || std::floor(*frame)!=*frame)
                    return fail(Error::malformed_tag,"invalid GotoFrame2 frame");
                pending.frame=static_cast<std::size_t>(*frame)-1;
                pending.has_frame=true;
                pending.has_label=false;
                pending.label.clear();
            } else return fail(Error::malformed_tag,"unsupported GotoFrame2 target type");
            pending.playing=(payload[0]&1)!=0;
        } else if(op==0x17) {
            if(stack.empty())return fail(Error::malformed_tag,"Pop underflow");
            stack.pop_back();
        } else if(op==0x1c) {
            if(stack.empty())return fail(Error::malformed_tag,"GetVariable underflow");
            auto key=pop();
            auto str=std::get_if<std::string>(&key);
            if(!str)return fail(Error::malformed_tag,"GetVariable requires string");
            auto found=variables.find(*str);
            stack.push_back(found==variables.end()?Avm1Value{}:found->second);
        } else if(op==0x1d) {
            if(stack.size()<2)return fail(Error::malformed_tag,"SetVariable underflow");
            auto value=pop(),name=pop();
            auto str=std::get_if<std::string>(&name);
            if(!str||str->empty()||str->size()>256)
                return fail(Error::malformed_tag,"invalid variable name");
            if(!variables.contains(*str)&&variables.size()>=context.max_variables)
                return fail(Error::size_limit,"AVM1 variable limit");
            variables[*str]=std::move(value);
        } else if(op==0x99||op==0x9d) {
            if(length!=2)return fail(Error::malformed_tag,"invalid branch length");
            bool jump=op==0x99;
            if(op==0x9d) {
                if(stack.empty())return fail(Error::malformed_tag,"If underflow");
                jump=truth(pop());
            }
            if(jump) {
                const auto offset=static_cast<std::int16_t>(
                    std::uint16_t(payload[0])|(std::uint16_t(payload[1])<<8));
                const auto target=std::int64_t(pc)+offset;
                if(target<0||target>=std::int64_t(bytes.size())||
                   !boundaries.contains(std::size_t(target)))
                    return fail(Error::malformed_tag,"invalid branch target");
                pc=std::size_t(target);
            }
        } else if(op==0x0a||op==0x0b||op==0x0c||op==0x0d||
                  op==0x0e||op==0x0f) {
            if(stack.size()<2)return fail(Error::malformed_tag,"arithmetic underflow");
            auto right=pop(),left=pop();double a=0,b=0;
            if(!number(left,a)||!number(right,b))
                return fail(Error::malformed_tag,"numeric conversion unsupported");
            double result=op==0x0a?a+b:op==0x0b?a-b:
                op==0x0c?a*b:op==0x0d?a/b:op==0x0e?double(a==b):double(a<b);
            if(!std::isfinite(result))return fail(Error::malformed_tag,"nonfinite result");
            stack.push_back(result);
        } else return fail(Error::malformed_tag,"unsupported AVM1 opcode");
    }
    return fail(Error::truncated,"AVM1 End missing");
}
}
