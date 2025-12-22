#pragma once

#include <inttypes.h>
#include <stddef.h>

#include <memory>
#include <phosg/Encoding.hh>
#include <phosg/Random.hh>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "Compression.hh"
#include "Text.hh"
#include "Types.hh"

class RandomGenerator {
public:
  virtual ~RandomGenerator() = default;
  virtual uint32_t next() = 0;

  inline uint32_t seed() const {
    return this->initial_seed;
  }

protected:
  uint32_t initial_seed;
  RandomGenerator(uint32_t seed);
};

class DisabledRandomGenerator : public RandomGenerator {
public:
  DisabledRandomGenerator();
  virtual ~DisabledRandomGenerator() = default;

  virtual uint32_t next();
};

class MT19937Generator : public RandomGenerator {
public:
  explicit MT19937Generator(uint32_t seed);
  virtual ~MT19937Generator() = default;

  virtual uint32_t next();

private:
  std::mt19937 gen;
};

class PSOEncryption {
public:
  enum class Type {
    V2 = 0,
    V3,
    BB,
    JSD0,
  };

  virtual ~PSOEncryption() = default;

  virtual void encrypt(void* data, size_t size) = 0;
  virtual void decrypt(void* data, size_t size);

  inline void encrypt(std::string& data) {
    this->encrypt(data.data(), data.size());
  }
  inline void decrypt(std::string& data) {
    this->decrypt(data.data(), data.size());
  }

  virtual Type type() const = 0;

protected:
  PSOEncryption() = default;
};

class PSOLFGEncryption : public PSOEncryption, public RandomGenerator {
public:
  virtual void encrypt(void* data, size_t size);
  void encrypt_big_endian(void* data, size_t size);
  void encrypt_minus(void* data, size_t size);
  void encrypt_big_endian_minus(void* data, size_t size);
  void encrypt_both_endian(void* le_data, void* be_data, size_t size);

  template <bool BE>
  void encrypt_t(void* vdata, size_t size) {
    size_t uint32_count = size >> 2;
    size_t extra_bytes = size & 3;
    U32T<BE>* data = reinterpret_cast<U32T<BE>*>(vdata);
    for (size_t x = 0; x < uint32_count; x++) {
      data[x] ^= this->next();
    }
    if (extra_bytes) {
      U32T<BE> last = 0;
      memcpy(&last, &data[uint32_count], extra_bytes);
      last ^= this->next();
      memcpy(&data[uint32_count], &last, extra_bytes);
    }
  }

  template <bool BE>
  void encrypt_minus_t(void* vdata, size_t size) {
    size_t uint32_count = size >> 2;
    size_t extra_bytes = size & 3;
    U32T<BE>* data = reinterpret_cast<U32T<BE>*>(vdata);
    for (size_t x = 0; x < uint32_count; x++) {
      data[x] = this->next() - data[x];
    }
    if (extra_bytes) {
      U32T<BE> last = 0;
      memcpy(&last, &data[uint32_count], extra_bytes);
      last = this->next() - last;
      memcpy(&data[uint32_count], &last, extra_bytes);
    }
  }

  virtual uint32_t next();

protected:
  PSOLFGEncryption(uint32_t seed, size_t stream_length, size_t end_offset);

  virtual void update_stream() = 0;

  std::vector<uint32_t> stream;
  size_t offset;
  size_t end_offset;
};

class PSOV2Encryption : public PSOLFGEncryption {
public:
  explicit PSOV2Encryption(uint32_t seed);
  virtual Type type() const;

protected:
  virtual void update_stream();

  static constexpr size_t STREAM_LENGTH = 0x38;
};

class PSOV3Encryption : public PSOLFGEncryption {
public:
  explicit PSOV3Encryption(uint32_t key);
  virtual Type type() const;

protected:
  virtual void update_stream();

  static constexpr size_t STREAM_LENGTH = 521;
};

class PSOBBEncryption : public PSOEncryption {
public:
  enum Subtype : uint8_t {
    STANDARD = 0x00,
    MOCB1 = 0x01,
    JSD1 = 0x02,
    TFS1 = 0x03,
  };

