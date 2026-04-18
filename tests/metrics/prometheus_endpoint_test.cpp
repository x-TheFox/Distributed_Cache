#include <gtest/gtest.h>

#include "metrics/prometheus_endpoint.hpp"

namespace cache::metrics {
TEST(PrometheusEndpoint, RendersMetricsForScrape) {
  PrometheusEndpoint endpoint;
  endpoint.SetGauge("cache_entries", 42, "Number of entries in cache");
  endpoint.IncrementCounter("cache_hits_total", 2, "Cache hits");
  endpoint.IncrementCounter("cache_hits_total", 3);

  auto rendered = endpoint.RenderPrometheusMetrics();
  EXPECT_NE(rendered.find("# HELP cache_entries Number of entries in cache"),
            std::string::npos);
  EXPECT_NE(rendered.find("# TYPE cache_entries gauge"), std::string::npos);
  EXPECT_NE(rendered.find("cache_entries 42"), std::string::npos);
  EXPECT_NE(rendered.find("cache_hits_total 5"), std::string::npos);
}
}  // namespace cache::metrics
