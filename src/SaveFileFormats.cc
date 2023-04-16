#include "SaveFileFormats.hh"

#include <stdexcept>
#include <string>

using namespace std;

ShuffleTables::ShuffleTables(PSOV2Encryption& crypt) {
  for (size_t x = 0; x < 0x100; x++) {
    this->forward_table[x] = x;
  }

  int32_t r28 = 0xFF;
  uint8_t* r31 = &this->forward_table[0xFF];
  while (r28 >= 0) {
    uint32_t r3 = this->pseudorand(crypt, r28 + 1);
    if (r3 >= 0x100) {
      throw logic_error("bad r3");
    }
    uint8_t t = this->forward_table[r3];
    this->forward_table[r3] = *r31;
    *r31 = t;

    this->reverse_table[t] = r28;
    r31--;
    r28--;
  }
}

uint32_t ShuffleTables::pseudorand(PSOV2Encryption& crypt, uint32_t prev) {
  return (((prev & 0xFFFF) * ((crypt.next() >> 16) & 0xFFFF)) >> 16) & 0xFFFF;
}

void ShuffleTables::shuffle(void* vdest, const void* vsrc, size_t size, bool reverse) const {
  uint8_t* dest = reinterpret_cast<uint8_t*>(vdest);
  const uint8_t* src = reinterpret_cast<const uint8_t*>(vsrc);
  const uint8_t* table = reverse ? this->reverse_table : this->forward_table;

  for (size_t block_offset = 0; block_offset < (size & 0xFFFFFF00); block_offset += 0x100) {
    for (size_t z = 0; z < 0x100; z++) {
      dest[block_offset + table[z]] = src[block_offset + z];
    }
  }

  // Any remaining bytes that don't fill an entire block are not shuffled
  memcpy(&dest[size & 0xFFFFFF00], &src[size & 0xFFFFFF00], size & 0xFF);
}

bool PSOGCIFileHeader::checksum_correct() const {
  uint32_t cs = crc32(&this->game_name, this->game_name.bytes());
  cs = crc32(&this->embedded_seed, sizeof(this->embedded_seed), cs);
  cs = crc32(&this->file_name, this->file_name.bytes(), cs);
  cs = crc32(&this->banner, this->banner.bytes(), cs);
  cs = crc32(&this->icon, this->icon.bytes(), cs);
  cs = crc32(&this->data_size, sizeof(this->data_size), cs);
  cs = crc32("\0\0\0\0", 4, cs); // this->checksum (treated as zero)
  return (cs == this->checksum);
}

void PSOGCIFileHeader::check() const {
  if (!this->checksum_correct()) {
    throw runtime_error("GCI file unencrypted header checksum is incorrect");
  }
  if (this->developer_id[0] != '8' || this->developer_id[1] != 'P') {
    throw runtime_error("GCI file is not for a Sega game");
  }
  if (this->game_id[0] != 'G') {
    throw runtime_error("GCI file is not for a GameCube game");
  }
  if (this->game_id[1] != 'P') {
    throw runtime_error("GCI file is not for Phantasy Star Online");
  }
  if ((this->game_id[1] != 'P') ||
      ((this->game_id[2] != 'S') && (this->game_id[2] != 'O'))) {
    throw runtime_error("GCI file is not for Phantasy Star Online");
  }
}

bool PSOGCIFileHeader::is_ep12() const {
  return (this->game_id[2] == 'O');
}

bool PSOGCIFileHeader::is_ep3() const {
  return (this->game_id[2] == 'S');
}

uint32_t compute_psogc_timestamp(
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t hour,
    uint8_t minute,
    uint8_t second) {
  static uint16_t month_start_day[12] = {
      0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

  uint32_t year_start_day = ((year - 1998) >> 2) + (year - 2000) * 365;
  if ((((year - 1998) & 3) == 0) && (month < 3)) {
    year_start_day--;
  }
  uint32_t res_day = (day - 1) + year_start_day + month_start_day[month - 1];
  return second + (minute + (hour + (res_day * 24)) * 60) * 60;
}
