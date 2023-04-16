#pragma once

#include <inttypes.h>
#include <stddef.h>

#include <memory>
#include <phosg/Encoding.hh>
#include <string>
#include <vector>

#include "Text.hh" // for parray

class PSOEncryption {
public:
  enum class Type {
    V2 = 0,
    V3,
    BB,
    JSD0,
  };

  virtual ~PSOEncryption() = default;

  virtual void encrypt(void* data, size_t size, bool advance = true) = 0;
  virtual void decrypt(void* data, size_t size, bool advance = true);

  inline void encrypt(std::string& data, bool advance = true) {
    this->encrypt(data.data(), data.size(), advance);
  }
  inline void decrypt(std::string& data, bool advance = true) {
    this->decrypt(data.data(), data.size(), advance);
  }

  virtual Type type() const = 0;

protected:
  PSOEncryption() = default;
};

class PSOLFGEncryption : public PSOEncryption {
public:
  virtual void encrypt(void* data, size_t size, bool advance = true);
  void encrypt_big_endian(void* data, size_t size, bool advance = true);
  void encrypt_minus(void* data, size_t size, bool advance = true);
  void encrypt_big_endian_minus(void* data, size_t size, bool advance = true);
  void encrypt_both_endian(void* le_data, void* be_data, size_t size, bool advance = true);

  template <bool IsBigEndian>
  void encrypt_t(void* data, size_t size, bool advance = true);
  template <bool IsBigEndian>
  void encrypt_minus_t(void* data, size_t size, bool advance = true);

  uint32_t next(bool advance = true);

protected:
  explicit PSOLFGEncryption(uint32_t seed, size_t stream_length, size_t end_offset);

  virtual void update_stream() = 0;

  std::vector<uint32_t> stream;
  size_t offset;
  size_t end_offset;
  uint32_t seed;
};

class PSOV2Encryption : public PSOLFGEncryption {
public:
  explicit PSOV2Encryption(uint32_t seed);

  virtual Type type() const;

protected:
  virtual void update_stream();

  static constexpr size_t STREAM_LENGTH = 56;
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
    // initial_keys are actually a stream of uint32_ts, but we treat them as
    // bytes for code simplicity
    union InitialKeys {
      uint8_t jsd1_stream_offset;
      parray<uint8_t, 0x48> as8;
      parray<le_uint32_t, 0x12> as32;
      InitialKeys() : as32() {}
      InitialKeys(const InitialKeys& other) : as32(other.as32) {}
    } __attribute__((packed));
    union PrivateKeys {
      parray<uint8_t, 0x1000> as8;
      parray<le_uint32_t, 0x400> as32;
      PrivateKeys() : as32() {}
      PrivateKeys(const PrivateKeys& other) : as32(other.as32) {}
    } __attribute__((packed));
    InitialKeys initial_keys;
    PrivateKeys private_keys;
    Subtype subtype;
  } __attribute__((packed));

  PSOBBEncryption(const KeyFile& key, const void* seed, size_t seed_size);

  virtual void encrypt(void* data, size_t size, bool advance = true);
  virtual void decrypt(void* data, size_t size, bool advance = true);

  virtual Type type() const;

protected:
  KeyFile state;

  void tfs1_scramble(uint32_t* out1, uint32_t* out2) const;
  void apply_seed(const void* original_seed, size_t seed_size);
};

// The following classes provide support for automatically detecting which type
// of encryption a client is using based on their initial response to the server

class PSOV2OrV3DetectorEncryption : public PSOEncryption {
public:
  PSOV2OrV3DetectorEncryption(
      uint32_t key,
      const std::unordered_set<uint32_t>& v2_matches,
      const std::unordered_set<uint32_t>& v3_matches);

  virtual void encrypt(void* data, size_t size, bool advance = true);

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

  virtual void encrypt(void* data, size_t size, bool advance = true);

  virtual Type type() const;

protected:
  uint32_t key;
  std::shared_ptr<const PSOV2OrV3DetectorEncryption> detector_crypt;
  std::shared_ptr<PSOEncryption> active_crypt;
};

// The following classes provide support for multiple PSOBB private keys, and
// the ability to automatically detect which key the client is using based on
// the first 8 bytes they send

class PSOBBMultiKeyDetectorEncryption : public PSOEncryption {
public:
  PSOBBMultiKeyDetectorEncryption(
      const std::vector<std::shared_ptr<const PSOBBEncryption::KeyFile>>& possible_keys,
      const std::unordered_set<std::string>& expected_first_data,
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

  virtual void encrypt(void* data, size_t size, bool advance = true);
  virtual void decrypt(void* data, size_t size, bool advance = true);

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

  virtual void encrypt(void* data, size_t size, bool advance = true);
  virtual void decrypt(void* data, size_t size, bool advance = true);

  virtual Type type() const = 0;

private:
  uint8_t key;
};

void decrypt_trivial_gci_data(void* data, size_t size, uint8_t basis);
