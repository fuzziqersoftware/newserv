#include "PSOEncryption.hh"

#include <stdio.h>
#include <string.h>

#include <stdexcept>
#include <string>
#include <phosg/Encoding.hh>
#include <phosg/Strings.hh>

using namespace std;



// TODO: fix style in this file, especially in psobb functions



// Most ciphers used by PSO are symmetric; alias decrypt to encrypt by default
void PSOEncryption::decrypt(void* data, size_t size, bool advance) {
  this->encrypt(data, size, advance);
}



void PSOPCEncryption::update_stream() {
  uint32_t esi, edi, eax, ebp, edx;
  edi = 1;
  edx = 0x18;
  eax = edi;
  while (edx > 0) {
    esi = this->stream[eax + 0x1F];
    ebp = this->stream[eax] - esi;
    this->stream[eax] = ebp;
    eax++;
    edx--;
  }
  edi = 0x19;
  edx = 0x1F;
  eax = edi;
  while (edx > 0) {
    esi = this->stream[eax - 0x18];
    ebp = this->stream[eax] - esi;
    this->stream[eax] = ebp;
    eax++;
    edx--;
  }
}

PSOPCEncryption::PSOPCEncryption(uint32_t seed) : offset(1) {
  uint32_t esi, ebx, edi, eax, edx, var1;
  esi = 1;
  ebx = seed;
  edi = 0x15;
  this->stream[56] = ebx;
  this->stream[55] = ebx;
  while (edi <= 0x46E) {
    eax = edi;
    var1 = eax / 55;
    edx = eax - (var1 * 55);
    ebx = ebx - esi;
    edi = edi + 0x15;
    this->stream[edx] = esi;
    esi = ebx;
    ebx = this->stream[edx];
  }
  for (size_t x = 0; x < 5; x++) {
    this->update_stream();
  }
}

uint32_t PSOPCEncryption::next(bool advance) {
  if (this->offset == PC_STREAM_LENGTH) {
    this->update_stream();
    this->offset = 1;
  }
  uint32_t ret = this->stream[this->offset];
  if (advance) {
    this->offset++;
  }
  return ret;
}

void PSOPCEncryption::encrypt(void* vdata, size_t size, bool advance) {
  if (size & 3) {
    throw invalid_argument("size must be a multiple of 4");
  }
  if (!advance && (size != 4)) {
    throw logic_error("cannot peek-encrypt/decrypt with size > 4");
  }
  size >>= 2;

  le_uint32_t* data = reinterpret_cast<le_uint32_t*>(vdata);
  for (size_t x = 0; x < size; x++) {
    data[x] ^= this->next(advance);
  }
}



void PSOGCEncryption::update_stream() {
  uint32_t r5, r6, r7;
  r5 = 0;
  r6 = 489;
  r7 = 0;

  while (r6 != GC_STREAM_LENGTH) {
    this->stream[r5++] ^= this->stream[r6++];
  }

  while (r5 != GC_STREAM_LENGTH) {
    this->stream[r5++] ^= this->stream[r7++];
  }

  this->offset = 0;
}

uint32_t PSOGCEncryption::next(bool advance) {
  if (this->offset == GC_STREAM_LENGTH) {
    this->update_stream();
  }
  uint32_t ret = this->stream[this->offset];
  if (advance) {
    this->offset++;
  }
  return ret;
}

PSOGCEncryption::PSOGCEncryption(uint32_t seed) : offset(0) {
  uint32_t x, y, basekey, source1, source2, source3;
  basekey = 0;

  for (x = 0; x <= 16; x++) {
    for (y = 0; y < 32; y++) {
      seed = seed * 0x5D588B65;
      basekey = basekey >> 1;
      seed++;
      if (seed & 0x80000000) {
        basekey = basekey | 0x80000000;
      } else {
        basekey = basekey & 0x7FFFFFFF;
      }
    }
    this->stream[this->offset++] = basekey;
  }

  this->stream[this->offset - 1] = (((this->stream[0] >> 9) ^ (this->stream[this->offset - 1] << 23)) ^ this->stream[15]);

  source1 = 0;
  source2 = 1;
  source3 = this->offset - 1;
  while (this->offset != GC_STREAM_LENGTH) {
    this->stream[this->offset++] = (this->stream[source3++] ^ (((this->stream[source1++] << 23) & 0xFF800000) ^ ((this->stream[source2++] >> 9) & 0x007FFFFF)));
  }

  for (size_t x = 0; x < 4; x++) {
    this->update_stream();
  }
}

