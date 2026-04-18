#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace cache::core {
class ConcurrentStore;
}

namespace cache::protocol::resp {
struct RESPCommand {
  std::string name;
  std::vector<std::string> args;
};

struct RESPParseResult {
  RESPCommand command;
  size_t bytes_consumed{0};
};

RESPParseResult ParseRESP(std::string_view frame);
std::string ExecuteCommand(const RESPCommand& cmd,
                           cache::core::ConcurrentStore& store);
}  // namespace cache::protocol::resp
