#include "cluster/hash_ring.hpp"

#include <string>

namespace cache::cluster {
namespace {
constexpr uint64_t kFnvOffset = 14695981039346656037ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

uint64_t Fnv1a(std::string_view value) {
  uint64_t hash = kFnvOffset;
  for (unsigned char ch : value) {
    hash ^= static_cast<uint64_t>(ch);
    hash *= kFnvPrime;
  }
  return hash;
}
}  // namespace

uint64_t HashRing::Hash64(std::string_view value) {
  return Fnv1a(value);
}

uint64_t HashRing::TokenFor(const std::string& node_id, int vnode,
                            int attempt) const {
  std::string token = node_id + "#" + std::to_string(vnode);
  if (attempt > 0) {
    token += "#" + std::to_string(attempt);
  }
  return Hash64(token);
}

void HashRing::AddNode(std::string node_id, int virtual_nodes) {
  if (virtual_nodes <= 0) {
    return;
  }

  RemoveNode(node_id);

  auto& tokens = node_tokens_[node_id];
  tokens.reserve(static_cast<size_t>(virtual_nodes));
  for (int i = 0; i < virtual_nodes; ++i) {
    int attempt = 0;
    uint64_t token = TokenFor(node_id, i, attempt);
    while (ring_.find(token) != ring_.end()) {
      ++attempt;
      token = TokenFor(node_id, i, attempt);
    }
    ring_.emplace(token, node_id);
    tokens.push_back(token);
  }
}

void HashRing::RemoveNode(const std::string& node_id) {
  auto it = node_tokens_.find(node_id);
  if (it == node_tokens_.end()) {
    return;
  }

  for (auto token : it->second) {
    auto ring_it = ring_.find(token);
    if (ring_it != ring_.end() && ring_it->second == node_id) {
      ring_.erase(ring_it);
    }
  }
  node_tokens_.erase(it);
}

std::string HashRing::OwnerForKey(const std::string& key) const {
  if (ring_.empty()) {
    return {};
  }

  auto hash = Hash64(key);
  auto it = ring_.lower_bound(hash);
  if (it == ring_.end()) {
    it = ring_.begin();
  }
  return it->second;
}
}  // namespace cache::cluster
