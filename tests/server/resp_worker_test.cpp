#include <gtest/gtest.h>

#include <atomic>
#include <cerrno>
#include <functional>
#include <system_error>

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "server/resp_worker.hpp"

TEST(RespWorker, ReleasesSlotAndClosesOnThreadFailure) {
  int fds[2] = {-1, -1};
  ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

  std::atomic<int> active{1};
  bool handled = false;
  auto handler = [&](int) { handled = true; };
  auto launcher = [&](std::function<void()>) {
    throw std::system_error(
        std::make_error_code(std::errc::resource_unavailable_try_again));
  };

  const bool started =
      cache::server::StartRespWorker(fds[0], active, handler, launcher);

  EXPECT_FALSE(started);
  EXPECT_FALSE(handled);
  EXPECT_EQ(active.load(), 0);

  errno = 0;
  EXPECT_EQ(::fcntl(fds[0], F_GETFD), -1);
  EXPECT_EQ(errno, EBADF);

  ::close(fds[1]);
}
