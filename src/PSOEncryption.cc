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



PSORC4Encryption::PSORC4Encryption(
    uint32_t seed, size_t stream_length, size_t end_offset)
  : stream(stream_length, 0), offset(0), end_offset(end_offset), seed(seed) { }

uint32_t PSORC4Encryption::next(bool advance) {
  if (this->offset == this->end_offset) {
    this->update_stream();
  }
  uint32_t ret = this->stream[this->offset];
  if (advance) {
    this->offset++;
  }
  return ret;
}

template <typename LongT>
void PSORC4Encryption::encrypt_t(void* vdata, size_t size, bool advance) {
  if (size & 3) {
    throw invalid_argument("size must be a multiple of 4");
  }
  if (!advance && (size != 4)) {
    throw logic_error("cannot peek-encrypt/decrypt with size > 4");
  }
  size >>= 2;

  LongT* data = reinterpret_cast<LongT*>(vdata);
  for (size_t x = 0; x < size; x++) {
    data[x] ^= this->next(advance);
  }
}

void PSORC4Encryption::encrypt(void* vdata, size_t size, bool advance) {
  this->encrypt_t<le_uint32_t>(vdata, size, advance);
}

void PSORC4Encryption::encrypt_big_endian(void* vdata, size_t size, bool advance) {
  this->encrypt_t<be_uint32_t>(vdata, size, advance);
}

void PSORC4Encryption::encrypt_both_endian(
    void* le_vdata, void* be_vdata, size_t size, bool advance) {
  if (size & 3) {
    throw invalid_argument("size must be a multiple of 4");
  }
  if (!advance && (size != 4)) {
    throw logic_error("cannot peek-encrypt/decrypt with size > 4");
  }
  size >>= 2;

  le_uint32_t* le_data = reinterpret_cast<le_uint32_t*>(le_vdata);
  be_uint32_t* be_data = reinterpret_cast<be_uint32_t*>(be_vdata);
  for (size_t x = 0; x < size; x++) {
    uint32_t key = this->next(advance);
    le_data[x] ^= key;
    be_data[x] ^= key;
  }
}



