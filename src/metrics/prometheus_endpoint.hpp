#pragma once

#include <map>
#include <mutex>
#include <string>

namespace cache::metrics {
class PrometheusEndpoint {
 public:
  void SetGauge(const std::string& name, double value,
                const std::string& help = "");
  void IncrementCounter(const std::string& name, double amount = 1.0,
                        const std::string& help = "");
  std::string RenderPrometheusMetrics() const;

 private:
  struct Metric {
    std::string name;
    std::string help;
    std::string type;
    double value{0.0};
  };

  mutable std::mutex mutex_;
  std::map<std::string, Metric> metrics_;
};
}  // namespace cache::metrics
