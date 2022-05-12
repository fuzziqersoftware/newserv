#pragma once

#include <inttypes.h>
#include <stddef.h>

#include <memory>
#include <string>
#include <vector>
#include <phosg/Encoding.hh>

#include "Text.hh" // for parray



#define PC_STREAM_LENGTH 56
#define GC_STREAM_LENGTH 521
#define BB_STREAM_LENGTH 1042

class PSOEncryption {
public:
  virtual ~PSOEncryption() = default;

  virtual void encrypt(void* data, size_t size, bool advance = true) = 0;
  virtual void decrypt(void* data, size_t size, bool advance = true);

  inline void encrypt(std::string& data, bool advance = true) {
    this->encrypt(data.data(), data.size(), advance);
  }
  inline void decrypt(std::string& data, bool advance = true) {
    this->decrypt(data.data(), data.size(), advance);
  }

protected:
  PSOEncryption() = default;
};

class PSOPCEncryption : public PSOEncryption {
public:
  explicit PSOPCEncryption(uint32_t seed);

  virtual void encrypt(void* data, size_t size, bool advance = true);

protected:
  void update_stream();
  uint32_t next(bool advance = true);

  uint32_t stream[PC_STREAM_LENGTH + 1];
  uint8_t offset;
};

class PSOGCEncryption : public PSOEncryption {
public:
  explicit PSOGCEncryption(uint32_t key);

  virtual void encrypt(void* data, size_t size, bool advance = true);

protected:
  void update_stream();
  uint32_t next(bool advance = true);

  uint32_t stream[GC_STREAM_LENGTH];
  uint16_t offset;
};

class PSOBBEncryption : public PSOEncryption {
public:
  enum Subtype : uint8_t {
    STANDARD = 0x00,
    MOCB1 = 0x01,
    JSD1 = 0x02,
  };

  struct KeyFile {
    // initial_keys are actually a stream of uint32_ts, but we treat them as
    // bytes for code simplicity
    union InitialKeys {
      uint8_t jsd1_stream_offset;
      parray<uint8_t, 0x48> as8;
      parray<le_uint32_t, 0x12> as32;
      InitialKeys() : as32() { }
      InitialKeys(const InitialKeys& other) : as32(other.as32) { }
    } __attribute__((packed));
    union PrivateKeys {
      parray<uint8_t, 0x1000> as8;
      parray<le_uint32_t, 0x400> as32;
      PrivateKeys() : as32() { }
      PrivateKeys(const PrivateKeys& other) : as32(other.as32) { }
    } __attribute__((packed));
    InitialKeys initial_keys;
    PrivateKeys private_keys;
    Subtype subtype;
  } __attribute__((packed));

  PSOBBEncryption(const KeyFile& key, const void* seed, size_t seed_size);

  virtual void encrypt(void* data, size_t size, bool advance = true);
  virtual void decrypt(void* data, size_t size, bool advance = true);

protected:
  KeyFile state;

  void apply_seed(const void* original_seed, size_t seed_size);
};

// The following classes provide support for multiple PSOBB private keys, and
// the ability to automatically detect which key the client is using based on
// the first 8 bytes they send.

class PSOBBMultiKeyDetectorEncryption : public PSOEncryption {
public:
  PSOBBMultiKeyDetectorEncryption(
      const std::vector<std::shared_ptr<const PSOBBEncryption::KeyFile>>& possible_keys,
      const std::string& expected_first_data,
      const void* seed,
      size_t seed_size);

  virtual void encrypt(void* data, size_t size, bool advance = true);
  virtual void decrypt(void* data, size_t size, bool advance = true);

  inline std::shared_ptr<const PSOBBEncryption::KeyFile> get_active_key() const {
    return this->active_key;
  }
  inline const std::string& get_seed() const {
    return this->seed;
  }

protected:
  std::vector<std::shared_ptr<const PSOBBEncryption::KeyFile>> possible_keys;
  std::shared_ptr<const PSOBBEncryption::KeyFile> active_key;
  std::shared_ptr<PSOBBEncryption> active_crypt;
  std::string expected_first_data;
  std::string seed;
};

class PSOBBMultiKeyImitatorEncryption : public PSOEncryption {
public:
  PSOBBMultiKeyImitatorEncryption(
      std::shared_ptr<const PSOBBMultiKeyDetectorEncryption> client_crypt,
      const void* seed,
      size_t seed_size,
      bool jsd1_use_detector_seed);

  virtual void encrypt(void* data, size_t size, bool advance = true);
  virtual void decrypt(void* data, size_t size, bool advance = true);

protected:
  std::shared_ptr<PSOBBEncryption> ensure_crypt();

  std::shared_ptr<const PSOBBMultiKeyDetectorEncryption> detector_crypt;
  std::shared_ptr<PSOBBEncryption> active_crypt;
  std::string seed;
  bool jsd1_use_detector_seed;
};
