#include "PSOEncryptionSeedFinder.hh"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <phosg/Time.hh>

#include "PSOEncryption.hh"

using namespace std;



static size_t difference_match(const string& data1, const string& data2) {
  if (data1.size() != data2.size()) {
    return max<size_t>(data1.size(), data2.size());
  }
  size_t differences = 0;
  for (size_t z = 0; z < data1.size(); z++) {
    if (data1[z] != data2[z]) {
      differences++;
    }
  }
  return differences;
}



PSOEncryptionSeedFinder::PSOEncryptionSeedFinder(
    const std::string& ciphertext,
    const std::vector<std::pair<std::string, std::string>>& plaintexts,
    size_t num_threads)
  : ciphertext(ciphertext), plaintexts(plaintexts), num_threads(num_threads) {
  if (num_threads == 0) {
    throw logic_error("must use at least one thread");
  }
  if (this->ciphertext.empty() || (this->ciphertext.size() & 3)) {
    throw runtime_error("ciphertext length must be a nonzero multiple of 4");
  }
  if (this->plaintexts.empty()) {
    throw runtime_error("no plaintexts provided");
  }
  size_t plaintext_size = this->plaintexts[0].first.size();
  for (const auto& plaintext : this->plaintexts) {
    if (plaintext.first.size() != plaintext_size) {
      throw runtime_error("plaintexts are not all the same size");
    }
    if (plaintext.first.size() != plaintext.second.size()) {
      throw logic_error("plaintext and plaintext mask are not the same size");
    }
  }
}



PSOEncryptionSeedFinder::Result::Result(uint32_t seed, size_t differences)
  : seed(seed),
    differences(differences),
    is_indeterminate(true),
    is_big_endian(false),
    is_v3(false) { }
PSOEncryptionSeedFinder::Result::Result(
    uint32_t seed, size_t differences, bool is_big_endian, bool is_v3)
  : seed(seed),
    differences(differences),
    is_indeterminate(false),
    is_big_endian(is_big_endian),
    is_v3(is_v3) { }



void PSOEncryptionSeedFinder::ThreadResults::add_result(const Result& res) {
  if (res.differences < this->min_differences) {
    this->results.clear();
    this->min_differences = res.differences;
  }
  if ((res.differences == this->min_differences) && (this->results.size() < 10)) {
    this->results.emplace_back(res);
  }
  if (this->difference_histogram.size() <= res.differences) {
    this->difference_histogram.resize(res.differences + 1, 0);
  }
  this->difference_histogram[res.differences]++;
}

void PSOEncryptionSeedFinder::ThreadResults::combine_from(
    const ThreadResults& other) {
  if (this->min_differences > other.min_differences) {
    this->min_differences = other.min_differences;
    this->results = other.results;
  } else if (this->min_differences == other.min_differences) {
    this->results.insert(this->results.end(), other.results.begin(), other.results.end());
  }
  if (this->difference_histogram.size() < other.difference_histogram.size()) {
    this->difference_histogram.resize(other.difference_histogram.size(), 0);
  }
  for (size_t z = 0; z < other.difference_histogram.size(); z++) {
    this->difference_histogram[z] += other.difference_histogram[z];
  }
}



PSOEncryptionSeedFinder::ThreadResults PSOEncryptionSeedFinder::find_seed(
    uint64_t flags) {
  // TODO: Use a specific logger here
  log_info("Searching for decryption key (%s, %zu threads)",
      (flags & Flag::V3) ? "v3" : "v2", this->num_threads);
  return this->parallel_find_seed_t(
      &PSOEncryptionSeedFinder::find_seed_without_rainbow_table_thread_fn,
      this,
      flags);
}

PSOEncryptionSeedFinder::ThreadResults PSOEncryptionSeedFinder::find_seed(
    const string& rainbow_table_filename) {
  size_t plaintext_size = this->plaintexts[0].first.size();

  scoped_fd fd(rainbow_table_filename, O_RDONLY);
  int64_t expected_rainbow_table_size = static_cast<int64_t>(plaintext_size) << 32;
  if (fstat(fd).st_size != expected_rainbow_table_size) {
    throw runtime_error("rainbow table size is incorrect");
  }

  // TODO: Use a specific logger here
  log_info("Searching for decryption key (%zu threads) using rainbow table %s",
      this->num_threads, rainbow_table_filename.c_str());
  return this->parallel_find_seed_t(
      &PSOEncryptionSeedFinder::find_seed_with_rainbow_table_thread_fn,
      this,
      static_cast<int>(fd),
      0x1000);
}

