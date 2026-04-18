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

RESPParseResult ParseRESP(std::string_view frame) {
  RESPParseResult result;
  RESPCommand cmd;
  constexpr long long kMaxArrayElements = 1024;
  constexpr long long kMaxBulkLength = 1024 * 1024;
  size_t offset = 0;
  std::string_view line;
  if (!ReadLine(frame, &offset, &line)) {
    return result;
  }
  if (line.empty() || line.front() != '*') {
    return result;
  }

  long long count = 0;
  if (!ParseNumber(line.substr(1), &count) || count <= 0 ||
      count > kMaxArrayElements) {
    return result;
  }

  std::vector<std::string> parts;
  parts.reserve(static_cast<size_t>(count));

  for (long long i = 0; i < count; ++i) {
    if (!ReadLine(frame, &offset, &line)) {
      return result;
    }
    if (line.empty() || line.front() != '$') {
      return result;
    }
    long long length = 0;
    if (!ParseNumber(line.substr(1), &length) || length < 0 ||
        length > kMaxBulkLength) {
      return result;
    }
    if (static_cast<unsigned long long>(length) >
        std::numeric_limits<size_t>::max()) {
      return result;
    }
    auto remaining = frame.size() - offset;
    auto length_size = static_cast<size_t>(length);
    if (remaining < 2 || length_size > remaining - 2) {
      return result;
    }
    parts.emplace_back(frame.substr(offset, length_size));
    offset += length_size;
    if (frame.substr(offset, 2) != "\r\n") {
      return result;
    }
    offset += 2;
  }

  if (!parts.empty()) {
    cmd.name = parts.front();
    if (parts.size() > 1) {
      cmd.args.assign(parts.begin() + 1, parts.end());
    }
  }
  result.command = std::move(cmd);
  result.bytes_consumed = offset;
  return result;
}
}  // namespace cache::protocol::resp
