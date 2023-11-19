#include "ARCodeTranslator.hh"

#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <resource_file/ExecutableFormats/DOLFile.hh>

using namespace std;

void run_ar_code_translator(const std::string& initial_directory) {
  string directory = initial_directory;
  while (ends_with(directory, "/")) {
    directory.resize(directory.size() - 1);
  }
  PrefixedLogger log("[ar-trans] ");

  unordered_map<string, shared_ptr<DOLFile>> files;
  for (const auto& filename : list_directory(directory)) {
    if (ends_with(filename, ".dol")) {
      string name = filename.substr(0, filename.size() - 4);
      string path = directory + "/" + filename;
      files.emplace(name, new DOLFile(path.c_str()));
      log.info("Loaded %s", name.c_str());
    }
  }

  string source_filename;
  shared_ptr<DOLFile> source_file;
  auto find_match = [&](std::shared_ptr<DOLFile> target_file, uint32_t source_address) -> uint32_t {
    const DOLFile::Section* source_section = nullptr;
    for (const auto& sec : source_file->sections) {
      if (source_address >= sec.address && source_address < sec.address + sec.data.size()) {
        source_section = &sec;
        break;
      }
    }
    if (!source_section) {
      throw runtime_error("source address not within any section");
    }
    size_t source_offset = source_address - source_section->address;
    size_t source_bytes_available_after = source_section->data.size() - source_offset;
    log.info("(find_match) Source offset = %08zX with %08zX bytes available after", source_offset, source_bytes_available_after);

    for (size_t match_length = 4;
         match_length < min<size_t>(source_bytes_available_after, 0x100);
         match_length += 4) {
      size_t num_matches = 0;
      size_t last_match_address = 0;
      StringReader source_r(source_section->data.data() + source_offset, match_length);
      for (const auto& target_section : target_file->sections) {
        for (size_t target_section_offset = 0;
             target_section_offset + match_length <= target_section.data.size();
             target_section_offset += 4) {
          source_r.go(0);
          StringReader target_r(target_section.data.data() + target_section_offset, match_length);
          size_t z;
          for (z = 0; z < match_length; z += 4) {
            if (source_section->is_text) {
              uint32_t source_opcode = source_r.get_u32b();
              uint32_t target_opcode = target_r.get_u32b();
              uint32_t source_class = source_opcode & 0xFC000000;
              if (source_class != (target_opcode & 0xFC000000)) {
                break;
              }
              if (source_class == 0x48000000) {
                source_opcode &= 0xFC000003;
                target_opcode &= 0xFC000003;
              } else if (source_class == 0x40000000) {
                source_opcode &= 0xFFFF0003;
                target_opcode &= 0xFFFF0003;
              }
              if (source_opcode != target_opcode) {
                break;
              }
            } else {
              if (source_r.get_u32l() != target_r.get_u32l()) {
                break;
              }
            }
          }
          if (z == match_length) {
            num_matches++;
            last_match_address = target_section.address + target_section_offset;
          }
        }
      }
      log.info("(find_match) For match length %zX, %zu matches found", match_length, num_matches);
      if (num_matches == 1) {
        return last_match_address;
      } else if (num_matches == 0) {
        throw runtime_error("did not find exactly one match");
      }
    }
    throw runtime_error("scan field too long; too many matches");
  };

  while (!feof(stdin)) {
    if (!source_filename.empty()) {
      fprintf(stdout, "ar-trans:%s/%s> ", directory.c_str(), source_filename.c_str());
    } else {
      fprintf(stdout, "ar-trans:%s> ", directory.c_str());
    }
    fflush(stdout);

    string command = fgets(stdin);
    try {
      strip_trailing_whitespace(command);
      auto tokens = split(command, ' ');
      if (tokens.empty()) {
        throw runtime_error("no command given");

      } else if (tokens[0] == "use") {
        source_filename = tokens.at(1);
        source_file = files.at(source_filename);

      } else if (tokens[0] == "match") {
        if (!source_file) {
          throw runtime_error("no source file selected");
        }

        uint32_t source_addr = stoul(tokens.at(1), nullptr, 16);
        for (const auto& it : files) {
          if (it.second == source_file) {
            log.info("(%s) %08" PRIX32, it.first.c_str(), source_addr);
          } else {
            try {
              uint32_t match_addr = find_match(it.second, source_addr);
              log.info("(%s) %08" PRIX32, it.first.c_str(), match_addr);
            } catch (const exception& e) {
              log.error("(%s) failed: %s", it.first.c_str(), e.what());
            }
          }
        }

      } else if (!tokens[0].empty()) {
        throw runtime_error("unknown command");
      }
    } catch (const exception& e) {
      log.error("Failed: %s", e.what());
    }
  }

  fputc('\n', stdout);
}
