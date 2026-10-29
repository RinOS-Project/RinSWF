// SPDX-License-Identifier: MIT
#pragma once
#include "rinswf/swf.hpp"
#include <cstdint>
#include <span>
#include <vector>
namespace rinswf {
// A deliberately capability-free AVM1 arithmetic core. No host API, file,
// network, display-list mutation, or implicit ActionScript execution.
struct Avm1Limits {
    std::size_t max_actions=10000;
    std::size_t max_stack=1024;
    std::size_t max_bytes=1024*1024;
};
struct Avm1Result {
    Error error=Error::none;
    std::string message;
    std::vector<double> stack;
    explicit operator bool() const noexcept {return error==Error::none;}
};
// Supports End, Push(Number/Integer), Pop, Add, Subtract, Multiply, Divide.
// Any other opcode is rejected instead of being silently ignored.
[[nodiscard]] Avm1Result execute_avm1(std::span<const std::uint8_t> bytecode,
                                      Avm1Limits limits={});
}
