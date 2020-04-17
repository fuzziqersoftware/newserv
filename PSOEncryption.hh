#pragma once

#include <inttypes.h>
#include <stddef.h>

#include <string>



#define PC_STREAM_LENGTH 57
#define GC_STREAM_LENGTH 521
#define BB_STREAM_LENGTH 1042

class PSOEncryption {
public:
  virtual ~PSOEncryption() = default;

  virtual void encrypt(void* data, size_t size) = 0;
  virtual void decrypt(void* data, size_t size);

protected:
  PSOEncryption() = default;
};

class PSOPCEncryption : public PSOEncryption {
public:
  explicit PSOPCEncryption(uint32_t seed);

  virtual void encrypt(void* data, size_t size);

protected:
  void update_stream();
  uint32_t next();

  uint32_t stream[PC_STREAM_LENGTH];
  uint16_t offset;
};

class PSOGCEncryption : public PSOEncryption {
public:
  explicit PSOGCEncryption(uint32_t key);

  virtual void encrypt(void* data, size_t size);

protected:
  void update_stream();
  uint32_t next();

  uint32_t stream[GC_STREAM_LENGTH];
  uint16_t offset;
};

class PSOBBEncryption : public PSOEncryption {
public:
  struct KeyFile {
    uint32_t initial_keys[18];
    uint32_t private_keys[1024];
  };

  PSOBBEncryption(const KeyFile& key, const void* seed, size_t seed_size);

  virtual void encrypt(void* data, size_t size);
  virtual void decrypt(void* data, size_t size);

protected:
  PSOBBEncryption() = default;

  void postprocess_initial_stream(const std::string& seed);

  void update_stream();

  uint32_t stream[BB_STREAM_LENGTH];
  uint16_t offset;
};
