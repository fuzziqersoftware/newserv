#pragma once

#include <phosg/Encoding.hh>
#include <phosg/Strings.hh>
#include <phosg/Vector.hh>
#include <set>

#include "Text.hh"

constexpr double radians_for_fixed_point_angle(uint16_t angle) {
  return static_cast<double>(angle * 2 * M_PI) / 0x10000;
}

struct VectorXZF {
  le_float x = 0.0;
  le_float z = 0.0;

  inline VectorXZF operator-() const {
    return VectorXZF{-this->x, -this->z};
  }

  inline VectorXZF operator+(const VectorXZF& other) const {
    return VectorXZF{this->x + other.x, this->z + other.z};
  }
  inline VectorXZF operator-(const VectorXZF& other) const {
    return VectorXZF{this->x - other.x, this->z - other.z};
  }

  inline bool operator==(const VectorXZF& other) const {
    return ((this->x == other.x) && (this->z == other.z));
  }
  inline bool operator!=(const VectorXZF& other) const {
    return !this->operator==(other);
  }

  inline double norm() const {
    return sqrt(this->norm2());
  }
  inline double norm2() const {
    return ((this->x * this->x) + (this->z * this->z));
  }
  inline double dist(const VectorXZF& other) const {
    return sqrt(this->dist2(other));
  }
  inline double dist2(const VectorXZF& other) const {
    double x = this->x - other.x;
    double z = this->z - other.z;
    return ((x * x) + (z * z));
  }

  inline VectorXZF rotate_y(double angle) const {
    double s = sin(angle);
    double c = cos(angle);
    return VectorXZF{this->x * c - this->z * s, this->x * s + this->z * c};
  }

  inline std::string str() const {
    return std::format("[VectorXZF x={:g} z={:g}]", this->x, this->z);
  }
} __packed_ws__(VectorXZF, 0x08);

struct VectorXYZF {
  le_float x = 0.0;
  le_float y = 0.0;
  le_float z = 0.0;

  inline operator VectorXZF() const {
    return VectorXZF{this->x, this->z};
  }

  inline VectorXYZF operator-() const {
    return VectorXYZF{-this->x, -this->y, -this->z};
  }

  inline VectorXYZF operator+(const VectorXYZF& other) const {
    return VectorXYZF{this->x + other.x, this->y + other.y, this->z + other.z};
  }
  inline VectorXYZF operator-(const VectorXYZF& other) const {
    return VectorXYZF{this->x - other.x, this->y - other.y, this->z - other.z};
  }

  inline bool operator==(const VectorXYZF& other) const {
    return ((this->x == other.x) && (this->y == other.y) && (this->z == other.z));
  }
  inline bool operator!=(const VectorXYZF& other) const {
    return !this->operator==(other);
  }

  inline double norm() const {
    return sqrt(this->norm2());
  }
  inline double norm2() const {
    return ((this->x * this->x) + (this->y * this->y) + (this->z * this->z));
  }

  inline VectorXYZF rotate_x(double angle) const {
    double s = sin(angle);
    double c = cos(angle);
    return VectorXYZF{this->x, this->y * c - this->z * s, this->y * s + this->z * c};
  }
  inline VectorXYZF rotate_y(double angle) const {
    double s = sin(angle);
    double c = cos(angle);
    return VectorXYZF{this->x * c + this->z * s, this->y, -this->x * s + this->z * c};
  }
  inline VectorXYZF rotate_z(double angle) const {
    double s = sin(angle);
    double c = cos(angle);
    return VectorXYZF{this->x * c - this->y * s, this->x * s + this->y * c, this->z};
  }

  inline std::string str() const {
    return std::format("[VectorXYZF x={:g} y={:g} z={:g}]", this->x, this->y, this->z);
  }
} __packed_ws__(VectorXYZF, 0x0C);

struct VectorXYZTF {
  le_float x = 0.0;
  le_float y = 0.0;
  le_float z = 0.0;
  le_float t = 0.0;
} __packed_ws__(VectorXYZTF, 0x10);

struct VectorXYZI {
  le_uint32_t x = 0;
  le_uint32_t y = 0;
  le_uint32_t z = 0;
} __packed_ws__(VectorXYZI, 0x0C);

template <bool BE>
struct ArrayRefT {
  static constexpr bool IsBE = BE;
  /* 00 */ U32T<BE> count;
  /* 04 */ U32T<BE> offset;
  /* 08 */
} __packed_ws_be__(ArrayRefT, 8);
using ArrayRef = ArrayRefT<false>;
using ArrayRefBE = ArrayRefT<true>;

