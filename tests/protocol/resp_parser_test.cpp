#include <gtest/gtest.h>

#include "protocol/resp/resp_parser.hpp"

TEST(RESPParser, ParsesSetCommandArray) {
  auto cmd = cache::protocol::resp::ParseRESP(
      "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
  EXPECT_EQ(cmd.name, "SET");
  ASSERT_GE(cmd.args.size(), 1u);
  EXPECT_EQ(cmd.args[0], "foo");
}