void PSOGCEncryption::encrypt(void* vdata, size_t size, bool advance) {
  if (size & 3) {
    throw invalid_argument("size must be a multiple of 4");
  }
  if (!advance && (size != 4)) {
    throw logic_error("cannot peek-encrypt/decrypt with size > 4");
  }
  size >>= 2;

  le_uint32_t* data = reinterpret_cast<le_uint32_t*>(vdata);
  for (size_t x = 0; x < size; x++) {
    data[x] ^= this->next(advance);
  }
}



void PSOBBEncryption::decrypt(void* vdata, size_t size, bool advance) {
  if (this->subtype != Subtype::JSD1) {
    if (size & 7) {
      throw invalid_argument("size must be a multiple of 8");
    }
    size_t num_dwords = size >> 2;
    le_uint32_t* dwords = reinterpret_cast<le_uint32_t*>(vdata);
    uint32_t edx, ebx, ebp, esi, edi;

    edx = 0;
    while (edx < num_dwords) {
      ebx = dwords[edx];
      ebx = ebx ^ this->stream[5];
      ebp = ((this->stream[(ebx >> 0x18) + 0x12] +
              this->stream[((ebx >> 0x10) & 0xFF) + 0x112]) ^
             this->stream[((ebx >> 0x8) & 0xFF) + 0x212]) +
            this->stream[(ebx & 0xFF) + 0x312];
      ebp = ebp ^ this->stream[4];
      ebp ^= dwords[edx + 1];
      edi = ((this->stream[(ebp >> 0x18) + 0x12] +
              this->stream[((ebp >> 0x10) & 0xFF) + 0x112]) ^
             this->stream[((ebp >> 0x8) & 0xFF) + 0x212]) +
            this->stream[(ebp & 0xFF) + 0x312];
      edi = edi ^ this->stream[3];
      ebx = ebx ^ edi;
      esi = ((this->stream[(ebx >> 0x18) + 0x12] +
              this->stream[((ebx >> 0x10) & 0xFF) + 0x112]) ^
             this->stream[((ebx >> 0x8) & 0xFF) + 0x212]) +
            this->stream[(ebx & 0xFF) + 0x312];
      ebp = ebp ^ esi ^ this->stream[2];
      edi = ((this->stream[(ebp >> 0x18) + 0x12] +
              this->stream[((ebp >> 0x10) & 0xFF) + 0x112]) ^
             this->stream[((ebp >> 0x8) & 0xFF) + 0x212]) +
            this->stream[(ebp & 0xFF) + 0x312];
      edi = edi ^ this->stream[1];
      ebp = ebp ^ this->stream[0];
      ebx = ebx ^ edi;
      dwords[edx] = ebp;
      dwords[edx + 1] = ebx;
      edx += 2;
    }

  } else { // subtype == Subtype::JSD1
    if (!advance && (size > 0x100)) {
      throw logic_error("JSD1 can only peek-decrypt up to 0x100 bytes");
    }
    uint8_t* bytes = reinterpret_cast<uint8_t*>(vdata);
    for (size_t z = 0; z < size; z += 2) {
      uint8_t a = bytes[z];
      uint8_t b = bytes[z + 1];
      bytes[z] = (a & 0x55) | (b & 0xAA);
      bytes[z + 1] = (a & 0xAA) | (b & 0x55);
    }
    uint8_t* stream_bytes = reinterpret_cast<uint8_t*>(this->stream.data());
    for (size_t z = 0; z < size; z++) {
      bytes[z] ^= stream_bytes[this->jsd1_stream_offset];
      if (advance) {
        stream_bytes[this->jsd1_stream_offset] -= bytes[z];
      }
      this->jsd1_stream_offset++;
    }
    if (!advance) {
      this->jsd1_stream_offset -= size;
    }
  }
}