template <bool BE>
struct RELFileFooterT {
  static constexpr bool IsBE = BE;
  // Relocations is a list of words (le_uint16_t on DC/PC/XB/BB, be_uint16_t on GC) containing the number of
  // doublewords (uint32_t) to skip for each relocation. The relocation pointer starts at the beginning of the file
  // data, and advances by the value of one relocation word (times 4) before each relocation. At each relocated
  // doubleword, the address of the first byte of the file is added to the existing value.
  //
  // For example, if the file data contains the following data (where R specifies doublewords to relocate):
  //   RR RR RR RR ?? ?? ?? ?? ?? ?? ?? ?? RR RR RR RR
  //   RR RR RR RR ?? ?? ?? ?? RR RR RR RR
  // then the relocation words should be 0000, 0003, 0001, and 0002.
  //
  // If there is a small number of relocations, they may be placed in the unused fields of this structure to save space
  // and/or confuse reverse engineers. The game never accesses the last 12 bytes of this structure unless
  // relocations_offset points there, so those 12 bytes may also be omitted entirely in some situations (e.g. in the B2
  // command, without changing code_size, so code_size would technically extend beyond the end of the B2 command).
  U32T<BE> relocations_offset = 0;
  U32T<BE> num_relocations = 0;
  parray<U32T<BE>, 2> unused1;
  U32T<BE> root_offset = 0;
  parray<U32T<BE>, 3> unused2;
} __packed_ws_be__(RELFileFooterT, 0x20);
using RELFileFooter = RELFileFooterT<false>;
using RELFileFooterBE = RELFileFooterT<true>;

template <bool BE>
std::set<uint32_t> all_relocation_offsets_for_rel_file(const void* data, size_t size) {
  phosg::StringReader r(data, size);

  std::set<uint32_t> ret;
  ret.emplace(r.size() - 0x20); // REL footer
  ret.emplace(r.pget<U32T<BE>>(r.size() - 0x10)); // root
  ret.emplace(r.pget<U32T<BE>>(r.size() - 0x20)); // relocations

  const auto& footer = r.pget<RELFileFooterT<BE>>(r.size() - sizeof(RELFileFooterT<BE>));
  auto sub_r = r.sub(footer.relocations_offset, footer.num_relocations * sizeof(U16T<BE>));
  uint32_t offset = 0;
  while (!sub_r.eof()) {
    offset += sub_r.template get<U16T<BE>>() * 4;
    ret.emplace(r.pget<U32T<BE>>(offset));
  }

  return ret;
}

template <typename T>
size_t get_rel_array_count(const std::set<uint32_t>& offsets, size_t start_offset) {
  auto it = offsets.lower_bound(start_offset);
  if (it == offsets.end()) {
    throw std::out_of_range("start offset out of range");
  }
  if (*it == start_offset) {
    it++;
  }
  if (it == offsets.end()) {
    throw std::out_of_range("no further offset beyond start offset");
  }
  return (*it - start_offset) / sizeof(T);
}

template <bool BE>
class RELFileWriter {
public:
  RELFileWriter() = default;
  ~RELFileWriter() = default;

  template <typename T>
  uint32_t put(const T& obj) {
    uint32_t ret = this->w.size();
    this->w.put<T>(obj);
    return ret;
  }

  uint32_t write(const void* data, size_t size) {
    uint32_t ret = this->w.size();
    this->w.write(data, size);
    return ret;
  }

  uint32_t write(const std::string& data) {
    uint32_t ret = this->w.size();
    this->w.write(data);
    return ret;
  }

  uint32_t write_offset(uint32_t value) {
    uint32_t ret = this->w.size();
    this->relocations.emplace(ret);
    this->w.put<U32T<BE>>(value);
    return ret;
  }

  uint32_t write_ref(const ArrayRefT<BE>& ref) {
    uint32_t ret = this->w.size();
    this->w.put<ArrayRefT<BE>>(ref);
    this->relocations.emplace(ret + offsetof(ArrayRefT<BE>, offset));
    return ret;
  }

  void align(size_t alignment) {
    while (this->w.size() & (alignment - 1)) {
      this->w.put_u8(0);
    }
  }

  std::string finalize(uint32_t root_offset) {
    RELFileFooterT<BE> footer;
    footer.root_offset = root_offset;

    this->align(0x20);
    footer.relocations_offset = this->w.size();
    footer.num_relocations = this->relocations.size();
    footer.unused1[0] = 1;
    uint32_t last_offset = 0;
    for (uint32_t reloc_offset : this->relocations) {
      if (reloc_offset & 3) {
        throw std::logic_error("Relocation is not 4-byte aligned");
      }
      size_t reloc_value = (reloc_offset - last_offset) >> 2;
      if (reloc_value > 0xFFFF) {
        throw std::runtime_error("Relocation offset is too far away from previous");
      }
      this->w.put<U16T<BE>>(reloc_value);
      last_offset = reloc_offset;
    }

    align(0x20);
    this->w.put<RELFileFooterT<BE>>(footer);

    return std::move(this->w.str());
  }

  phosg::StringWriter w;
  std::set<uint32_t> relocations;
};