PSOV2Encryption::PSOV2Encryption(uint32_t seed)
  : PSORC4Encryption(seed, this->STREAM_LENGTH + 1, this->STREAM_LENGTH) {
  uint32_t esi, ebx, edi, eax, edx, var1;
  esi = 1;
  ebx = this->seed;
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

void PSOV2Encryption::update_stream() {
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
  this->offset = 1;
}

PSOEncryption::Type PSOV2Encryption::type() const {
  return Type::V2;
}



PSOV3Encryption::PSOV3Encryption(uint32_t seed)
  : PSORC4Encryption(seed, this->STREAM_LENGTH, this->STREAM_LENGTH) {
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
  while (this->offset != this->STREAM_LENGTH) {
    this->stream[this->offset++] = (this->stream[source3++] ^ (((this->stream[source1++] << 23) & 0xFF800000) ^ ((this->stream[source2++] >> 9) & 0x007FFFFF)));
  }

  for (size_t x = 0; x < 4; x++) {
    this->update_stream();
  }
}

void PSOV3Encryption::update_stream() {
  uint32_t r5, r6, r7;
  r5 = 0;
  r6 = 489;
  r7 = 0;

  while (r6 != this->STREAM_LENGTH) {
    this->stream[r5++] ^= this->stream[r6++];
  }

  while (r5 != this->STREAM_LENGTH) {
    this->stream[r5++] ^= this->stream[r7++];
  }

  this->offset = 0;
}

PSOEncryption::Type PSOV3Encryption::type() const {
  return Type::V3;
}



PSOBBEncryption::PSOBBEncryption(
    const KeyFile& key, const void* original_seed, size_t seed_size)
  : state(key) {
  this->apply_seed(original_seed, seed_size);
}

void PSOBBEncryption::encrypt(void* vdata, size_t size, bool advance) {
  if (this->state.subtype == Subtype::TFS1) {
    if (size & 7) {
      throw invalid_argument("size must be a multiple of 8");
    }

    le_uint32_t* dwords = reinterpret_cast<le_uint32_t*>(vdata);
    for (size_t x = 0; x < (size >> 2); x += 2) {
      for (size_t y = 0; y < 4; y += 2) {
        dwords[x] ^= this->state.initial_keys.as32[y];
        dwords[x + 1] ^= ((this->state.private_keys.as32[dwords[x] >> 24] +
                         this->state.private_keys.as32[((dwords[x] >> 16) & 0xFF) + 0x100]) ^
                        this->state.private_keys.as32[((dwords[x] >> 8) & 0xFF) + 0x200]) +
                       this->state.private_keys.as32[(dwords[x] & 0xFF) + 0x300];
        dwords[x + 1] ^= this->state.initial_keys.as32[y + 1];
        dwords[x] ^= ((this->state.private_keys.as32[dwords[x + 1] >> 24] +
                     this->state.private_keys.as32[(dwords[x + 1] >> 16 & 0xFF) + 0x100]) ^
                    this->state.private_keys.as32[(dwords[x + 1] >> 8 & 0xFF) + 0x200]) +
                   this->state.private_keys.as32[(dwords[x + 1] & 0xFF) + 0x300];
      }
      dwords[x] ^= this->state.initial_keys.as32[4];
      dwords[x + 1] ^= this->state.initial_keys.as32[5];

      uint32_t a = dwords[x];
      dwords[x] = dwords[x + 1];
      dwords[x + 1] = a;
    }

  } else if (this->state.subtype == Subtype::JSD1) {
    if (size & 1) {
      throw invalid_argument("size must be a multiple of 2");
    }
    if (!advance && (size > 0x100)) {
      throw logic_error("JSD1 can only peek-encrypt up to 0x100 bytes");
    }
    uint8_t* bytes = reinterpret_cast<uint8_t*>(vdata);
    for (size_t z = 0; z < size; z++) {
      uint8_t v = bytes[z];
      bytes[z] = v ^ this->state.private_keys.as8[this->state.initial_keys.jsd1_stream_offset];
      if (advance) {
        this->state.private_keys.as8[this->state.initial_keys.jsd1_stream_offset] -= v;
      }
      this->state.initial_keys.jsd1_stream_offset++;
    }
    if (!advance) {
      this->state.initial_keys.jsd1_stream_offset -= size;
    }
    for (size_t z = 0; z < size; z += 2) {
      uint8_t a = bytes[z];
      uint8_t b = bytes[z + 1];
      bytes[z] = (a & 0x55) | (b & 0xAA);
      bytes[z + 1] = (a & 0xAA) | (b & 0x55);
    }

  } else { // STANDARD or MOCB1
    if (size & 7) {
      throw invalid_argument("size must be a multiple of 8");
    }

    size_t num_dwords = size >> 2;
    le_uint32_t* data = reinterpret_cast<le_uint32_t*>(vdata);
    uint32_t edx, ebx, ebp, esi, edi;

    edx = 0;
    while (edx < num_dwords) {
      ebx = data[edx] ^ this->state.initial_keys.as32[0];
      ebp = ((this->state.private_keys.as32[(ebx >> 0x18)] +
              this->state.private_keys.as32[((ebx >> 0x10) & 0xFF) + 0x100]) ^
             this->state.private_keys.as32[((ebx >> 0x8) & 0xFF) + 0x200]) +
            this->state.private_keys.as32[(ebx & 0xFF) + 0x300];
      ebp = ebp ^ this->state.initial_keys.as32[1];
      ebp ^= data[edx + 1];
      edi = ((this->state.private_keys.as32[(ebp >> 0x18)] +
              this->state.private_keys.as32[((ebp >> 0x10) & 0xFF) + 0x100]) ^
             this->state.private_keys.as32[((ebp >> 0x8) & 0xFF) + 0x200]) +
            this->state.private_keys.as32[(ebp & 0xFF) + 0x300];
      edi = edi ^ this->state.initial_keys.as32[2];
      ebx = ebx ^ edi;
      esi = ((this->state.private_keys.as32[(ebx >> 0x18)] +
              this->state.private_keys.as32[((ebx >> 0x10) & 0xFF) + 0x100]) ^
             this->state.private_keys.as32[((ebx >> 0x8) & 0xFF) + 0x200]) +
            this->state.private_keys.as32[(ebx & 0xFF) + 0x300];
      ebp = ebp ^ esi ^ this->state.initial_keys.as32[3];
      edi = ((this->state.private_keys.as32[(ebp >> 0x18)] +
              this->state.private_keys.as32[((ebp >> 0x10) & 0xFF) + 0x100]) ^
             this->state.private_keys.as32[((ebp >> 0x8) & 0xFF) + 0x200]) +
            this->state.private_keys.as32[(ebp & 0xFF) + 0x300];
      edi = edi ^ this->state.initial_keys.as32[4];
      ebp = ebp ^ this->state.initial_keys.as32[5];
      ebx = ebx ^ edi;
      data[edx] = ebp;
      data[edx + 1] = ebx;
      edx += 2;
    }
  }
}

void PSOBBEncryption::decrypt(void* vdata, size_t size, bool advance) {
  if (this->state.subtype == Subtype::TFS1) {
    if (size & 7) {
      throw invalid_argument("size must be a multiple of 8");
    }

    le_uint32_t* dwords = reinterpret_cast<le_uint32_t*>(vdata);
    for (size_t x = 0; x < (size >> 2); x += 2) {
      for (size_t y = 4; y > 0; y -= 2) {
        dwords[x] = dwords[x] ^ this->state.initial_keys.as32[y + 1];
        dwords[x + 1] ^= ((this->state.private_keys.as32[dwords[x] >> 24] +
                             this->state.private_keys.as32[((dwords[x] >> 16) & 0xFF) + 0x100]) ^
                            this->state.private_keys.as32[((dwords[x] >> 8) & 0xFF) + 0x200]) +
                           this->state.private_keys.as32[(dwords[x] & 0xFF) + 0x300];
        dwords[x + 1] ^= this->state.initial_keys.as32[y];
        dwords[x] ^= ((this->state.private_keys.as32[dwords[x + 1] >> 24] +
                         this->state.private_keys.as32[((dwords[x + 1] >> 16) & 0xFF) + 0x100]) ^
                        this->state.private_keys.as32[((dwords[x + 1] >> 8) & 0xFF) + 0x200]) +
                       this->state.private_keys.as32[(dwords[x + 1] & 0xFF) + 0x300];
      }
      dwords[x] ^= this->state.initial_keys.as32[1];
      dwords[x + 1] ^= this->state.initial_keys.as32[0];

      uint32_t a = dwords[x];
      dwords[x] = dwords[x + 1];
      dwords[x + 1] = a;
    }

  } else if (this->state.subtype == Subtype::JSD1) {
    if (size & 1) {
      throw invalid_argument("size must be a multiple of 2");
    }
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
    for (size_t z = 0; z < size; z++) {
      bytes[z] ^= this->state.private_keys.as8[this->state.initial_keys.jsd1_stream_offset];
      if (advance) {
        this->state.private_keys.as8[this->state.initial_keys.jsd1_stream_offset] -= bytes[z];
      }
      this->state.initial_keys.jsd1_stream_offset++;
    }
    if (!advance) {
      this->state.initial_keys.jsd1_stream_offset -= size;
    }

  } else { // STANDARD or MOCB1
    if (size & 7) {
      throw invalid_argument("size must be a multiple of 8");
    }
    size_t num_dwords = size >> 2;
    le_uint32_t* dwords = reinterpret_cast<le_uint32_t*>(vdata);
    uint32_t edx, ebx, ebp, esi, edi;

    edx = 0;
    while (edx < num_dwords) {
      ebx = dwords[edx];
      ebx = ebx ^ this->state.initial_keys.as32[5];
      ebp = ((this->state.private_keys.as32[(ebx >> 0x18)] +
              this->state.private_keys.as32[((ebx >> 0x10) & 0xFF) + 0x100]) ^
             this->state.private_keys.as32[((ebx >> 0x8) & 0xFF) + 0x200]) +
            this->state.private_keys.as32[(ebx & 0xFF) + 0x300];
      ebp = ebp ^ this->state.initial_keys.as32[4];
      ebp ^= dwords[edx + 1];
      edi = ((this->state.private_keys.as32[(ebp >> 0x18)] +
              this->state.private_keys.as32[((ebp >> 0x10) & 0xFF) + 0x100]) ^
             this->state.private_keys.as32[((ebp >> 0x8) & 0xFF) + 0x200]) +
            this->state.private_keys.as32[(ebp & 0xFF) + 0x300];
      edi = edi ^ this->state.initial_keys.as32[3];
      ebx = ebx ^ edi;
      esi = ((this->state.private_keys.as32[(ebx >> 0x18)] +
              this->state.private_keys.as32[((ebx >> 0x10) & 0xFF) + 0x100]) ^
             this->state.private_keys.as32[((ebx >> 0x8) & 0xFF) + 0x200]) +
            this->state.private_keys.as32[(ebx & 0xFF) + 0x300];
      ebp = ebp ^ esi ^ this->state.initial_keys.as32[2];
      edi = ((this->state.private_keys.as32[(ebp >> 0x18)] +
              this->state.private_keys.as32[((ebp >> 0x10) & 0xFF) + 0x100]) ^
             this->state.private_keys.as32[((ebp >> 0x8) & 0xFF) + 0x200]) +
            this->state.private_keys.as32[(ebp & 0xFF) + 0x300];
      edi = edi ^ this->state.initial_keys.as32[1];
      ebp = ebp ^ this->state.initial_keys.as32[0];
      ebx = ebx ^ edi;
      dwords[edx] = ebp;
      dwords[edx + 1] = ebx;
      edx += 2;
    }
  }
}

PSOEncryption::Type PSOBBEncryption::type() const {
  return Type::BB;
}

void PSOBBEncryption::tfs1_scramble(uint32_t* out1, uint32_t* out2) const {
  uint32_t a = *out1;
  uint32_t b = *out2;
  for (size_t x = 0; x < 0x10; x += 2) {
    a ^= this->state.initial_keys.as32[x];
    b ^= (((this->state.private_keys.as32[a >> 24] +
            this->state.private_keys.as32[((a >> 16) & 0xFF) + 0x100]) ^
           this->state.private_keys.as32[((a >> 8) & 0xFF) + 0x200]) +
          this->state.private_keys.as32[(a & 0xFF) + 0x300]) ^ this->state.initial_keys.as32[x + 1];
    a ^= ((this->state.private_keys.as32[b >> 24] +
           this->state.private_keys.as32[((b >> 16) & 0xFF) + 0x100]) ^
          this->state.private_keys.as32[((b >> 8) & 0xFF) + 0x200]) +
         this->state.private_keys.as32[(b & 0xFF) + 0x300];
  }
  *out1 = this->state.initial_keys.as32[0x11] ^ b;
  *out2 = this->state.initial_keys.as32[0x10] ^ a;
}

void PSOBBEncryption::apply_seed(const void* original_seed, size_t seed_size) {
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

  if (this->state.subtype == Subtype::TFS1) {
    for (size_t x = 0; x < 0x12; x++) {
      uint32_t a = this->state.initial_keys.as32[x] & 0xFFFF;
      this->state.initial_keys.as32[x] = ((a << 0x10) ^ (this->state.initial_keys.as32[x] & 0xFFFF0000)) + a;
    };

    const uint8_t* useed = reinterpret_cast<const uint8_t*>(seed.data());
    for (size_t x = 0; x < 0x48; x += 4) {
      uint32_t seed_data =
          (useed[x % seed_size] << 24) |
          (useed[(x + 1) % seed_size] << 16) |
          (useed[(x + 2) % seed_size] << 8) |
          useed[(x + 3) % seed_size];
      this->state.initial_keys.as32[x >> 2] ^= seed_data;
    }

    uint32_t a = 0, b = 0;
    for (size_t x = 0; x < 0x12; x += 2) {
      this->tfs1_scramble(&a, &b);
      this->state.initial_keys.as32[x] = a;
      this->state.initial_keys.as32[x + 1] = b;
    }

    for (size_t x = 0; x < 0x400; x += 2) {
      this->tfs1_scramble(&a, &b);
      this->state.private_keys.as32[x] = a;
      this->state.private_keys.as32[x + 1] = b;
    }

  } else if (this->state.subtype == Subtype::JSD1) {
    size_t seed_offset = 0;
    for (size_t z = 0; z < 0x100; z++) {
      this->state.private_keys.as8[z] = (z + seed[seed_offset]) ^ (static_cast<uint8_t>(seed[seed_offset]) >> 1);
      seed_offset = (seed_offset + 1) % seed.size();
    }

  } else { // STANDARD or MOCB1 (they share most of their logic)
    if (seed_size % 3) {
      throw invalid_argument("seed size must be divisible by 3");
    }

    if (this->state.subtype == Subtype::MOCB1) {
      for (size_t x = 0; x < 0x12; x++) {
        uint8_t a = this->state.initial_keys.as8[4 * x + 0];
        uint8_t b = this->state.initial_keys.as8[4 * x + 1];
        uint8_t c = this->state.initial_keys.as8[4 * x + 2];
        uint8_t d = this->state.initial_keys.as8[4 * x + 3];
        this->state.initial_keys.as32[x] = ((a ^ d) << 24) | ((b ^ c) << 16) | (a << 8) | b;
      }
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
        this->state.initial_keys.as32[ebx] ^= ebp;
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
        esi = esi ^ this->state.initial_keys.as32[0];
        eax = esi >> 0x18;
        ebx = (esi >> 0x10) & 0xFF;
        eax = this->state.private_keys.as32[eax] + this->state.private_keys.as32[ebx + 0x100];
        ebx = (esi >> 8) & 0xFF;
        eax = eax ^ this->state.private_keys.as32[ebx + 0x200];
        ebx = esi & 0xFF;
        eax = eax + this->state.private_keys.as32[ebx + 0x300];

        eax = eax ^ this->state.initial_keys.as32[1];
        ecx = ecx ^ eax;
        ebx = ecx >> 0x18;
        eax = (ecx >> 0x10) & 0xFF;
        ebx = this->state.private_keys.as32[ebx] + this->state.private_keys.as32[eax + 0x100];
        eax = (ecx >> 8) & 0xFF;
        ebx = ebx ^ this->state.private_keys.as32[eax + 0x200];
        eax = ecx & 0xFF;
        ebx = ebx + this->state.private_keys.as32[eax + 0x300];

        for (x = 0; x <= 5; x++) {
          ebx = ebx ^ this->state.initial_keys.as32[(x * 2) + 2];
          esi = esi ^ ebx;
          ebx = esi >> 0x18;
          eax = (esi >> 0x10) & 0xFF;
          ebx = this->state.private_keys.as32[ebx] + this->state.private_keys.as32[eax + 0x100];
          eax = (esi >> 8) & 0xFF;
          ebx = ebx ^ this->state.private_keys.as32[eax + 0x200];
          eax = esi & 0xFF;
          ebx = ebx + this->state.private_keys.as32[eax + 0x300];

          ebx = ebx ^ this->state.initial_keys.as32[(x * 2) + 3];
          ecx = ecx ^ ebx;
          ebx = ecx >> 0x18;
          eax = (ecx >> 0x10) & 0xFF;
          ebx = this->state.private_keys.as32[ebx] + this->state.private_keys.as32[eax + 0x100];
          eax = (ecx >> 8) & 0xFF;
          ebx = ebx ^ this->state.private_keys.as32[eax + 0x200];
          eax = ecx & 0xFF;
          ebx = ebx + this->state.private_keys.as32[eax + 0x300];
        }

        ebx = ebx ^ this->state.initial_keys.as32[14];
        esi = esi ^ ebx;
        eax = esi >> 0x18;
        ebx = (esi >> 0x10) & 0xFF;
        eax = this->state.private_keys.as32[eax] + this->state.private_keys.as32[ebx + 0x100];
        ebx = (esi >> 8) & 0xFF;
        eax = eax ^ this->state.private_keys.as32[ebx + 0x200];
        ebx = esi & 0xFF;
        eax = eax + this->state.private_keys.as32[ebx + 0x300];

        eax = eax ^ this->state.initial_keys.as32[15];
        eax = ecx ^ eax;
        ecx = eax >> 0x18;
        ebx = (eax >> 0x10) & 0xFF;
        ecx = this->state.private_keys.as32[ecx] + this->state.private_keys.as32[ebx + 0x100];
        ebx = (eax >> 8) & 0xFF;
        ecx = ecx ^ this->state.private_keys.as32[ebx + 0x200];
        ebx = eax & 0xFF;
        ecx = ecx + this->state.private_keys.as32[ebx + 0x300];

        ecx = ecx ^ this->state.initial_keys.as32[16];
        ecx = ecx ^ esi;
        esi = this->state.initial_keys.as32[17];
        esi = esi ^ eax;
        this->state.initial_keys.as32[(edi / 4)] = esi;
        this->state.initial_keys.as32[(edi / 4)+1] = ecx;
        edi = edi + 8;
      }

      eax = 0;
      edx = 0;
      ou = 0;
      while (ou < 0x1000) {
        edi = 0;
        edx = 0x400;

        while (edi < edx) {
          esi = esi ^ this->state.initial_keys.as32[0];
          eax = esi >> 0x18;
          ebx = (esi >> 0x10) & 0xFF;
          eax = this->state.private_keys.as32[eax] + this->state.private_keys.as32[ebx + 0x100];
          ebx = (esi >> 8) & 0xFF;
          eax = eax ^ this->state.private_keys.as32[ebx + 0x200];
          ebx = esi & 0xFF;
          eax = eax + this->state.private_keys.as32[ebx + 0x300];

          eax = eax ^ this->state.initial_keys.as32[1];
          ecx = ecx ^ eax;
          ebx = ecx >> 0x18;
          eax = (ecx >> 0x10) & 0xFF;
          ebx = this->state.private_keys.as32[ebx] + this->state.private_keys.as32[eax + 0x100];
          eax = (ecx >> 8) & 0xFF;
          ebx = ebx ^ this->state.private_keys.as32[eax + 0x200];
          eax = ecx & 0xFF;
          ebx = ebx + this->state.private_keys.as32[eax + 0x300];

          for (x = 0; x <= 5; x++) {
            ebx = ebx ^ this->state.initial_keys.as32[(x * 2) + 2];
            esi = esi ^ ebx;
            ebx = esi >> 0x18;
            eax = (esi >> 0x10) & 0xFF;
            ebx = this->state.private_keys.as32[ebx] + this->state.private_keys.as32[eax + 0x100];
            eax = (esi >> 8) & 0xFF;
            ebx = ebx ^ this->state.private_keys.as32[eax + 0x200];
            eax = esi & 0xFF;
            ebx = ebx + this->state.private_keys.as32[eax + 0x300];

            ebx = ebx ^ this->state.initial_keys.as32[(x * 2) + 3];
            ecx = ecx ^ ebx;
            ebx = ecx >> 0x18;
            eax = (ecx >> 0x10) & 0xFF;
            ebx = this->state.private_keys.as32[ebx] + this->state.private_keys.as32[eax + 0x100];
            eax = (ecx >> 8) & 0xFF;
            ebx = ebx ^ this->state.private_keys.as32[eax + 0x200];
            eax = ecx & 0xFF;
            ebx = ebx + this->state.private_keys.as32[eax + 0x300];
          }

          ebx = ebx ^ this->state.initial_keys.as32[14];
          esi = esi ^ ebx;
          eax = esi >> 0x18;
          ebx = (esi >> 0x10) & 0xFF;
          eax = this->state.private_keys.as32[eax] + this->state.private_keys.as32[ebx + 0x100];
          ebx = (esi >> 8) & 0xFF;
          eax = eax ^ this->state.private_keys.as32[ebx + 0x200];
          ebx = esi & 0xFF;
          eax = eax + this->state.private_keys.as32[ebx + 0x300];

          eax = eax ^ this->state.initial_keys.as32[15];
          eax = ecx ^ eax;
          ecx = eax >> 0x18;
          ebx = (eax >> 0x10) & 0xFF;
          ecx = this->state.private_keys.as32[ecx] + this->state.private_keys.as32[ebx + 0x100];
          ebx = (eax >> 8) & 0xFF;
          ecx = ecx ^ this->state.private_keys.as32[ebx + 0x200];
          ebx = eax & 0xFF;
          ecx = ecx + this->state.private_keys.as32[ebx + 0x300];

          ecx = ecx ^ this->state.initial_keys.as32[16];
          ecx = ecx ^ esi;
          esi = this->state.initial_keys.as32[17];
          esi = esi ^ eax;
          this->state.private_keys.as32[(ou / 4) + (edi / 4)] = esi;
          this->state.private_keys.as32[(ou / 4) + (edi / 4) + 1] = ecx;
          edi = edi + 8;
        }
        ou = ou + 0x400;
      }
    }
  }
}