void PSOBBEncryption::encrypt(void* vdata, size_t size, bool advance) {
  if (this->subtype != Subtype::JSD1) {
    if (size & 7) {
      throw invalid_argument("size must be a multiple of 8");
    }

    size_t num_dwords = size >> 2;
    le_uint32_t* data = reinterpret_cast<le_uint32_t*>(vdata);
    uint32_t edx, ebx, ebp, esi, edi;

    edx = 0;
    while (edx < num_dwords) {
      ebx = data[edx] ^ this->stream[0];
      ebp = ((this->stream[(ebx >> 0x18) + 0x12] + this->stream[((ebx >> 0x10) & 0xFF) + 0x112])
          ^ this->stream[((ebx >> 0x8) & 0xFF) + 0x212]) + this->stream[(ebx & 0xFF) + 0x312];
      ebp = ebp ^ this->stream[1];
      ebp ^= data[edx + 1];
      edi = ((this->stream[(ebp >> 0x18) + 0x12] + this->stream[((ebp >> 0x10) & 0xFF) + 0x112])
          ^ this->stream[((ebp >> 0x8) & 0xFF) + 0x212]) + this->stream[(ebp & 0xFF) + 0x312];
      edi = edi ^ this->stream[2];
      ebx = ebx ^ edi;
      esi = ((this->stream[(ebx >> 0x18) + 0x12] + this->stream[((ebx >> 0x10) & 0xFF) + 0x112])
          ^ this->stream[((ebx >> 0x8) & 0xFF) + 0x212]) + this->stream[(ebx & 0xFF) + 0x312];
      ebp = ebp ^ esi ^ this->stream[3];
      edi = ((this->stream[(ebp >> 0x18) + 0x12] + this->stream[((ebp >> 0x10) & 0xFF) + 0x112])
          ^ this->stream[((ebp >> 0x8) & 0xFF) + 0x212]) + this->stream[(ebp & 0xFF) + 0x312];
      edi = edi ^ this->stream[4];
      ebp = ebp ^ this->stream[5];
      ebx = ebx ^ edi;
      data[edx] = ebp;
      data[edx + 1] = ebx;
      edx += 2;
    }

  } else { // subtype == Subtype::JSD1
    if (!advance && (size > 0x100)) {
      throw logic_error("JSD1 can only peek-encrypt up to 0x100 bytes");
    }
    uint8_t* bytes = reinterpret_cast<uint8_t*>(vdata);
    uint8_t* stream_bytes = reinterpret_cast<uint8_t*>(this->stream.data());
    for (size_t z = 0; z < size; z++) {
      uint8_t v = bytes[z];
      bytes[z] = v ^ stream_bytes[this->jsd1_stream_offset];
      if (advance) {
        stream_bytes[this->jsd1_stream_offset] -= v;
      }
      this->jsd1_stream_offset++;
    }
    if (!advance) {
      this->jsd1_stream_offset -= size;
    }
    for (size_t z = 0; z < size; z += 2) {
      uint8_t a = bytes[z];
      uint8_t b = bytes[z + 1];
      bytes[z] = (a & 0x55) | (b & 0xAA);
      bytes[z + 1] = (a & 0xAA) | (b & 0x55);
    }
  }
}

