#include "StepGraph.hh"

using namespace std;

void StepGraph::add_step(const string& name, const vector<string>& depends_on_names, function<void()>&& execute) {
  auto new_step = make_shared<Step>();
  new_step->execute = std::move(execute);
  this->steps.emplace(name, new_step);

  for (const auto& depends_on_name : depends_on_names) {
    auto upstream_step = this->steps.at(depends_on_name);
    upstream_step->downstream_dependencies.emplace_back(new_step);
    new_step->upstream_dependencies.emplace_back(upstream_step);
  }
}

void StepGraph::run(const string& start_step_name, bool run_upstreams) {
  vector<string> start_step_names({start_step_name});
  this->run(start_step_names, run_upstreams);
}

void StepGraph::run(const vector<string>& start_step_names, bool run_upstreams) {
  // Collect all steps to run
  deque<shared_ptr<Step>> steps_to_visit;
  try {
    for (const auto& start_step_name : start_step_names) {
      steps_to_visit.emplace_back(this->steps.at(start_step_name));
    }
  } catch (const out_of_range&) {
    throw runtime_error("invalid step name");
  }
  unordered_set<shared_ptr<Step>> steps_to_run;
  while (!steps_to_visit.empty()) {
    auto step = std::move(steps_to_visit.front());
    steps_to_visit.pop_front();
    if (steps_to_run.emplace(step).second) {
      if (run_upstreams) {
        for (const auto& w_other_step : step->upstream_dependencies) {
          auto other_step = w_other_step.lock();
          if (!other_step) {
            throw runtime_error("upstream step is deleted");
          }
          steps_to_visit.emplace_back(other_step);
        }
      } else {
        for (const auto& other_step : step->downstream_dependencies) {
          steps_to_visit.emplace_back(other_step);
        }
      }
    }
  }

  // Topological sort: repeatedly take all steps that are not a downstream
  // dependency of any other step in the set
  vector<shared_ptr<Step>> steps_order;
  steps_order.reserve(steps_to_run.size());
  while (!steps_to_run.empty()) {
    unordered_set<shared_ptr<Step>> candidate_steps = steps_to_run;
    for (const auto& step : steps_to_run) {
      for (const auto& downstream_step : step->downstream_dependencies) {
        candidate_steps.erase(downstream_step);
      }
    }
    if (candidate_steps.empty()) {
      throw logic_error("dependency graph contains a cycle");
    }
    for (const auto& step : candidate_steps) {
      steps_to_run.erase(step);
      steps_order.emplace_back(step);
    }
  }

  // Run the steps in order
  uint64_t run_id = ++this->last_run_id;
  for (auto step : steps_order) {
    if (step->last_run_id < run_id) {
      step->last_run_id = run_id;
      if (step->execute) {
        step->execute();
      }
    }
  }
}
