#pragma once

#include <grpcpp/grpcpp.h>

#include "cache.grpc.pb.h"
#include "core/concurrent_store.hpp"

namespace cache::protocol::grpc {

class CacheServiceImpl final : public cache::CacheService::Service {
 public:
  explicit CacheServiceImpl(cache::core::ConcurrentStore& store);

  ::grpc::Status Set(::grpc::ServerContext* context,
                     const cache::SetRequest* request,
                     cache::SetResponse* response) override;
  ::grpc::Status Get(::grpc::ServerContext* context,
                     const cache::GetRequest* request,
                     cache::GetResponse* response) override;

 private:
  cache::core::ConcurrentStore& store_;
};

}  // namespace cache::protocol::grpc