void PSOEncryptionSeedFinder::generate_rainbow_table(
    const std::string& filename,
    bool is_v3,
    bool is_big_endian,
    size_t match_length,
    size_t num_threads) {
  if ((match_length == 0) || (match_length & 3)) {
    throw runtime_error("match length must be a nonzero multiple of 4");
  }
  if (num_threads == 0) {
    throw logic_error("must use at least one thread");
  }

  uint64_t file_size = static_cast<uint64_t>(match_length) << 32;
  string file_size_str = format_size(file_size);

  scoped_fd fd(filename, O_CREAT | O_WRONLY);
  log_info("Allocating file space for rainbow table (match_length=%zu bytes => table size is %s)",
      match_length, file_size_str.c_str());
  if (ftruncate(fd, file_size) < 0) {
    throw runtime_error("cannot allocate file space for table");
  }

  size_t page_size = 0x1000;

  PSOEncryptionSeedFinder::parallel_all_seeds_t(
      num_threads,
      &PSOEncryptionSeedFinder::generate_rainbow_table_thread_fn,
      static_cast<int>(fd),
      match_length,
      page_size,
      is_v3,
      is_big_endian);

  log_info("Wrote %s to rainbow table %s\n", file_size_str.c_str(), filename.c_str());
}

void PSOEncryptionSeedFinder::parallel_all_seeds(
    size_t num_threads, function<bool(uint32_t, size_t)> fn) {
  PSOEncryptionSeedFinder::parallel_all_seeds_t(
      num_threads,
      &PSOEncryptionSeedFinder::parallel_all_seeds_thread_fn,
      fn);
}



// TODO: Use phosg's parallel_range for this instead
template <typename... ThreadArgTs>
void PSOEncryptionSeedFinder::parallel_all_seeds_t(
    size_t num_threads, ThreadArgTs... args) {
  atomic<uint64_t> current_seed(0);
  vector<thread> threads;
  while (threads.size() < num_threads) {
    threads.emplace_back(args..., ref(current_seed), threads.size());
  }

  uint64_t start_time = now();
  uint64_t displayed_current_seed;
  while ((displayed_current_seed = current_seed.load()) < 0x100000000) {

    uint64_t elapsed_time = now() - start_time;
    string elapsed_str = format_duration(elapsed_time);

    string remaining_str;
    if (displayed_current_seed) {
      uint64_t total_time = (elapsed_time << 32) / displayed_current_seed;
      uint64_t remaining_time = total_time - elapsed_time;
      remaining_str = format_duration(remaining_time);
    } else {
      remaining_str = "...";
    }

    fprintf(stderr, "... %08" PRIX64 " (%s / -%s)\r", displayed_current_seed,
        elapsed_str.c_str(), remaining_str.c_str());
    usleep(1000000);
  }

  for (auto& t : threads) {
    t.join();
  }
}

template <typename... ThreadArgTs>
PSOEncryptionSeedFinder::ThreadResults PSOEncryptionSeedFinder::parallel_find_seed_t(
    ThreadArgTs... args) {

  vector<ThreadResults> all_thread_results;
  all_thread_results.resize(this->num_threads);
  this->parallel_all_seeds_t(this->num_threads, args..., ref(all_thread_results));

  ThreadResults overall_results = all_thread_results[0];
  for (const auto& thread_results : all_thread_results) {
    overall_results.combine_from(thread_results);
  }
  return overall_results;
}



void PSOEncryptionSeedFinder::parallel_all_seeds_thread_fn(
    function<bool(uint32_t, size_t)> fn,
    atomic<uint64_t>& current_seed,
    size_t thread_num) {
  uint64_t seed;
  while ((seed = current_seed.fetch_add(1)) < 0x100000000) {
    if (fn(seed, thread_num)) {
      current_seed = 0x100000000;
    }
  }
}

