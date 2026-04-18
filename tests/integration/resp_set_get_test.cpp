#include <gtest/gtest.h>

#include "core/concurrent_store.hpp"
#include "protocol/resp/resp_parser.hpp"

TEST(RESPSetGet, RoundTrip) {
  cache::core::ConcurrentStore store(4);
  auto set_cmd = cache::protocol::resp::ParseRESP(
      "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
  auto set_response = cache::protocol::resp::ExecuteCommand(set_cmd, store);
  EXPECT_EQ(set_response, "+OK\r\n");

  auto get_cmd = cache::protocol::resp::ParseRESP(
      "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n");
  auto get_response = cache::protocol::resp::ExecuteCommand(get_cmd, store);
  EXPECT_EQ(get_response, "$3\r\nbar\r\n");
}

TEST(RESPSetGet, RejectsWrongArity) {
  cache::core::ConcurrentStore store(4);
  auto set_cmd = cache::protocol::resp::ParseRESP(
      "*4\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n$3\r\nbaz\r\n");
  auto set_response = cache::protocol::resp::ExecuteCommand(set_cmd, store);
  EXPECT_EQ(set_response, "-ERR wrong number of arguments\r\n");

  auto get_cmd = cache::protocol::resp::ParseRESP(
      "*3\r\n$3\r\nGET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
  auto get_response = cache::protocol::resp::ExecuteCommand(get_cmd, store);
  EXPECT_EQ(get_response, "-ERR wrong number of arguments\r\n");
}

TEST(RESPSetGet, RejectsEmptyKey) {
  cache::core::ConcurrentStore store(4);
  auto set_cmd = cache::protocol::resp::ParseRESP(
      "*3\r\n$3\r\nSET\r\n$0\r\n\r\n$3\r\nbar\r\n");
  auto set_response = cache::protocol::resp::ExecuteCommand(set_cmd, store);
  EXPECT_EQ(set_response, "-ERR key must not be empty\r\n");

  auto get_cmd = cache::protocol::resp::ParseRESP(
      "*2\r\n$3\r\nGET\r\n$0\r\n\r\n");
  auto get_response = cache::protocol::resp::ExecuteCommand(get_cmd, store);
  EXPECT_EQ(get_response, "-ERR key must not be empty\r\n");
}
