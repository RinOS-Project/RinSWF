// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/swf.hpp"
#include <cstdint>
#include <span>
#include <vector>
#include <map>
#include <string>
#include <variant>
namespace rinswf {
// A deliberately capability-free AVM1 arithmetic core. No host API, file,
// network, display-list mutation, or implicit ActionScript execution.
struct Avm1ExecutionBudget {
    std::size_t remaining_actions=0;
};
struct Avm1Limits {
    std::size_t max_actions=10000;
    std::size_t max_stack=1024;
    std::size_t max_bytes=1024*1024;
    std::size_t max_value_bytes=256*1024; // approximate stack + variables
    Avm1ExecutionBudget* shared_budget=nullptr; // optional, caller-owned
};
struct Avm1Null {};
using Avm1Value=std::variant<std::monostate,double,bool,std::string,Avm1Null>; // monostate = undefined
struct Avm1Context {
    std::map<std::string,Avm1Value> variables;
    std::size_t max_variables=1024;
};
struct Avm1TimelineCommand {
    bool playing=true;
    bool sets_playing=false;
    bool has_frame=false;
    std::size_t frame=0; // zero based
    bool has_label=false;
    std::string label;
};
struct Avm1TypedResult {
    Error error=Error::none;
    std::string message;
    std::vector<Avm1Value> stack;
    explicit operator bool() const noexcept {return error==Error::none;}
};
// Executes only the supported subset; commits variable writes on success.
// Host IO, native objects, scope chains, prototypes and timeline actions are denied.
[[nodiscard]] Avm1TypedResult execute_avm1_typed(
    std::span<const std::uint8_t> bytecode,Avm1Context& context,
    Avm1Limits limits={},Avm1TimelineCommand* timeline=nullptr);
struct Avm1Result {
    Error error=Error::none;
    std::string message;
    std::vector<double> stack;
    explicit operator bool() const noexcept {return error==Error::none;}
};
// Supports End, Push(Number/Integer), Pop, arithmetic, numeric comparison,\n// Jump, and If; only validated action boundaries may be branch targets.
// Any other opcode is rejected instead of being silently ignored.
[[nodiscard]] Avm1Result execute_avm1(std::span<const std::uint8_t> bytecode,
                                      Avm1Limits limits={});
}
