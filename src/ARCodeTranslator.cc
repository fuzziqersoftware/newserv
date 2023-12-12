#include "ARCodeTranslator.hh"

#include <future>
#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <resource_file/ExecutableFormats/DOLFile.hh>

using namespace std;

class ARCodeTranslator {
public:
  enum class ExpandMethod {
    FORWARD = 0,
    BACKWARD,
    BOTH,
  };

  ARCodeTranslator(const string& directory)
      : log("[ar-trans] "),
        directory(directory) {
    while (ends_with(this->directory, "/")) {
      this->directory.pop_back();
    }
    for (const auto& filename : list_directory(this->directory)) {
      if (ends_with(filename, ".dol")) {
        string name = filename.substr(0, filename.size() - 4);
        string path = directory + "/" + filename;
        this->files.emplace(name, make_shared<DOLFile>(path.c_str()));
        this->log.info("Loaded %s", name.c_str());
      }
    }
  }
  ~ARCodeTranslator() = default;

  const string& get_source_filename() const {
    return this->src_filename;
  }
  void set_source_file(const string& filename) {
    this->src_filename = filename;
    this->src_file = files.at(this->src_filename);
  }

  void find_rtoc_global_regs() const {
    for (const auto& it : files) {
      bool r2_high_found = false;
      bool r2_low_found = false;
      bool r13_high_found = false;
      bool r13_low_found = false;
      uint32_t r2 = 0;
      uint32_t r13 = 0;
      for (const auto& section : it.second->sections) {
        if (!section.is_text) {
          continue;
        }
        StringReader r(section.data);
        while (!r.eof() && r.where()) {
          uint32_t opcode = r.get_u32b();
          if ((opcode & 0xFFFF0000) == 0x3DA00000) {
            if (r13_high_found) {
              throw runtime_error("multiple values for r13_high");
            }
            r13_high_found = true;
            r13 |= (opcode << 16);
          } else if ((opcode & 0xFFFF0000) == 0x3C400000) {
            if (r2_high_found) {
              throw runtime_error("multiple values for r2_high");
            }
            r2_high_found = true;
            r2 |= (opcode << 16);
          } else if ((opcode & 0xFFFF0000) == 0x61AD0000) {
            if (r13_low_found) {
              throw runtime_error("multiple values for r13_low");
            }
            r13_low_found = true;
            r13 |= (opcode & 0xFFFF);
          } else if ((opcode & 0xFFFF0000) == 0x60420000) {
            if (r2_low_found) {
              throw runtime_error("multiple values for r2_low");
            }
            r2_low_found = true;
            r2 |= (opcode & 0xFFFF);
          }
        }
      }
      if (r2_low_found && r2_high_found) {
        fprintf(stderr, "(%s) r2 = %08" PRIX32 "\n", it.first.c_str(), r2);
      } else {
        fprintf(stderr, "(%s) r2 = __MISSING__\n", it.first.c_str());
      }
      if (r13_low_found && r13_high_found) {
        fprintf(stderr, "(%s) r13 = %08" PRIX32 "\n", it.first.c_str(), r13);
      } else {
        fprintf(stderr, "(%s) r13 = __MISSING__\n", it.first.c_str());
      }
    }
  }

