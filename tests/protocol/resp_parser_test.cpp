#include <gtest/gtest.h>

#include "protocol/resp/resp_parser.hpp"

TEST(RESPParser, ParsesSetCommandArray) {
  auto cmd = cache::protocol::resp::ParseRESP(
      "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
  EXPECT_EQ(cmd.name, "SET");
  ASSERT_GE(cmd.args.size(), 1u);
  EXPECT_EQ(cmd.args[0], "foo");
}

TEST(RESPParser, RejectsOversizedArrayCount) {
  auto cmd = cache::protocol::resp::ParseRESP("*1025\r\n");
  EXPECT_TRUE(cmd.name.empty());
  EXPECT_TRUE(cmd.args.empty());
}

TEST(RESPParser, RejectsOversizedBulkLength) {
  auto cmd = cache::protocol::resp::ParseRESP("*1\r\n$1048577\r\n");
  EXPECT_TRUE(cmd.name.empty());
  EXPECT_TRUE(cmd.args.empty());
}

TEST(RESPParser, RejectsOverflowingArrayCount) {
  auto cmd = cache::protocol::resp::ParseRESP("*999999999999999999999999\r\n");
  EXPECT_TRUE(cmd.name.empty());
  EXPECT_TRUE(cmd.args.empty());
}

TEST(RESPParser, RejectsOverflowingBulkLength) {
  auto cmd =
      cache::protocol::resp::ParseRESP("*1\r\n$99999999999999999999999\r\n");
  EXPECT_TRUE(cmd.name.empty());
  EXPECT_TRUE(cmd.args.empty());
}

TEST(RESPParser, RejectsTruncatedBulkString) {
  auto cmd = cache::protocol::resp::ParseRESP("*1\r\n$3\r\nGE");
  EXPECT_TRUE(cmd.name.empty());
  EXPECT_TRUE(cmd.args.empty());
}

TEST(RESPParser, RejectsBadBulkCRLF) {
  auto cmd = cache::protocol::resp::ParseRESP("*1\r\n$3\r\nGET!!");
  EXPECT_TRUE(cmd.name.empty());
  EXPECT_TRUE(cmd.args.empty());
}
