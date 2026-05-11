#include "DOLFileIndex.hh"

#include <stdio.h>
#include <string.h>

#include <filesystem>
#include <phosg/Filesystem.hh>
#include <phosg/Hash.hh>
#include <phosg/Time.hh>
#include <stdexcept>

#include <resource_file/Emulators/PPC32Emulator.hh>
#include <resource_file/Emulators/SH4Emulator.hh>
#include <resource_file/Emulators/X86Emulator.hh>

#include "CommandFormats.hh"
#include "CommonFileFormats.hh"
#include "Compression.hh"
#include "Loggers.hh"

using namespace std;

DOLFileIndex::DOLFileIndex(const string& directory) {
  if (!std::filesystem::is_directory(directory)) {
    client_functions_log.info_f("DOL file directory is missing");
    return;
  }

  auto menu = make_shared<Menu>(MenuID::PROGRAMS, "Programs");
  this->menu = menu;
  menu->items.emplace_back(ProgramsMenuItemID::GO_BACK, "Go back", "Return to the\nmain menu", 0);

  uint32_t next_menu_item_id = 0;
  for (const auto& item : std::filesystem::directory_iterator(directory)) {
    string filename = item.path().filename().string();
    bool is_dol = filename.ends_with(".dol");
    bool is_compressed_dol = filename.ends_with(".dol.prs");
    if (!is_dol && !is_compressed_dol) {
      continue;
    }
    string name = filename.substr(0, filename.size() - (is_compressed_dol ? 8 : 4));

    try {
      auto dol = make_shared<File>();
      dol->menu_item_id = next_menu_item_id++;
      dol->name = name;

      string path = directory + "/" + filename;
      string file_data = phosg::load_file(path);

      string description;
      if (is_compressed_dol) {
        size_t decompressed_size = prs_decompress_size(file_data);

        phosg::StringWriter w;
        w.put_u32b(file_data.size());
        w.put_u32b(decompressed_size);
        w.write(file_data);
        while (w.size() & 3) {
          w.put_u8(0);
        }
        dol->data = std::move(w.str());

        string compressed_size_str = phosg::format_size(file_data.size());
        string decompressed_size_str = phosg::format_size(decompressed_size);
        client_functions_log.debug_f("Loaded compressed DOL file {} ({} -> {})",
            dol->name, compressed_size_str, decompressed_size_str);
        description = std::format("$C6{}$C7\n{}\n{} (orig)", dol->name, compressed_size_str, decompressed_size_str);

      } else {
        phosg::StringWriter w;
        w.put_u32b(0);
        w.put_u32b(file_data.size());
        w.write(file_data);
        while (w.size() & 3) {
          w.put_u8(0);
        }
        dol->data = std::move(w.str());

        string size_str = phosg::format_size(dol->data.size());
        client_functions_log.debug_f("Loaded DOL file {} ({})", filename, size_str);
        description = std::format("$C6{}$C7\n{}", dol->name, size_str);
      }

      this->name_to_file.emplace(dol->name, dol);
      this->item_id_to_file.emplace_back(dol);

      menu->items.emplace_back(dol->menu_item_id, dol->name, description, MenuItem::Flag::REQUIRES_SEND_FUNCTION_CALL_RUNS_CODE);

    } catch (const exception& e) {
      client_functions_log.warning_f("Failed to load DOL file {}: {}", filename, e.what());
    }
  }
}