PSOV2OrV3DetectorEncryption::PSOV2OrV3DetectorEncryption(
    uint32_t key,
    const std::unordered_set<uint32_t>& v2_matches,
    const std::unordered_set<uint32_t>& v3_matches)
  : key(key), v2_matches(v2_matches), v3_matches(v3_matches) { }

void PSOV2OrV3DetectorEncryption::encrypt(void* data, size_t size, bool advance) {
  if (!this->active_crypt) {
    if (size != 4) {
      throw logic_error("initial detector decrypt size must be 4");
    }

    le_uint32_t encrypted = *reinterpret_cast<le_uint32_t*>(data);

    le_uint32_t decrypted_v2 = encrypted;
    unique_ptr<PSOEncryption> v2_crypt(new PSOV2Encryption(this->key));
    v2_crypt->decrypt(&decrypted_v2, sizeof(decrypted_v2), false);

    le_uint32_t decrypted_v3 = encrypted;
    unique_ptr<PSOEncryption> v3_crypt(new PSOV3Encryption(this->key));
    v3_crypt->decrypt(&decrypted_v3, sizeof(decrypted_v3), false);

    bool v2_match = this->v2_matches.count(decrypted_v2);
    bool v3_match = this->v3_matches.count(decrypted_v3);
    if (!v2_match && !v3_match) {
      throw runtime_error(string_printf(
          "unable to determine crypt version (input=%08" PRIX32 ", v2=%08" PRIX32 ", v3=%08" PRIX32 ")",
          encrypted.load(), decrypted_v2.load(), decrypted_v3.load()));
    } else if (v2_match && v3_match) {
      throw runtime_error(string_printf(
          "ambiguous crypt version (v2=%08" PRIX32 ", v3=%08" PRIX32 ")",
          decrypted_v2.load(), decrypted_v3.load()));
    } else if (v2_match) {
      this->active_crypt = move(v2_crypt);
    } else {
      this->active_crypt = move(v3_crypt);
    }
  }
  this->active_crypt->encrypt(data, size, advance);
}

