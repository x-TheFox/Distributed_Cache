#include "metrics/prometheus_endpoint.hpp"

#include <sstream>

namespace cache::metrics {
void PrometheusEndpoint::SetGauge(const std::string& name, double value,
                                  const std::string& help) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& metric = metrics_[name];
  metric.name = name;
  metric.type = "gauge";
  if (!help.empty()) {
    metric.help = help;
  }
  metric.value = value;
}

void PrometheusEndpoint::IncrementCounter(const std::string& name,
                                          double amount,
                                          const std::string& help) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& metric = metrics_[name];
  metric.name = name;
  metric.type = "counter";
  if (!help.empty()) {
    metric.help = help;
  }
  metric.value += amount;
}

std::string PrometheusEndpoint::RenderPrometheusMetrics() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::ostringstream out;
  for (const auto& [name, metric] : metrics_) {
    if (!metric.help.empty()) {
      out << "# HELP " << metric.name << ' ' << metric.help << '\n';
    }
    out << "# TYPE " << metric.name << ' ' << metric.type << '\n';
    out << metric.name << ' ' << metric.value << '\n';
  }
  return out.str();
}
}  // namespace cache::metrics
