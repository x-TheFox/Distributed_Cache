#include "protocol/resp/resp_parser.hpp"

#include <charconv>
#include <limits>
#include <system_error>

namespace cache::protocol::resp {
namespace {
bool ReadLine(std::string_view frame, size_t* offset, std::string_view* line) {
  auto pos = frame.find("\r\n", *offset);
  if (pos == std::string_view::npos) {
    return false;
  }
  *line = frame.substr(*offset, pos - *offset);
  *offset = pos + 2;
  return true;
}

bool ParseNumber(std::string_view text, long long* value) {
  if (text.empty()) {
    return false;
  }
  long long result = 0;
  auto begin = text.data();
  auto end = text.data() + text.size();
  auto [ptr, ec] = std::from_chars(begin, end, result);
  if (ec != std::errc{} || ptr != end) {
    return false;
  }
  *value = result;
  return true;
}
}  // namespace

RESPCommand ParseRESP(std::string_view frame) {
  RESPCommand cmd;
  constexpr long long kMaxArrayElements = 1024;
  constexpr long long kMaxBulkLength = 1024 * 1024;
  size_t offset = 0;
  std::string_view line;
  if (!ReadLine(frame, &offset, &line)) {
    return cmd;
  }
  if (line.empty() || line.front() != '*') {
    return cmd;
  }

  long long count = 0;
  if (!ParseNumber(line.substr(1), &count) || count <= 0 ||
      count > kMaxArrayElements) {
    return cmd;
  }

  std::vector<std::string> parts;
  parts.reserve(static_cast<size_t>(count));

  for (long long i = 0; i < count; ++i) {
    if (!ReadLine(frame, &offset, &line)) {
      return RESPCommand{};
    }
    if (line.empty() || line.front() != '$') {
      return RESPCommand{};
    }
    long long length = 0;
    if (!ParseNumber(line.substr(1), &length) || length < 0 ||
        length > kMaxBulkLength) {
      return RESPCommand{};
    }
    if (length > static_cast<long long>(std::numeric_limits<size_t>::max())) {
      return RESPCommand{};
    }
    auto remaining = frame.size() - offset;
    auto length_size = static_cast<size_t>(length);
    if (remaining < 2 || length_size > remaining - 2) {
      return RESPCommand{};
    }
    parts.emplace_back(frame.substr(offset, length_size));
    offset += length_size;
    if (frame.substr(offset, 2) != "\r\n") {
      return RESPCommand{};
    }
    offset += 2;
  }

  if (!parts.empty()) {
    cmd.name = parts.front();
    if (parts.size() > 1) {
      cmd.args.assign(parts.begin() + 1, parts.end());
    }
  }
  return cmd;
}
}  // namespace cache::protocol::resp
