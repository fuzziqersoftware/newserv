#pragma once

#include <stdint.h>

#include <functional>
#include <memory>
#include <phosg/Strings.hh>
#include <string>
#include <unordered_map>
#include <vector>

struct StepGraph {
  struct Step {
    std::vector<std::shared_ptr<Step>> downstream_dependencies;
    std::vector<std::weak_ptr<Step>> upstream_dependencies;
    std::function<void()> execute;
    uint64_t last_run_id = 0;
  };

  std::unordered_map<std::string, std::shared_ptr<Step>> steps;
  uint64_t last_run_id = 0;

  StepGraph() = default;

  void add_step(const std::string& name, const std::vector<std::string>& depends_on_names, std::function<void()>&& execute);
  void run(const std::string& start_step, bool run_upstreams);
  void run(const std::vector<std::string>& start_steps, bool run_upstreams);
};