PSOEncryption::Type PSOV2OrV3DetectorEncryption::type() const {
  if (!this->active_crypt) {
    throw logic_error("detector encryption state is indeterminate");
  }
  return this->active_crypt->type();
}



PSOV2OrV3ImitatorEncryption::PSOV2OrV3ImitatorEncryption(
    uint32_t key, std::shared_ptr<PSOV2OrV3DetectorEncryption> detector_crypt)
  : key(key), detector_crypt(detector_crypt) { }

void PSOV2OrV3ImitatorEncryption::encrypt(void* data, size_t size, bool advance) {
  if (!this->active_crypt) {
    auto t = this->detector_crypt->type();
    if (t == Type::V2) {
      this->active_crypt.reset(new PSOV2Encryption(this->key));
    } else if (t == Type::V3) {
      this->active_crypt.reset(new PSOV3Encryption(this->key));
    } else {
      throw logic_error("detector crypt is not V2 or V3");
    }
  }
  this->active_crypt->encrypt(data, size, advance);
}

PSOEncryption::Type PSOV2OrV3ImitatorEncryption::type() const {
  if (!this->active_crypt) {
    return this->detector_crypt->type();
  }
  return this->active_crypt->type();
}



PSOBBMultiKeyDetectorEncryption::PSOBBMultiKeyDetectorEncryption(
    const vector<shared_ptr<const PSOBBEncryption::KeyFile>>& possible_keys,
    const unordered_set<string>& expected_first_data,
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
    if (size != 8) {
      throw logic_error("initial decryption size does not match expected first data size");
    }

    for (const auto& key : this->possible_keys) {
      this->active_key = key;
      this->active_crypt.reset(new PSOBBEncryption(
          *this->active_key, this->seed.data(), this->seed.size()));
      string test_data(reinterpret_cast<const char*>(data), size);
      this->active_crypt->decrypt(test_data.data(), test_data.size(), false);
      if (this->expected_first_data.count(test_data)) {
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

PSOEncryption::Type PSOBBMultiKeyDetectorEncryption::type() const {
  return Type::BB;
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

PSOEncryption::Type PSOBBMultiKeyImitatorEncryption::type() const {
  return Type::BB;
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



void decrypt_trivial_gci_data(void* data, size_t size, uint8_t basis) {
  uint8_t* bytes = reinterpret_cast<uint8_t*>(data);
  uint8_t key = basis + 0x80;
  for (size_t z = 0; z < size; z++) {
    key = (key * 5) + 1;
    bytes[z] ^= key;
  }
}