  struct KeyFile {
    // initial_keys are actually a stream of uint32_ts, but we treat them as bytes for code simplicity
    union InitialKeys {
      uint8_t jsd1_stream_offset;
      parray<uint8_t, 0x48> as8;
      parray<le_uint32_t, 0x12> as32;
      InitialKeys() : as32() {}
      InitialKeys(const InitialKeys& other) : as32(other.as32) {}
    } __packed_ws__(InitialKeys, 0x48);
    union PrivateKeys {
      parray<uint8_t, 0x1000> as8;
      parray<le_uint32_t, 0x400> as32;
      PrivateKeys() : as32() {}
      PrivateKeys(const PrivateKeys& other) : as32(other.as32) {}
    } __packed_ws__(PrivateKeys, 0x1000);
    InitialKeys initial_keys;
    PrivateKeys private_keys;
    // This field only really needs to be one byte, but annoyingly, some compilers pad this structure to a longer
    // alignment, presumably because the unions above contain structures with 32-bit alignment. To prevent this
    // structure's size from not matching the .nsk files' sizes, we use an unnecessarily large size for this field.
    le_uint64_t subtype;
  } __packed_ws__(KeyFile, 0x1050);

  PSOBBEncryption(const KeyFile& key, const void* seed, size_t seed_size);

  virtual void encrypt(void* data, size_t size);
  virtual void decrypt(void* data, size_t size);

  virtual Type type() const;

protected:
  KeyFile state;

  void tfs1_scramble(uint32_t* out1, uint32_t* out2) const;
  void apply_seed(const void* original_seed, size_t seed_size);
};

// The following classes provide support for automatically detecting which type of encryption a client is using based
// on their initial response to the server

class PSOV2OrV3DetectorEncryption : public PSOEncryption {
public:
  PSOV2OrV3DetectorEncryption(
      uint32_t key,
      const std::unordered_set<uint32_t>& v2_matches,
      const std::unordered_set<uint32_t>& v3_matches);

  virtual void encrypt(void* data, size_t size);

  virtual Type type() const;

protected:
  uint32_t key;
  const std::unordered_set<uint32_t>& v2_matches;
  const std::unordered_set<uint32_t>& v3_matches;
  std::unique_ptr<PSOEncryption> active_crypt;
};

class PSOV2OrV3ImitatorEncryption : public PSOEncryption {
public:
  PSOV2OrV3ImitatorEncryption(
      uint32_t key, std::shared_ptr<PSOV2OrV3DetectorEncryption> client_crypt);

  virtual void encrypt(void* data, size_t size);

  virtual Type type() const;

protected:
  uint32_t key;
  std::shared_ptr<const PSOV2OrV3DetectorEncryption> detector_crypt;
  std::shared_ptr<PSOEncryption> active_crypt;
};

// The following classes provide support for multiple PSOBB private keys, and the ability to automatically detect which
// key the client is using based on the first 8 bytes they send

class PSOBBMultiKeyDetectorEncryption : public PSOEncryption {
public:
  PSOBBMultiKeyDetectorEncryption(
      const std::vector<std::shared_ptr<const PSOBBEncryption::KeyFile>>& possible_keys,
      const std::unordered_set<std::string>& expected_first_data,
      const void* seed,
      size_t seed_size);

  virtual void encrypt(void* data, size_t size);
  virtual void decrypt(void* data, size_t size);

  inline std::shared_ptr<const PSOBBEncryption::KeyFile> get_active_key() const {
    return this->active_key;
  }
  inline const std::string& get_seed() const {
    return this->seed;
  }

  virtual Type type() const;

protected:
  std::vector<std::shared_ptr<const PSOBBEncryption::KeyFile>> possible_keys;
  std::shared_ptr<const PSOBBEncryption::KeyFile> active_key;
  std::shared_ptr<PSOBBEncryption> active_crypt;
  const std::unordered_set<std::string>& expected_first_data;
  std::string seed;
};

class PSOBBMultiKeyImitatorEncryption : public PSOEncryption {
public:
  PSOBBMultiKeyImitatorEncryption(
      std::shared_ptr<const PSOBBMultiKeyDetectorEncryption> client_crypt,
      const void* seed,
      size_t seed_size,
      bool jsd1_use_detector_seed);

  virtual void encrypt(void* data, size_t size);
  virtual void decrypt(void* data, size_t size);

  virtual Type type() const;

protected:
  std::shared_ptr<PSOBBEncryption> ensure_crypt();

  std::shared_ptr<const PSOBBMultiKeyDetectorEncryption> detector_crypt;
  std::shared_ptr<PSOBBEncryption> active_crypt;
  std::string seed;
  bool jsd1_use_detector_seed;
};

class JSD0Encryption : public PSOEncryption {
public:
  JSD0Encryption(const void* seed, size_t seed_size);

  virtual void encrypt(void* data, size_t size);
  virtual void decrypt(void* data, size_t size);

