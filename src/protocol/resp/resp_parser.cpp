#include "protocol/resp/resp_parser.hpp"

#include <cctype>

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
  bool negative = text.front() == '-';
  size_t start = negative ? 1 : 0;
  if (start == text.size()) {
    return false;
  }
  long long result = 0;
  for (size_t i = start; i < text.size(); ++i) {
    unsigned char ch = static_cast<unsigned char>(text[i]);
    if (!std::isdigit(ch)) {
      return false;
    }
    result = result * 10 + (text[i] - '0');
  }
  *value = negative ? -result : result;
  return true;
}
}  // namespace

RESPCommand ParseRESP(std::string_view frame) {
  RESPCommand cmd;
  size_t offset = 0;
  std::string_view line;
  if (!ReadLine(frame, &offset, &line)) {
    return cmd;
  }
  if (line.empty() || line.front() != '*') {
    return cmd;
  }

  long long count = 0;
  if (!ParseNumber(line.substr(1), &count) || count <= 0) {
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
    if (!ParseNumber(line.substr(1), &length) || length < 0) {
      return RESPCommand{};
    }
    auto remaining = frame.size() - offset;
    if (remaining < static_cast<size_t>(length + 2)) {
      return RESPCommand{};
    }
    parts.emplace_back(frame.substr(offset, static_cast<size_t>(length)));
    offset += static_cast<size_t>(length);
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
