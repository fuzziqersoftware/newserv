#include <inttypes.h>
#include <stddef.h>



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

private:
  void update_stream();
  uint32_t next();

  uint32_t stream[PC_STREAM_LENGTH];
  uint16_t offset;
};

class PSOGCEncryption : public PSOEncryption {
public:
  explicit PSOGCEncryption(uint32_t key);

  virtual void encrypt(void* data, size_t size);

private:
  void update_stream();
  uint32_t next();

  uint32_t stream[GC_STREAM_LENGTH];
  uint16_t offset;
};

class PSOBBEncryption : public PSOEncryption {
public:
  explicit PSOBBEncryption(const void* key);

  virtual void encrypt(void* data, size_t size);
  virtual void decrypt(void* data, size_t size);

private:
  void update_stream();

  uint32_t stream[BB_STREAM_LENGTH];
  uint16_t offset;
};