PSOBBEncryption::PSOBBEncryption(
    const KeyFile& key, const void* original_seed, size_t seed_size)
  : subtype(key.subtype), jsd1_stream_offset(0) {

  // Note: This part is done in the 03 command handler in the BB client, and
  // isn't actually part of the encryption library. (Why did they do this?)
  string seed;
  const uint8_t* original_seed_data = reinterpret_cast<const uint8_t*>(
      original_seed);
  for (size_t x = 0; x < seed_size; x += 3) {
    seed.push_back(original_seed_data[x] ^ 0x19);
    seed.push_back(original_seed_data[x + 1] ^ 0x16);
    seed.push_back(original_seed_data[x + 2] ^ 0x18);
  }

  if (this->subtype != Subtype::JSD1) {
    if (seed_size % 3) {
      throw invalid_argument("seed size must be divisible by 3");
    }

    this->stream.resize(BB_STREAM_LENGTH, 0);

    if (key.subtype == Subtype::MOCB1) {
      for (size_t x = 0; x < 0x12; x++) {
        uint8_t a = key.initial_keys[4 * x + 0];
        uint8_t b = key.initial_keys[4 * x + 1];
        uint8_t c = key.initial_keys[4 * x + 2];
        uint8_t d = key.initial_keys[4 * x + 3];
        this->stream[x] = ((a ^ d) << 24) | ((b ^ c) << 16) | (a << 8) | b;
      }
      memcpy(this->stream.data() + 0x12, &key.private_keys, sizeof(key.private_keys));

    } else {
      memcpy(this->stream.data(), &key, sizeof(key));
    }

    // This block was formerly postprocess_initial_stream
    {
      uint32_t eax, ecx, edx, ebx, ebp, esi, edi, ou, x;

      ecx = 0;
      ebx = 0;

      while (ebx < 0x12) {
        ebp = static_cast<uint32_t>(seed[ecx]) << 0x18;
        eax = ecx + 1;
        edx = eax % seed.size();
        eax = (static_cast<uint32_t>(seed[edx]) << 0x10) & 0x00FF0000;
        ebp = (ebp | eax) & 0xFFFF00FF;
        eax = ecx + 2;
        edx = eax % seed.size();
        eax = (static_cast<uint32_t>(seed[edx]) << 0x08) & 0x0000FF00;
        ebp = (ebp | eax) & 0xFFFFFF00;
        eax = ecx + 3;
        ecx = ecx + 4;
        edx = eax % seed.size();
        eax = static_cast<uint32_t>(seed[edx]) & 0x000000FF;
        ebp = ebp | eax;
        eax = ecx;
        edx = eax % seed.size();
        this->stream[ebx] ^= ebp;
        ecx = edx;
        ebx++;
      }

      ebp = 0;
      esi = 0;
      ecx = 0;
      edi = 0;
      ebx = 0;
      edx = 0x48;

      while (edi < edx) {
        esi = esi ^ this->stream[0];
        eax = esi >> 0x18;
        ebx = (esi >> 0x10) & 0xFF;
        eax = this->stream[eax + 0x12] + this->stream[ebx + 0x112];
        ebx = (esi >> 8) & 0xFF;
        eax = eax ^ this->stream[ebx + 0x212];
        ebx = esi & 0xFF;
        eax = eax + this->stream[ebx + 0x312];

        eax = eax ^ this->stream[1];
        ecx = ecx ^ eax;
        ebx = ecx >> 0x18;
        eax = (ecx >> 0x10) & 0xFF;
        ebx = this->stream[ebx + 0x12] + this->stream[eax + 0x112];
        eax = (ecx >> 8) & 0xFF;
        ebx = ebx ^ this->stream[eax + 0x212];
        eax = ecx & 0xFF;
        ebx = ebx + this->stream[eax + 0x312];

        for (x = 0; x <= 5; x++) {
          ebx = ebx ^ this->stream[(x * 2) + 2];
          esi = esi ^ ebx;
          ebx = esi >> 0x18;
          eax = (esi >> 0x10) & 0xFF;
          ebx = this->stream[ebx + 0x12] + this->stream[eax + 0x112];
          eax = (esi >> 8) & 0xFF;
          ebx = ebx ^ this->stream[eax + 0x212];
          eax = esi & 0xFF;
          ebx = ebx + this->stream[eax + 0x312];

          ebx = ebx ^ this->stream[(x * 2) + 3];
          ecx = ecx ^ ebx;
          ebx = ecx >> 0x18;
          eax = (ecx >> 0x10) & 0xFF;
          ebx = this->stream[ebx + 0x12] + this->stream[eax + 0x112];
          eax = (ecx >> 8) & 0xFF;
          ebx = ebx ^ this->stream[eax + 0x212];
          eax = ecx & 0xFF;
          ebx = ebx + this->stream[eax + 0x312];
        }

        ebx = ebx ^ this->stream[14];
        esi = esi ^ ebx;
        eax = esi >> 0x18;
        ebx = (esi >> 0x10) & 0xFF;
        eax = this->stream[eax + 0x12] + this->stream[ebx + 0x112];
        ebx = (esi >> 8) & 0xFF;
        eax = eax ^ this->stream[ebx + 0x212];
        ebx = esi & 0xFF;
        eax = eax + this->stream[ebx + 0x312];

        eax = eax ^ this->stream[15];
        eax = ecx ^ eax;
        ecx = eax >> 0x18;
        ebx = (eax >> 0x10) & 0xFF;
        ecx = this->stream[ecx + 0x12] + this->stream[ebx + 0x112];
        ebx = (eax >> 8) & 0xFF;
        ecx = ecx ^ this->stream[ebx + 0x212];
        ebx = eax & 0xFF;
        ecx = ecx + this->stream[ebx + 0x312];

        ecx = ecx ^ this->stream[16];
        ecx = ecx ^ esi;
        esi = this->stream[17];
        esi = esi ^ eax;
        this->stream[(edi / 4)] = esi;
        this->stream[(edi / 4)+1] = ecx;
        edi = edi + 8;
      }

      eax = 0;
      edx = 0;
      ou = 0;
      while (ou < 0x1000) {
        edi = 0x48;
        edx = 0x448;

        while (edi < edx) {
          esi = esi ^ this->stream[0];
          eax = esi >> 0x18;
          ebx = (esi >> 0x10) & 0xFF;
          eax = this->stream[eax + 0x12] + this->stream[ebx + 0x112];
          ebx = (esi >> 8) & 0xFF;
          eax = eax ^ this->stream[ebx + 0x212];
          ebx = esi & 0xFF;
          eax = eax + this->stream[ebx + 0x312];

          eax = eax ^ this->stream[1];
          ecx = ecx ^ eax;
          ebx = ecx >> 0x18;
          eax = (ecx >> 0x10) & 0xFF;
          ebx = this->stream[ebx + 0x12] + this->stream[eax + 0x112];
          eax = (ecx >> 8) & 0xFF;
          ebx = ebx ^ this->stream[eax + 0x212];
          eax = ecx & 0xFF;
          ebx = ebx + this->stream[eax + 0x312];

          for (x = 0; x <= 5; x++) {
            ebx = ebx ^ this->stream[(x * 2) + 2];
            esi = esi ^ ebx;
            ebx = esi >> 0x18;
            eax = (esi >> 0x10) & 0xFF;
            ebx = this->stream[ebx + 0x12] + this->stream[eax + 0x112];
            eax = (esi >> 8) & 0xFF;
            ebx = ebx ^ this->stream[eax + 0x212];
            eax = esi & 0xFF;
            ebx = ebx + this->stream[eax + 0x312];

            ebx = ebx ^ this->stream[(x * 2) + 3];
            ecx = ecx ^ ebx;
            ebx = ecx >> 0x18;
            eax = (ecx >> 0x10) & 0xFF;
            ebx = this->stream[ebx + 0x12] + this->stream[eax + 0x112];
            eax = (ecx >> 8) & 0xFF;
            ebx = ebx ^ this->stream[eax + 0x212];
            eax = ecx & 0xFF;
            ebx = ebx + this->stream[eax + 0x312];
          }

          ebx = ebx ^ this->stream[14];
          esi = esi ^ ebx;
          eax = esi >> 0x18;
          ebx = (esi >> 0x10) & 0xFF;
          eax = this->stream[eax + 0x12] + this->stream[ebx + 0x112];
          ebx = (esi >> 8) & 0xFF;
          eax = eax ^ this->stream[ebx + 0x212];
          ebx = esi & 0xFF;
          eax = eax + this->stream[ebx + 0x312];

          eax = eax ^ this->stream[15];
          eax = ecx ^ eax;
          ecx = eax >> 0x18;
          ebx = (eax >> 0x10) & 0xFF;
          ecx = this->stream[ecx + 0x12] + this->stream[ebx + 0x112];
          ebx = (eax >> 8) & 0xFF;
          ecx = ecx ^ this->stream[ebx + 0x212];
          ebx = eax & 0xFF;
          ecx = ecx + this->stream[ebx + 0x312];

          ecx = ecx ^ this->stream[16];
          ecx = ecx ^ esi;
          esi = this->stream[17];
          esi = esi ^ eax;
          this->stream[(ou / 4) + (edi / 4)] = esi;
          this->stream[(ou / 4) + (edi / 4) + 1] = ecx;
          edi = edi + 8;
        }
        ou = ou + 0x400;
      }
    }

  } else { // subtype == Subtype::JSD1
    this->stream.resize(0x40);
    uint8_t* stream_bytes = reinterpret_cast<uint8_t*>(this->stream.data());
    size_t seed_offset = 0;
    for (size_t z = 0; z < 0x100; z++) {
      stream_bytes[z] = (z + seed[seed_offset]) ^ (static_cast<uint8_t>(seed[seed_offset]) >> 1);
      seed_offset = (seed_offset + 1) % seed.size();
    }
  }
}



