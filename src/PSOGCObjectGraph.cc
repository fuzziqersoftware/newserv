#include "PSOGCObjectGraph.hh"

#include "Text.hh"

using namespace std;

struct TObjectVTable {
  phosg::be_uint32_t unused_a1;
  phosg::be_uint32_t unused_a2;
  phosg::be_uint32_t destroy;
  phosg::be_uint32_t update;
  phosg::be_uint32_t render;
  phosg::be_uint32_t render_shadow;
} __packed_ws__(TObjectVTable, 0x18);

struct TObject {
  phosg::be_uint32_t type_name_addr;
  phosg::be_uint16_t flags;
  parray<uint8_t, 2> unused;
  phosg::be_uint32_t prev_sibling_addr;
  phosg::be_uint32_t next_sibling_addr;
  phosg::be_uint32_t parent_addr;
  phosg::be_uint32_t children_head_addr;
  phosg::be_uint32_t vtable_addr;
} __packed_ws__(TObject, 0x1C);

PSOGCObjectGraph::PSOGCObjectGraph(const string& memory_data, uint32_t root_address) {
  phosg::StringReader r(memory_data);
  this->root = this->parse_object_memo(r, root_address);
}

shared_ptr<PSOGCObjectGraph::VTable> PSOGCObjectGraph::parse_vtable_memo(phosg::StringReader& r, uint32_t addr) {
  try {
    return this->all_vtables.at(addr);
  } catch (const out_of_range&) {
  }

  const auto& vt = r.pget<TObjectVTable>(addr & 0x01FFFFFF);
  auto ret = this->all_vtables.emplace(addr, make_shared<VTable>()).first->second;
  ret->address = addr;
  ret->destroy_addr = vt.destroy;
  ret->update_addr = vt.update;
  ret->render_addr = vt.render;
  ret->render_shadow_addr = vt.render_shadow;
  return ret;
}

shared_ptr<PSOGCObjectGraph::Object> PSOGCObjectGraph::parse_object_memo(phosg::StringReader& r, uint32_t addr) {
  try {
    return this->all_objects.at(addr);
  } catch (const out_of_range&) {
  }

  const auto& obj = r.pget<TObject>(addr & 0x01FFFFFF);
  string type_name = r.pget_cstr(obj.type_name_addr & 0x01FFFFFF);

  auto ret = this->all_objects.emplace(addr, make_shared<Object>()).first->second;
  ret->address = addr;
  ret->flags = obj.flags;
  ret->type_name = std::move(type_name);
  ret->vtable = this->parse_vtable_memo(r, obj.vtable_addr);
  if (obj.parent_addr) {
    ret->parent = this->parse_object_memo(r, obj.parent_addr);
  }
  if (obj.children_head_addr) {
    uint32_t child_addr = obj.children_head_addr;
    while (child_addr) {
      ret->children.emplace_back(this->parse_object_memo(r, child_addr));
      child_addr = r.pget<TObject>(child_addr & 0x01FFFFFF).next_sibling_addr;
    }
  }
  return ret;
}

void PSOGCObjectGraph::print(FILE* stream) const {
  this->root->print(stream);
}

void PSOGCObjectGraph::Object::print(FILE* stream, size_t indent_level) const {
  for (size_t z = 0; z < indent_level; z++) {
    fputc(' ', stream);
    fputc(' ', stream);
  }
  fprintf(stream, "%s +%04hX @ %08" PRIX32 " (VT %08" PRIX32 ": destroy=%08" PRIX32 " update=%08" PRIX32 " render=%08" PRIX32 " render_shadow=%08" PRIX32 ")\n",
      this->type_name.c_str(),
      this->flags,
      this->address,
      this->vtable->address,
      this->vtable->destroy_addr,
      this->vtable->update_addr,
      this->vtable->render_addr,
      this->vtable->render_shadow_addr);
  for (const auto& child : this->children) {
    child->print(stream, indent_level + 1);
  }
}
