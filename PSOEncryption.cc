#include "PSOEncryption.hh"

#include <stdio.h>
#include <string.h>

#include <stdexcept>
#include <string>
#include <phosg/Encoding.hh>
#include <phosg/Strings.hh>

using namespace std;



// TODO: fix style in this file, especially in psobb functions



// most ciphers used by pso are symmetric; alias decrypt to encrypt by default
void PSOEncryption::decrypt(void* data, size_t size) {
  this->encrypt(data, size);
}



void PSOPCEncryption::update_stream() {
  uint32_t esi, edi, eax, ebp, edx;
  edi = 1;
  edx = 0x18;
  eax = edi;
  while (edx > 0) {
    esi = this->stream[eax + 0x1F];
    ebp = this->stream[eax];
    ebp = ebp - esi;
    this->stream[eax] = ebp;
    eax++;
    edx--;
  }
  edi = 0x19;
  edx = 0x1F;
  eax = edi;
  while (edx > 0) {
    esi = this->stream[eax - 0x18];
    ebp = this->stream[eax];
    ebp = ebp - esi;
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

uint32_t PSOPCEncryption::next() {
  if (this->offset == PC_STREAM_LENGTH) {
    this->update_stream();
    this->offset = 1;
  }
  return this->stream[this->offset++];
}

void PSOPCEncryption::encrypt(void* vdata, size_t size) {
  if (size & 3) {
    throw invalid_argument("size must be a multiple of 4");
  }
  size >>= 2;

  uint32_t* data = reinterpret_cast<uint32_t*>(vdata);
  for (size_t x = 0; x < size; x++) {
    data[x] ^= this->next();
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

uint32_t PSOGCEncryption::next() {
  this->offset++;
  if (this->offset == GC_STREAM_LENGTH) {
    this->update_stream();
  }
  return this->stream[this->offset];
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

  for (size_t x = 0; x < 3; x++) {
    this->update_stream();
  }
  this->offset = GC_STREAM_LENGTH - 1;
}

void PSOGCEncryption::encrypt(void* vdata, size_t size) {
  if (size & 3) {
    throw invalid_argument("size must be a multiple of 4");
  }
  size >>= 2;

  uint32_t* data = reinterpret_cast<uint32_t*>(vdata);
  for (size_t x = 0; x < size; x++) {
    data[x] ^= this->next();
  }
}



void PSOBBEncryption::decrypt(void* vdata, size_t size) {
  if (size & 7) {
    throw invalid_argument("size must be a multiple of 8");
  }
  size >>= 3;

  uint32_t* data = reinterpret_cast<uint32_t*>(vdata);
  uint32_t edx, ebx, ebp, esi, edi;

  edx = 0;
  while (edx < size) {
    ebx = data[edx];
    ebx = ebx ^ this->stream[5];
    ebp = ((this->stream[(ebx >> 0x18) + 0x12]+this->stream[((ebx >> 0x10)& 0xff) + 0x112])
      ^ this->stream[((ebx >> 0x8)& 0xff) + 0x212]) + this->stream[(ebx & 0xff) + 0x312];
    ebp = ebp ^ this->stream[4];
    ebp ^= data[edx + 1];
    edi = ((this->stream[(ebp >> 0x18) + 0x12]+this->stream[((ebp >> 0x10)& 0xff) + 0x112])
      ^ this->stream[((ebp >> 0x8)& 0xff) + 0x212]) + this->stream[(ebp & 0xff) + 0x312];
    edi = edi ^ this->stream[3];
    ebx = ebx ^ edi;
    esi = ((this->stream[(ebx >> 0x18) + 0x12]+this->stream[((ebx >> 0x10)& 0xff) + 0x112])
      ^ this->stream[((ebx >> 0x8)& 0xff) + 0x212]) + this->stream[(ebx & 0xff) + 0x312];
    ebp = ebp ^ esi ^ this->stream[2];
    edi = ((this->stream[(ebp >> 0x18) + 0x12]+this->stream[((ebp >> 0x10)& 0xff) + 0x112])
      ^ this->stream[((ebp >> 0x8)& 0xff) + 0x212]) + this->stream[(ebp & 0xff) + 0x312];
    edi = edi ^ this->stream[1];
    ebp = ebp ^ this->stream[0];
    ebx = ebx ^ edi;
    data[edx] = ebp;
    data[edx + 1] = ebx;
    edx += 2;
  }
}

void PSOBBEncryption::encrypt(void* vdata, size_t size) {
  if (size & 7) {
    throw invalid_argument("size must be a multiple of 8");
  }
  size >>= 3;

  uint8_t* data = reinterpret_cast<uint8_t*>(vdata);
  uint32_t edx, ebx, ebp, esi, edi;

  edx = 0;
  while (edx < size) {
    ebx = data[edx];
    ebx = ebx ^ this->stream[0];
    ebp = ((this->stream[(ebx >> 0x18) + 0x12]+this->stream[((ebx >> 0x10)& 0xff) + 0x112])
      ^ this->stream[((ebx >> 0x8)& 0xff) + 0x212]) + this->stream[(ebx & 0xff) + 0x312];
    ebp = ebp ^ this->stream[1];
    ebp ^= data[edx + 1];
    edi = ((this->stream[(ebp >> 0x18) + 0x12]+this->stream[((ebp >> 0x10)& 0xff) + 0x112])
      ^ this->stream[((ebp >> 0x8)& 0xff) + 0x212]) + this->stream[(ebp & 0xff) + 0x312];
    edi = edi ^ this->stream[2];
    ebx = ebx ^ edi;
    esi = ((this->stream[(ebx >> 0x18) + 0x12]+this->stream[((ebx >> 0x10)& 0xff) + 0x112])
      ^ this->stream[((ebx >> 0x8)& 0xff) + 0x212]) + this->stream[(ebx & 0xff) + 0x312];
    ebp = ebp ^ esi ^ this->stream[3];
    edi = ((this->stream[(ebp >> 0x18) + 0x12]+this->stream[((ebp >> 0x10)& 0xff) + 0x112])
      ^ this->stream[((ebp >> 0x8)& 0xff) + 0x212]) + this->stream[(ebp & 0xff) + 0x312];
    edi = edi ^ this->stream[4];
    ebp = ebp ^ this->stream[5];
    ebx = ebx ^ edi;
    data[edx] = ebp;
    data[edx + 1] = ebx;
    edx += 2;
  }
}

PSOBBEncryption::PSOBBEncryption(const KeyFile& key, const void* original_seed,
    size_t seed_size) : offset(0) {
  if (seed_size % 3) {
    throw invalid_argument("seed size must be divisible by 3");
  }

  string seed;
  const uint8_t* original_seed_data = reinterpret_cast<const uint8_t*>(
      original_seed);
  for (size_t x = 0; x < seed_size; x += 3) {
    seed.push_back(original_seed_data[x] ^ 0x19);
    seed.push_back(original_seed_data[x + 1] ^ 0x16);
    seed.push_back(original_seed_data[x + 2] ^ 0x18);
  }

  memcpy(this->stream, &key, sizeof(key));

  this->postprocess_initial_stream(seed);
}

void PSOBBEncryption::postprocess_initial_stream(const string& seed) {
  uint32_t eax, ecx, edx, ebx, ebp, esi, edi, ou, x;

  ecx = 0;
  ebx = 0;

  while (ebx < (seed.size() / 4)) {
    ebp = static_cast<uint32_t>(seed[ecx]) << 0x18;
    eax = ecx + 1;
    edx = eax % seed.size();
    eax = (static_cast<uint32_t>(seed[edx]) << 0x10) & 0xFF0000;
    ebp = (ebp | eax) & 0xffff00ff;
    eax = ecx + 2;
    edx = eax % seed.size();
    eax = (static_cast<uint32_t>(seed[edx]) << 0x08) & 0xFF00;
    ebp = (ebp | eax) & 0xffffff00;
    eax = ecx + 3;
    ecx = ecx + 4;
    edx = eax % seed.size();
    eax = static_cast<uint32_t>(seed[edx]);
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
    ebx = (esi >> 0x10) & 0xff;
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
      eax = ecx & 0xff;
      ebx = ebx + this->stream[eax + 0x312];
    }

    ebx = ebx ^ this->stream[14];
    esi = esi ^ ebx;
    eax = esi >> 0x18;
    ebx = (esi >> 0x10) & 0xFF;
    eax = this->stream[eax + 0x12] + this->stream[ebx + 0x112];
    ebx = (esi >> 8) & 0xff;
    eax = eax ^ this->stream[ebx + 0x212];
    ebx = esi & 0xff;
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
      ebx = (esi >> 0x10) & 0xff;
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
