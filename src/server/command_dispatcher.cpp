#include "server/command_dispatcher.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include "core/concurrent_store.hpp"
#include "protocol/resp/resp_parser.hpp"

namespace cache::server {
namespace {
constexpr char kEmptyKeyMessage[] = "key must not be empty";

std::string ToUpper(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
  return value;
}

CommandResult ErrorResult(CommandStatus status, std::string message) {
  return CommandResult{status, std::nullopt, std::move(message)};
}
}  // namespace

CommandResult DispatchCommand(std::string_view command,
                              const std::vector<std::string>& args,
                              cache::core::ConcurrentStore& store) {
  auto name = ToUpper(std::string(command));
  if (name == "SET") {
    if (args.size() != 2) {
      return ErrorResult(CommandStatus::kError, "wrong number of arguments");
    }
    if (args[0].empty()) {
      return ErrorResult(CommandStatus::kInvalidKey, kEmptyKeyMessage);
    }
    store.Set(args[0], args[1], std::nullopt);
    return CommandResult{CommandStatus::kOk, std::nullopt, {}};
  }
  if (name == "GET") {
    if (args.size() != 1) {
      return ErrorResult(CommandStatus::kError, "wrong number of arguments");
    }
    if (args[0].empty()) {
      return ErrorResult(CommandStatus::kInvalidKey, kEmptyKeyMessage);
    }
    auto value = store.Get(args[0]);
    if (!value.has_value()) {
      return CommandResult{CommandStatus::kNotFound, std::nullopt, {}};
    }
    return CommandResult{CommandStatus::kOk, *value, {}};
  }
  return ErrorResult(CommandStatus::kUnknownCommand, "unknown command");
}
}  // namespace cache::server

namespace cache::protocol::resp {
namespace {
std::string EncodeBulkString(const std::string& value) {
  return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
}
}  // namespace

std::string ExecuteCommand(const RESPCommand& cmd,
                           cache::core::ConcurrentStore& store) {
  auto result = cache::server::DispatchCommand(cmd.name, cmd.args, store);
  switch (result.status) {
    case cache::server::CommandStatus::kOk:
      if (result.value.has_value()) {
        return EncodeBulkString(*result.value);
      }
      return "+OK\r\n";
    case cache::server::CommandStatus::kNotFound:
      return "$-1\r\n";
    case cache::server::CommandStatus::kError:
    case cache::server::CommandStatus::kInvalidKey:
    case cache::server::CommandStatus::kUnknownCommand:
      return "-ERR " + result.message + "\r\n";
  }
  return "-ERR unknown command\r\n";
}
}  // namespace cache::protocol::resp
