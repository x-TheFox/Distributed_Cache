#include "protocol/resp/resp_parser.hpp"

#include <algorithm>
#include <cctype>
#include <optional>

#include "core/concurrent_store.hpp"

namespace cache::protocol::resp {
namespace {
std::string ToUpper(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
  return value;
}

std::string EncodeBulkString(const std::string& value) {
  return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
}
}  // namespace

std::string ExecuteCommand(const RESPCommand& cmd,
                           cache::core::ConcurrentStore& store) {
  auto name = ToUpper(cmd.name);
  if (name == "SET") {
    if (cmd.args.size() != 2) {
      return "-ERR wrong number of arguments\r\n";
    }
    store.Set(cmd.args[0], cmd.args[1], std::nullopt);
    return "+OK\r\n";
  }
  if (name == "GET") {
    if (cmd.args.size() != 1) {
      return "-ERR wrong number of arguments\r\n";
    }
    auto value = store.Get(cmd.args[0]);
    if (!value.has_value()) {
      return "$-1\r\n";
    }
    return EncodeBulkString(*value);
  }
  return "-ERR unknown command\r\n";
}
}  // namespace cache::protocol::resp
