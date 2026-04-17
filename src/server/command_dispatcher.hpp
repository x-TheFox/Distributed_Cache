#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cache::core {
class ConcurrentStore;
}

namespace cache::server {

enum class CommandStatus {
  kOk,
  kNotFound,
  kError,
  kUnknownCommand,
};

struct CommandResult {
  CommandStatus status;
  std::optional<std::string> value;
  std::string message;
};

CommandResult DispatchCommand(std::string_view command,
                              const std::vector<std::string>& args,
                              cache::core::ConcurrentStore& store);

}  // namespace cache::server
