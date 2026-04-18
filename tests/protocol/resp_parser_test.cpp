#include <gtest/gtest.h>

#include "protocol/resp/resp_parser.hpp"

TEST(RESPParser, ParsesSetCommandArray) {
  auto result = cache::protocol::resp::ParseRESP(
      "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
  EXPECT_EQ(result.command.name, "SET");
  EXPECT_EQ(result.bytes_consumed,
            std::string("*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n")
                .size());
  ASSERT_GE(result.command.args.size(), 1u);
  EXPECT_EQ(result.command.args[0], "foo");
}

TEST(RESPParser, RejectsOversizedArrayCount) {
  auto result = cache::protocol::resp::ParseRESP("*1025\r\n");
  EXPECT_TRUE(result.command.name.empty());
  EXPECT_TRUE(result.command.args.empty());
  EXPECT_EQ(result.bytes_consumed, 0u);
}

TEST(RESPParser, RejectsOversizedBulkLength) {
  auto result = cache::protocol::resp::ParseRESP("*1\r\n$1048577\r\n");
  EXPECT_TRUE(result.command.name.empty());
  EXPECT_TRUE(result.command.args.empty());
  EXPECT_EQ(result.bytes_consumed, 0u);
}

TEST(RESPParser, RejectsOverflowingArrayCount) {
  auto result =
      cache::protocol::resp::ParseRESP("*999999999999999999999999\r\n");
  EXPECT_TRUE(result.command.name.empty());
  EXPECT_TRUE(result.command.args.empty());
  EXPECT_EQ(result.bytes_consumed, 0u);
}

TEST(RESPParser, RejectsOverflowingBulkLength) {
  auto result =
      cache::protocol::resp::ParseRESP("*1\r\n$99999999999999999999999\r\n");
  EXPECT_TRUE(result.command.name.empty());
  EXPECT_TRUE(result.command.args.empty());
  EXPECT_EQ(result.bytes_consumed, 0u);
}

TEST(RESPParser, RejectsTruncatedBulkString) {
  auto result = cache::protocol::resp::ParseRESP("*1\r\n$3\r\nGE");
  EXPECT_TRUE(result.command.name.empty());
  EXPECT_TRUE(result.command.args.empty());
  EXPECT_EQ(result.bytes_consumed, 0u);
}

TEST(RESPParser, RejectsBadBulkCRLF) {
  auto result = cache::protocol::resp::ParseRESP("*1\r\n$3\r\nGET!!");
  EXPECT_TRUE(result.command.name.empty());
  EXPECT_TRUE(result.command.args.empty());
  EXPECT_EQ(result.bytes_consumed, 0u);
}

TEST(RESPParser, ParsesMultipleFramesSequentially) {
  const std::string payload =
      "*1\r\n$4\r\nPING\r\n*1\r\n$4\r\nPING\r\n";
  auto first = cache::protocol::resp::ParseRESP(payload);
  ASSERT_EQ(first.command.name, "PING");
  ASSERT_GT(first.bytes_consumed, 0u);
  auto second = cache::protocol::resp::ParseRESP(
      std::string_view(payload).substr(first.bytes_consumed));
  EXPECT_EQ(second.command.name, "PING");
  EXPECT_EQ(second.bytes_consumed,
            payload.size() - first.bytes_consumed);
}