  uint32_t find_match(shared_ptr<const DOLFile> dest_file, uint32_t src_address, ExpandMethod expand_method) const {
    if (!this->src_file) {
      throw runtime_error("no source file selected");
    }

    const DOLFile::Section* src_section = nullptr;
    for (const auto& sec : this->src_file->sections) {
      if (src_address >= sec.address && src_address < sec.address + sec.data.size()) {
        src_section = &sec;
        break;
      }
    }
    if (!src_section) {
      throw runtime_error("source address not within any section");
    }
    size_t src_offset = src_address - src_section->address;
    size_t src_bytes_available_before = src_section->data.size() - src_offset;
    size_t src_bytes_available_after = src_section->data.size() - src_offset;
    this->log.info("(find_match) Source offset = %08zX with %zX/%zX bytes available before/after",
        src_offset, src_bytes_available_before, src_bytes_available_after);

    size_t match_bytes_before = 0;
    size_t match_bytes_after = 0;
    while (match_bytes_before + match_bytes_after + 4 < 0x100) {
      size_t num_matches = 0;
      size_t last_match_address = 0;
      size_t match_length = match_bytes_before + match_bytes_after + 4;
      StringReader src_r(src_section->data.data() + src_offset - match_bytes_before, match_length);
      for (const auto& dest_section : dest_file->sections) {
        for (size_t dest_match_offset = 0;
             dest_match_offset < dest_section.data.size();
             dest_match_offset += 4) {
          src_r.go(0);
          StringReader dest_r(dest_section.data.data() + dest_match_offset, match_length);
          size_t z;
          for (z = 0; z < match_length; z += 4) {
            if (src_section->is_text) {
              uint32_t src_opcode = src_r.get_u32b();
              uint32_t dest_opcode = dest_r.get_u32b();
              uint32_t src_class = src_opcode & 0xFC000000;
              if (src_class != (dest_opcode & 0xFC000000)) {
                break;
              }
              if (src_class == 0x48000000) {
                // b +-offset
                src_opcode &= 0xFC000003;
                dest_opcode &= 0xFC000003;
              } else if (src_class == 0x40000000) {
                // bc +- offset
                src_opcode &= 0xFFFF0003;
                dest_opcode &= 0xFFFF0003;
              } else if (((src_opcode & 0xEC1F0000) == 0x800D0000) || ((src_opcode & 0xEC1F0000) == 0x80020000)) {
                // lwz rXX, [r2/r13 +- offset] OR stw [r2/r13 +- offset], rXX
                src_opcode &= 0xFFFF0000;
                dest_opcode &= 0xFFFF0000;
              }
              if (src_opcode != dest_opcode) {
                break;
              }
            } else {
              uint32_t src_data = src_r.get_u32b();
              uint32_t dest_data = dest_r.get_u32b();
              if ((src_data & 0xFE000000) == 0x80000000) {
                src_data &= 0xFE000003;
              }
              if ((dest_data & 0xFE000000) == 0x80000000) {
                dest_data &= 0xFE000003;
              }
              if (src_data != dest_data) {
                break;
              }
            }
          }
          if (z == match_length) {
            num_matches++;
            last_match_address = dest_section.address + dest_match_offset + match_bytes_before;
          }
        }
      }
      this->log.info("(find_match) For match length %zX, %zu matches found", match_length, num_matches);
      if (num_matches == 1) {
        return last_match_address;
      } else if (num_matches == 0) {
        throw runtime_error("did not find exactly one match");
      }
      switch (expand_method) {
        case ExpandMethod::BACKWARD:
          match_bytes_before += 4;
          if (src_bytes_available_before < match_bytes_before) {
            throw runtime_error("no more source data to match");
          }
          break;
        case ExpandMethod::FORWARD:
          match_bytes_after += 4;
          if (src_bytes_available_after < match_bytes_after) {
            throw runtime_error("no more source data to match");
          }
          break;
        case ExpandMethod::BOTH:
          match_bytes_before += 4;
          match_bytes_after += 4;
          if ((src_bytes_available_before < match_bytes_before) && (src_bytes_available_after < match_bytes_after)) {
            throw runtime_error("no more source data to match");
          } else if (src_bytes_available_before < match_bytes_before) {
            match_bytes_before -= 4;
          } else if (src_bytes_available_after < match_bytes_after) {
            match_bytes_after -= 4;
          }
          break;
        default:
          throw logic_error("invalid expand method");
      }
    }
    throw runtime_error("scan field too long; too many matches");
  }

