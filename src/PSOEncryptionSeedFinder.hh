#pragma once

#include <inttypes.h>
#include <stddef.h>

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <utility>
#include <vector>



class PSOEncryptionSeedFinder {
public:
  PSOEncryptionSeedFinder(
      const std::string& ciphertext,
      const std::vector<std::pair<std::string, std::string>>& plaintexts,
      size_t num_threads);
  ~PSOEncryptionSeedFinder() = default;

  enum Flag {
    V3                 = 0x01,
    SKIP_LITTLE_ENDIAN = 0x02,
    SKIP_BIG_ENDIAN    = 0x04,
  };

  struct Result {
    uint32_t seed;
    size_t differences;
    bool is_indeterminate;
    bool is_big_endian;
    bool is_v3;

    Result(uint32_t seed, size_t differences);
    Result(uint32_t seed, size_t differences, bool is_big_endian, bool is_v3);
  };

  struct ThreadResults {
    std::vector<Result> results;
    size_t min_differences;
    std::vector<size_t> difference_histogram;

    void add_result(const Result& res);
    void combine_from(const ThreadResults& other);
  };

  ThreadResults find_seed(std::function<bool(uint32_t, size_t)> fn);

  ThreadResults find_seed(uint64_t flags);
  ThreadResults find_seed(const std::string& rainbow_table_filename);

  static void generate_rainbow_table(
      const std::string& filename,
      bool is_v3,
      bool is_big_endian,
      size_t match_length,
      size_t num_threads);

  static void parallel_all_seeds(
      size_t num_threads, std::function<bool(uint32_t, size_t)> fn);

private:
  template <typename... ThreadArgTs>
  static void parallel_all_seeds_t(size_t num_threads, ThreadArgTs... args);
  template <typename... ThreadArgTs>
  ThreadResults parallel_find_seed_t(ThreadArgTs... args);


  static void parallel_all_seeds_thread_fn(
      std::function<bool(uint32_t, size_t)> fn,
      std::atomic<uint64_t>& current_seed,
      size_t thread_num);

  void find_seed_without_rainbow_table_thread_fn(
      uint64_t flags,
      std::vector<ThreadResults>& all_results,
      std::atomic<uint64_t>& current_seed,
      size_t thread_num);
  void find_seed_with_rainbow_table_thread_fn(
      int fd,
      size_t page_size,
      std::vector<ThreadResults>& all_results,
      std::atomic<uint64_t>& current_seed,
      size_t thread_num);

  static void generate_rainbow_table_thread_fn(
      int fd,
      size_t match_length,
      size_t page_size,
      bool is_v3,
      bool is_big_endian,
      std::atomic<uint64_t>& current_seed,
      size_t thread_num);

  std::string ciphertext;
  std::vector<std::pair<std::string, std::string>> plaintexts;
  size_t num_threads;
};