  virtual Type type() const = 0;

private:
  uint8_t key;
};

void decrypt_trivial_gci_data(void* data, size_t size, uint8_t basis);

uint32_t encrypt_challenge_time(uint16_t value);
uint16_t decrypt_challenge_time(uint32_t value);

template <bool BE>
class ChallengeTimeT {
private:
  U32T<BE> value;

public:
  ChallengeTimeT() : value(0) {}
  ChallengeTimeT(uint16_t v) {
    this->encode(v);
  }
  ChallengeTimeT(const ChallengeTimeT& other) = default;
  ChallengeTimeT(ChallengeTimeT&& other) = default;
  ChallengeTimeT& operator=(const ChallengeTimeT& other) = default;
  ChallengeTimeT& operator=(ChallengeTimeT&& other) = default;

  bool has_value() const {
    return this->value != 0;
  }

  uint32_t load_raw() const {
    return this->value;
  }
  void store_raw(uint32_t value) {
    this->value = value;
  }

  uint16_t decode() const {
    return decrypt_challenge_time(this->value);
  }
  void encode(uint16_t v) {
    this->value = ((v == 0) || (v == 0xFFFF)) ? 0 : encrypt_challenge_time(v);
  }

  operator ChallengeTimeT<!BE>() const {
    ChallengeTimeT<!BE> ret;
    ret.store_raw(this->value);
    return ret;
  }
} __attribute__((packed));
using ChallengeTime = ChallengeTimeT<false>;
using ChallengeTimeBE = ChallengeTimeT<true>;
check_struct_size(ChallengeTime, 4);
check_struct_size(ChallengeTimeBE, 4);

std::string decrypt_v2_registry_value(const void* data, size_t size);

inline std::string decrypt_v2_registry_value(const std::string& s) {
  return decrypt_v2_registry_value(s.data(), s.size());
}

template <bool BE>
std::string decrypt_pr1_data(const void* data, size_t size) {
  if (size < 4) {
    throw std::runtime_error("not enough data for PR1 footer");
  }
  phosg::StringReader r(data, size);
  std::string ret = r.read(size - 4);
  PSOV2Encryption crypt(r.get<U32T<BE>>());
  if constexpr (BE) {
    crypt.encrypt_big_endian(ret.data(), ret.size());
  } else {
    crypt.decrypt(ret.data(), ret.size());
  }
  return ret;
}

template <bool BE>
std::string encrypt_pr1_data(const void* data, size_t size, uint32_t seed) {
  phosg::StringWriter w;
  w.write(data, size);
  w.put<U32T<BE>>(seed);
  PSOV2Encryption crypt(seed);
  if constexpr (BE) {
    crypt.encrypt_big_endian(w.str().data(), size);
  } else {
    crypt.encrypt(w.str().data(), size);
  }
  return std::move(w.str());
}

struct DecryptedPR2 {
  std::string compressed_data;
  size_t decompressed_size;
};

template <bool BE>
DecryptedPR2 decrypt_pr2_data(const std::string& data) {
  if (data.size() < 8) {
    throw std::runtime_error("not enough data for PR2 header");
  }
  phosg::StringReader r(data);
  DecryptedPR2 ret = {.compressed_data = data.substr(8), .decompressed_size = r.get<U32T<BE>>()};
  PSOV2Encryption crypt(r.get<U32T<BE>>());
  if (BE) {
    crypt.encrypt_big_endian(ret.compressed_data.data(), ret.compressed_data.size());
  } else {
    crypt.decrypt(ret.compressed_data.data(), ret.compressed_data.size());
  }
  return ret;
}

template <bool BE>
std::string decrypt_and_decompress_pr2_data(const std::string& data) {
  auto decrypted = decrypt_pr2_data<BE>(data);
  std::string decompressed = prs_decompress(decrypted.compressed_data);
  if (decompressed.size() != decrypted.decompressed_size) {
    throw std::runtime_error("decompressed size does not match expected size");
  }
  return decompressed;
}

template <bool BE>
std::string encrypt_pr2_data(const std::string& data, size_t decompressed_size, uint32_t seed) {
  phosg::StringWriter w;
  w.put<U32T<BE>>(decompressed_size);
  w.put<U32T<BE>>(seed);
  w.write(data);

  std::string ret = std::move(w.str());
  PSOV2Encryption crypt(seed);
  if (BE) {
    crypt.encrypt_big_endian(ret.data() + 8, ret.size() - 8);
  } else {
    crypt.decrypt(ret.data() + 8, ret.size() - 8);
  }
  return ret;
}