PSOBBMultiKeyDetectorEncryption::PSOBBMultiKeyDetectorEncryption(
    const vector<shared_ptr<const PSOBBEncryption::KeyFile>>& possible_keys,
    const string& expected_first_data,
    const void* seed,
    size_t seed_size)
  : possible_keys(possible_keys),
    expected_first_data(expected_first_data),
    seed(reinterpret_cast<const char*>(seed), seed_size) { }

void PSOBBMultiKeyDetectorEncryption::encrypt(void* data, size_t size, bool advance) {
  if (!this->active_crypt.get()) {
    throw logic_error("PSOBB multi-key encryption requires client input first");
  }
  this->active_crypt->encrypt(data, size, advance);
}

void PSOBBMultiKeyDetectorEncryption::decrypt(void* data, size_t size, bool advance) {
  if (!this->active_crypt.get()) {
    if (size != this->expected_first_data.size()) {
      throw logic_error("initial decryption size does not match expected first data size");
    }

    for (const auto& key : this->possible_keys) {
      this->active_key = key;
      this->active_crypt.reset(new PSOBBEncryption(
          *this->active_key, this->seed.data(), this->seed.size()));
      string test_data(reinterpret_cast<const char*>(data), size);
      this->active_crypt->decrypt(test_data.data(), test_data.size(), false);
      if (test_data == this->expected_first_data) {
        break;
      }
      this->active_key.reset();
      this->active_crypt.reset();
    }
    if (!this->active_crypt.get()) {
      throw runtime_error("none of the registered private keys are valid for this client");
    }
  }
  this->active_crypt->decrypt(data, size, advance);
}



