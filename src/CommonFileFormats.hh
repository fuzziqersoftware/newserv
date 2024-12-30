#pragma once

#include <phosg/Encoding.hh>
#include <phosg/Vector.hh>

#include "Text.hh"

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

  inline std::string str() const {
    return phosg::string_printf("[VectorXZF x=%g z=%g]", this->x.load(), this->z.load());
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

  inline std::string str() const {
    return phosg::string_printf("[VectorXYZF x=%g y=%g z=%g]", this->x.load(), this->y.load(), this->z.load());
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
} __packed__;
using ArrayRef = ArrayRefT<false>;
using ArrayRefBE = ArrayRefT<true>;
check_struct_size(ArrayRef, 8);
check_struct_size(ArrayRefBE, 8);

template <bool BE>
struct RELFileFooterT {
  static constexpr bool IsBE = BE;
  // Relocations is a list of words (le_uint16_t on DC/PC/XB/BB, be_uint16_t on
  // GC) containing the number of doublewords (uint32_t) to skip for each
  // relocation. The relocation pointer starts immediately after the
  // checksum_size field in the header, and advances by the value of one
  // relocation word (times 4) before each relocation. At each relocated
  // doubleword, the address of the first byte of the code (after checksum_size)
  // is added to the existing value.
  // For example, if the code segment contains the following data (where R
  // specifies doublewords to relocate):
  //   RR RR RR RR ?? ?? ?? ?? ?? ?? ?? ?? RR RR RR RR
  //   RR RR RR RR ?? ?? ?? ?? RR RR RR RR
  // then the relocation words should be 0000, 0003, 0001, and 0002.
  // If there is a small number of relocations, they may be placed in the unused
  // fields of this structure to save space and/or confuse reverse engineers.
  // The game never accesses the last 12 bytes of this structure unless
  // relocations_offset points there, so those 12 bytes may also be omitted
  // entirely in situations (e.g. in the B2 command, without changing code_size,
  // so code_size would technically extend beyond the end of the B2 command).
  U32T<BE> relocations_offset = 0;
  U32T<BE> num_relocations = 0;
  parray<U32T<BE>, 2> unused1;
  U32T<BE> root_offset = 0;
  parray<U32T<BE>, 3> unused2;
} __attribute__((packed));
using RELFileFooter = RELFileFooterT<false>;
using RELFileFooterBE = RELFileFooterT<true>;
check_struct_size(RELFileFooter, 0x20);
check_struct_size(RELFileFooterBE, 0x20);
