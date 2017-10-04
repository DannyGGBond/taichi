/*******************************************************************************
    Taichi - Physically based Computer Graphics Library

    Copyright (c) 2017 Yuanming Hu <yuanmhu@gmail.com>

    All rights reserved. Use of this source code is governed by
    the MIT license as written in the LICENSE file.
*******************************************************************************/

#pragma once

#include <taichi/common/util.h>
#include <taichi/system/timer.h>
#include <vector>
#include <map>
#include <memory>

TC_NAMESPACE_BEGIN

class ProfilerRecords {
 public:
  struct Node {
    std::vector<std::unique_ptr<Node>> childs;
    Node *parent;
    std::string name;
    float64 total_time;
    int64 num_samples;

    Node(const std::string &name, Node *parent) {
      this->name = name;
      this->parent = parent;
      this->total_time = 0.0_f64;
      this->num_samples = 1LL;
    }

    void insert_sample(float64 sample) {
      num_samples += 1;
      total_time += sample;
    }

    float64 get_averaged() const {
      return total_time / (float64)std::max(num_samples, 1LL);
    }

    Node *get_child(const std::string &name) {
      for (auto &ch : childs) {
        if (ch->name == name) {
          return ch.get();
        }
      }
      childs.push_back(std::make_unique<Node>(name, this));
      return childs.back().get();
    }
  };

  std::unique_ptr<Node> root;
  Node *current_node;
  int current_depth;

  ProfilerRecords() {
    root = std::make_unique<Node>("[Taichi Profiler]", nullptr);
    current_node = root.get();
    current_depth = 0;  // depth(root) = 0
  }

  void print(Node *node, int depth) {
    auto make_indent = [depth](int additional) {
      for (int i = 0; i < depth + additional; i++) {
        printf("  ");
      }
    };
    float64 total_time = node->get_averaged();
    if (depth == 0) {
      // Root node only
      make_indent(0);
      printf("%s\n", node->name.c_str());
    }
    if (total_time < 1e-6f) {
      for (auto &ch : node->childs) {
        make_indent(1);
        auto child_time = ch->get_averaged();
        printf("%6.2f %s\n", child_time, ch->name.c_str());
        print(ch.get(), depth + 1);
      }
    } else {
      std::string unit = "s";
      real scale = 1;
      if (total_time < 0.1) {
        // Use ms
        unit = "ms";
        scale = 1000;
      }
      float64 unaccounted = total_time;
      for (auto &ch : node->childs) {
        make_indent(1);
        auto child_time = ch->get_averaged();
        printf("%6.2f%s %4.1f%%  %s\n", child_time * scale, unit.c_str(),
               child_time * 100.0 / total_time, ch->name.c_str());
        print(ch.get(), depth + 1);
        unaccounted -= child_time;
      }
      if (!node->childs.empty() && (unaccounted > total_time * 0.05)) {
        make_indent(1);
        printf("%6.2f%s %4.1f%%  %s\n", unaccounted * scale, unit.c_str(),
               unaccounted * 100.0 / total_time, "[unaccounted]");
      }
    }
  }

  void print() { print(root.get(), 0); }

  void insert_sample(float64 time) { current_node->insert_sample(time); }

  void push(const std::string name) {
    current_node = current_node->get_child(name);
    current_depth += 1;
  }

  void pop() {
    current_node = current_node->parent;
    current_depth -= 1;
  }

  static ProfilerRecords &get_instance() {
    static ProfilerRecords profiler_records;
    return profiler_records;
  }
};

class Profiler {
 public:
  float64 start_time;
  std::string name;
  bool stopped;

  Profiler(std::string name) {
    start_time = Time::get_time();
    this->name = name;
    stopped = false;
    ProfilerRecords::get_instance().push(name);
  }

  void stop() {
    assert_info(!stopped, "Profiler already stopped.");
    float64 elapsed = Time::get_time() - start_time;
    ProfilerRecords::get_instance().insert_sample(elapsed);
    ProfilerRecords::get_instance().pop();
  }

  ~Profiler() {
    if (!stopped) {
      stop();
    }
  }
};

#define TC_PROFILE(name, statements) \
  {                                  \
    Profiler _(name);                \
    statements;                      \
  }

TC_NAMESPACE_END