void PSOEncryptionSeedFinder::find_seed_without_rainbow_table_thread_fn(
    uint64_t flags,
    vector<ThreadResults>& all_results,
    atomic<uint64_t>& current_seed,
    size_t thread_num) {
  size_t plaintext_size = this->plaintexts[0].first.size();

  auto& results = all_results.at(thread_num);
  results.results.clear();
  results.min_differences = plaintext_size + 1;
  results.difference_histogram.clear();

  bool is_v3 = flags & Flag::V3;
  bool skip_little_endian = flags & Flag::SKIP_LITTLE_ENDIAN;
  bool skip_big_endian = flags & Flag::SKIP_BIG_ENDIAN;

  uint64_t seed;
  while ((seed = current_seed.fetch_add(1)) < 0x100000000) {
    string be_decrypt_buf = this->ciphertext.substr(0, plaintext_size);
    string le_decrypt_buf = this->ciphertext.substr(0, plaintext_size);
    if (is_v3) {
      PSOV3Encryption(seed).encrypt_both_endian(
          le_decrypt_buf.data(),
          be_decrypt_buf.data(),
          be_decrypt_buf.size());
    } else {
      PSOV2Encryption(seed).encrypt_both_endian(
          le_decrypt_buf.data(),
          be_decrypt_buf.data(),
          be_decrypt_buf.size());
    }

    for (const auto& plaintext : this->plaintexts) {
      if (!skip_little_endian) {
        size_t diff = difference_match(le_decrypt_buf, plaintext.first);
        results.add_result(Result(seed, diff, false, is_v3));
      } else if (!skip_big_endian) {
        size_t diff = difference_match(be_decrypt_buf, plaintext.first);
        results.add_result(Result(seed, diff, true, is_v3));
      }
    }
    if (results.min_differences == 0) {
      current_seed = 0x100000000;
    }
  }
}

void PSOEncryptionSeedFinder::find_seed_with_rainbow_table_thread_fn(
    int fd,
    size_t page_size,
    vector<ThreadResults>& all_results,
    atomic<uint64_t>& current_seed,
    size_t thread_num) {
  size_t plaintext_size = this->plaintexts[0].first.size();

  auto& results = all_results.at(thread_num);
  results.results.clear();
  results.min_differences = plaintext_size + 1;
  results.difference_histogram.clear();

  uint64_t seed;
  string rainbow_buf(page_size * plaintext_size, '\0');
  while ((seed = current_seed.fetch_add(page_size)) < 0x100000000) {
    preadx(fd, rainbow_buf.data(), rainbow_buf.size(), seed * plaintext_size);
    for (size_t z = 0; z < page_size; z++) {
      for (size_t x = 0; x < plaintext_size; x++) {
        rainbow_buf[z * plaintext_size + x] ^= this->ciphertext[x];
      }
      for (const auto& plaintext : this->plaintexts) {
        size_t diff = difference_match(
            &rainbow_buf[z * plaintext_size], plaintext.first);
        results.add_result(Result(seed, diff));
      }
      if (results.min_differences == 0) {
        current_seed = 0x100000000;
      }
    }
  }
}

void PSOEncryptionSeedFinder::generate_rainbow_table_thread_fn(
    int fd,
    size_t match_length,
    size_t page_size,
    bool is_v3,
    bool is_big_endian,
    atomic<uint64_t>& current_seed,
    size_t) {
  uint64_t seed;
  string buf(match_length * page_size, '\0');
  while ((seed = current_seed.fetch_add(page_size)) < 0x100000000) {
    memset(buf.data(), 0, buf.size());
    for (size_t z = 0; z < page_size; z++) {
      if (is_v3) {
        PSOV3Encryption crypt(seed + z);
        if (is_big_endian) {
          crypt.encrypt_big_endian(buf.data() + z * match_length, match_length);
        } else {
          crypt.encrypt(buf.data() + z * match_length, match_length);
        }
      } else {
        PSOV2Encryption crypt(seed + z);
        if (is_big_endian) {
          crypt.encrypt_big_endian(buf.data() + z * match_length, match_length);
        } else {
          crypt.encrypt(buf.data() + z * match_length, match_length);
        }
      }
    }
    pwritex(fd, buf.data(), buf.size(), seed * match_length);
  }
}
