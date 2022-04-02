#pragma once

#include <inttypes.h>
#include <stddef.h>

#include <string>
#include <vector>



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

  virtual void skip(size_t size) = 0;

protected:
  PSOEncryption() = default;
};

class PSOPCEncryption : public PSOEncryption {
public:
  explicit PSOPCEncryption(uint32_t seed);

  virtual void encrypt(void* data, size_t size, bool advance = true);
  virtual void skip(size_t size);

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
  virtual void skip(size_t size);

protected:
  void update_stream();
  uint32_t next(bool advance = true);

  uint32_t stream[GC_STREAM_LENGTH];
  uint16_t offset;
};

class PSOBBEncryption : public PSOEncryption {
public:
  struct KeyFile {
    uint32_t initial_keys[18];
    uint32_t private_keys[1024];
  } __attribute__((packed));

  PSOBBEncryption(const KeyFile& key, const void* seed, size_t seed_size);

  virtual void encrypt(void* data, size_t size, bool advance = true);
  virtual void decrypt(void* data, size_t size, bool advance = true);
  virtual void skip(size_t size);

protected:
  static std::vector<uint32_t> generate_stream(
      const KeyFile& key, const void* seed, size_t seed_size);

  const std::vector<uint32_t> stream;
};