  void find_all_matches(uint32_t src_addr) const {
    if (!this->src_file) {
      throw runtime_error("no source file selected");
    }

    unordered_map<string, uint32_t> results;
    for (const auto& it : files) {
      if (it.second == this->src_file) {
        log.info("(%s) %08" PRIX32 " (from source)", it.first.c_str(), src_addr);
        results.emplace(it.first, src_addr);
      } else {

        auto f_forward = async(&ARCodeTranslator::find_match, this, it.second, src_addr, ExpandMethod::FORWARD);
        auto f_backward = async(&ARCodeTranslator::find_match, this, it.second, src_addr, ExpandMethod::BACKWARD);
        auto f_both = async(&ARCodeTranslator::find_match, this, it.second, src_addr, ExpandMethod::BOTH);

        uint32_t match_addr_forward = 0;
        uint32_t match_addr_backward = 0;
        uint32_t match_addr_both = 0;
        try {
          match_addr_forward = f_forward.get();
          log.info("(%s) (forward) %08" PRIX32, it.first.c_str(), match_addr_forward);
        } catch (const exception& e) {
          log.error("(%s) (forward) failed: %s", it.first.c_str(), e.what());
        }
        try {
          match_addr_backward = f_backward.get();
          log.info("(%s) (backward) %08" PRIX32, it.first.c_str(), match_addr_backward);
        } catch (const exception& e) {
          log.error("(%s) (backward) failed: %s", it.first.c_str(), e.what());
        }
        try {
          match_addr_both = f_both.get();
          log.info("(%s) (both) %08" PRIX32, it.first.c_str(), match_addr_both);
        } catch (const exception& e) {
          log.error("(%s) (both) failed: %s", it.first.c_str(), e.what());
        }

        uint32_t all_addrs = match_addr_backward | match_addr_forward | match_addr_both;
        if (!all_addrs) {
          log.error("(%s) no match found", it.first.c_str());
        } else if ((match_addr_forward && (match_addr_forward != all_addrs)) ||
            (match_addr_backward && (match_addr_backward != all_addrs)) ||
            (match_addr_both && (match_addr_both != all_addrs))) {
          log.error("(%s) different matches found by different methods", it.first.c_str());
        } else {
          results.emplace(it.first, all_addrs);
        }
      }
    }
    for (const auto& it : results) {
      fprintf(stdout, "%s => %08" PRIX32 "\n", it.first.c_str(), it.second);
    }
  }

  void handle_command(const string& command) {
    auto tokens = split(command, ' ');
    if (tokens.empty()) {
      throw runtime_error("no command given");
    }
    strip_trailing_whitespace(tokens[tokens.size() - 1]);

    if (tokens[0] == "use") {
      this->set_source_file(tokens.at(1));
    } else if (tokens[0] == "match") {
      this->find_all_matches(stoul(tokens.at(1), nullptr, 16));
    } else if (tokens[0] == "find-globals") {
      this->find_rtoc_global_regs();
    } else if (!tokens[0].empty()) {
      throw runtime_error("unknown command");
    }
  }

  void run_shell() {
    while (!feof(stdin)) {
      if (!this->src_filename.empty()) {
        fprintf(stdout, "ar-trans:%s/%s> ", this->directory.c_str(), this->src_filename.c_str());
      } else {
        fprintf(stdout, "ar-trans:%s> ", this->directory.c_str());
      }
      fflush(stdout);

      string command = fgets(stdin);
      try {
        this->handle_command(command);
      } catch (const exception& e) {
        this->log.error("Failed: %s", e.what());
      }
    }
    fputc('\n', stdout);
  }

private:
  PrefixedLogger log;
  string directory;
  unordered_map<string, shared_ptr<const DOLFile>> files;
  string src_filename;
  shared_ptr<const DOLFile> src_file;
};

void run_ar_code_translator(const std::string& directory, const std::string& use_filename, const std::string& command) {
  ARCodeTranslator trans(directory);
  if (!use_filename.empty()) {
    trans.set_source_file(use_filename);
  }

  if (!command.empty()) {
    trans.handle_command(command);
  } else {
    trans.run_shell();
  }
}
