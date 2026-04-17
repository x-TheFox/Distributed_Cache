#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>

#include "protocol/grpc/cache_service.hpp"

TEST(GRPCCacheService, SetAndGetRoundTrip) {
  cache::core::ConcurrentStore store(4);
  cache::protocol::grpc::CacheServiceImpl service(store);

  grpc::ServerBuilder builder;
  int port = 0;
  builder.AddListeningPort("localhost:0", grpc::InsecureServerCredentials(),
                           &port);
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  ASSERT_NE(port, 0);

  auto channel = grpc::CreateChannel("localhost:" + std::to_string(port),
                                     grpc::InsecureChannelCredentials());
  auto stub = cache::CacheService::NewStub(channel);

  cache::SetRequest set_request;
  set_request.set_key("foo");
  set_request.set_value("bar");
  cache::SetResponse set_response;
  grpc::ClientContext set_context;
  auto set_status = stub->Set(&set_context, set_request, &set_response);
  ASSERT_TRUE(set_status.ok());
  EXPECT_TRUE(set_response.ok());

  cache::GetRequest get_request;
  get_request.set_key("foo");
  cache::GetResponse get_response;
  grpc::ClientContext get_context;
  auto get_status = stub->Get(&get_context, get_request, &get_response);
  ASSERT_TRUE(get_status.ok());
  EXPECT_TRUE(get_response.found());
  EXPECT_EQ(get_response.value(), "bar");

  server->Shutdown();
}