PSOBBMultiKeyImitatorEncryption::PSOBBMultiKeyImitatorEncryption(
    shared_ptr<const PSOBBMultiKeyDetectorEncryption> detector_crypt,
    const void* seed,
    size_t seed_size,
    bool jsd1_use_detector_seed)
  : detector_crypt(detector_crypt),
    seed(reinterpret_cast<const char*>(seed), seed_size),
    jsd1_use_detector_seed(jsd1_use_detector_seed) { }

void PSOBBMultiKeyImitatorEncryption::encrypt(void* data, size_t size, bool advance) {
  this->ensure_crypt()->encrypt(data, size, advance);
}

void PSOBBMultiKeyImitatorEncryption::decrypt(void* data, size_t size, bool advance) {
  this->ensure_crypt()->decrypt(data, size, advance);
}

shared_ptr<PSOBBEncryption> PSOBBMultiKeyImitatorEncryption::ensure_crypt() {
  if (!this->active_crypt.get()) {
    auto key = this->detector_crypt->get_active_key();
    if (!key.get()) {
      throw logic_error("server crypt cannot be initialized because client crypt is not ready");
    }
    // Hack: JSD1 uses the client seed for both ends of the connection and
    // ignores the server seed (though each end has its own state after that).
    // To handle this, we use the other crypt's seed if the type is JSD1.
    if ((key->subtype == PSOBBEncryption::Subtype::JSD1) && this->jsd1_use_detector_seed) {
      const auto& detector_seed = this->detector_crypt->get_seed();
      this->active_crypt.reset(new PSOBBEncryption(
          *key, detector_seed.data(), detector_seed.size()));
    } else {
      this->active_crypt.reset(new PSOBBEncryption(
          *key, this->seed.data(), this->seed.size()));
    }
  }
  return this->active_crypt;
}
