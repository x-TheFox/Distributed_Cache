#include "protocol/grpc/cache_service.hpp"

#include "server/command_dispatcher.hpp"

namespace cache::protocol::grpc {
CacheServiceImpl::CacheServiceImpl(cache::core::ConcurrentStore& store)
    : store_(store) {}

::grpc::Status CacheServiceImpl::Set(::grpc::ServerContext*,
                                     const cache::SetRequest* request,
                                     cache::SetResponse* response) {
  auto result = cache::server::DispatchCommand(
      "SET", {request->key(), request->value()}, store_);
  if (result.status == cache::server::CommandStatus::kInvalidKey) {
    response->set_ok(false);
    response->set_error(result.message);
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, result.message);
  }
  if (result.status == cache::server::CommandStatus::kOk) {
    response->set_ok(true);
    return ::grpc::Status::OK;
  }
  response->set_ok(false);
  response->set_error(result.message);
  return ::grpc::Status::OK;
}

::grpc::Status CacheServiceImpl::Get(::grpc::ServerContext*,
                                     const cache::GetRequest* request,
                                     cache::GetResponse* response) {
  auto result = cache::server::DispatchCommand("GET", {request->key()}, store_);
  if (result.status == cache::server::CommandStatus::kInvalidKey) {
    response->set_found(false);
    response->set_error(result.message);
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, result.message);
  }
  if (result.status == cache::server::CommandStatus::kOk &&
      result.value.has_value()) {
    response->set_found(true);
    response->set_value(*result.value);
    return ::grpc::Status::OK;
  }
  if (result.status == cache::server::CommandStatus::kNotFound) {
    response->set_found(false);
    return ::grpc::Status::OK;
  }
  response->set_found(false);
  response->set_error(result.message);
  return ::grpc::Status::OK;
}

}  // namespace cache::protocol::grpc
