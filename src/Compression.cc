#include "Compression.hh"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <phosg/Strings.hh>

using namespace std;



struct prs_compress_ctx {
  unsigned char bitpos;
  std::string forward_log;
  std::string output;

  prs_compress_ctx() : bitpos(0) { }

  string finish() {
    this->put_control_bit(0);
    this->put_control_bit(1);
    if (this->bitpos != 0) {
      this->forward_log[0] = ((this->forward_log[0] << this->bitpos) >> 8);
    }
    this->put_static_data(0);
    this->put_static_data(0);
    this->output += this->forward_log;
    this->forward_log.clear();
    return this->output;
  }

  void put_control_bit_nosave(bool bit) {
    this->forward_log[0] = this->forward_log[0] >> 1;
    this->forward_log[0] |= ((!!bit) << 7);
    this->bitpos++;
  }

  void put_control_save() {
    if (this->bitpos >= 8) {
      this->bitpos = 0;
      this->output += this->forward_log;
      this->forward_log.resize(1);
      this->forward_log[0] = 0;
    }
  }

  void put_control_bit(bool bit) {
    this->put_control_bit_nosave(bit);
    this->put_control_save();
  }

  void put_static_data(uint8_t data) {
    this->forward_log += static_cast<char>(data);
  }

  void raw_byte(uint8_t value) {
    this->put_control_bit_nosave(1);
    this->put_static_data(value);
    this->put_control_save();
  }

  void short_copy(ssize_t offset, uint8_t size) {
    size -= 2;
    this->put_control_bit(0);
    this->put_control_bit(0);
    this->put_control_bit((size >> 1) & 1);
    this->put_control_bit_nosave(size & 1);
    this->put_static_data(offset & 0xFF);
    this->put_control_save();
  }

  void long_copy(ssize_t offset, uint8_t size) {
    if (size <= 9) {
      this->put_control_bit(0);
      this->put_control_bit_nosave(1);
      this->put_static_data(((offset << 3) & 0xF8) | ((size - 2) & 0x07));
      this->put_static_data((offset >> 5) & 0xFF);
      this->put_control_save();
    } else {
      this->put_control_bit(0);
      this->put_control_bit_nosave(1);
      this->put_static_data((offset << 3) & 0xF8);
      this->put_static_data((offset >> 5) & 0xFF);
      this->put_static_data(size - 1);
      this->put_control_save();
    }
  }

  void copy(ssize_t offset, uint8_t size) {
    if ((offset > -0x100) && (size <= 5)) {
      this->short_copy(offset, size);
    } else {
      this->long_copy(offset, size);
    }
  }
};

string prs_compress(const string& data) {
  prs_compress_ctx pc;

  ssize_t data_ssize = static_cast<ssize_t>(data.size());
  ssize_t read_offset = 0;
  while (read_offset < data_ssize) {

    // look for a chunk of data in history matching what's at the current offset
    ssize_t best_offset = 0;
    ssize_t best_size = 0;
    for (ssize_t this_offset = -3;
         (this_offset + data_ssize >= 0) &&
         (this_offset > -0x1FF0) &&
         (best_size < 255);
         this_offset--) {

      // for this offset, expand the match as much as possible
      ssize_t this_size = 1;
      while ((this_size < 0x100) && // max copy size is 255 bytes
             ((this_offset + this_size) < 0) && // don't copy past the read offset
             (this_size <= data_ssize - read_offset) && // don't copy past the end
             !memcmp(data.data() + read_offset + this_offset,
                     data.data() + read_offset, this_size)) {
        this_size++;
      }
      this_size--;

      if (this_size > best_size) {
        best_offset = this_offset;
        best_size = this_size;
      }
    }

    // if there are no good matches, write the byte directly
    if (best_size < 3) {
      pc.raw_byte(data[read_offset]);
      read_offset++;

    } else {
      pc.copy(best_offset, best_size);
      read_offset += best_size;
    }
  }

  return pc.finish();
}



static int16_t get_u8_or_eof(StringReader& r) {
  return r.eof() ? -1 : r.get_u8();
}

string prs_decompress(const string& data, size_t max_size) {
  string output;
  StringReader r(data.data(), data.size());

  int32_t r3, r5;
  int bitpos = 9;
  int16_t currentbyte; // int16_t because it can be -1 when EOF occurs
  int flag;
  int offset;
  unsigned long x, t;

  currentbyte = get_u8_or_eof(r);
  if (currentbyte == EOF) {
    return output;
  }

  for (;;) {
    bitpos--;
    if (bitpos == 0) {
      currentbyte = get_u8_or_eof(r);
      if (currentbyte == EOF) {
        return output;
      }
      bitpos = 8;
    }
    flag = currentbyte & 1;
    currentbyte = currentbyte >> 1;
    if (flag) {
      int ch = get_u8_or_eof(r);
      if (ch == EOF) {
        return output;
      }
      output += static_cast<char>(ch);
      if (max_size && (output.size() > max_size)) {
        throw runtime_error("maximum output size exceeded");
      }
      continue;
    }
    bitpos--;
    if (bitpos == 0) {
      currentbyte = get_u8_or_eof(r);
      if (currentbyte == EOF) {
        return output;
      }
      bitpos = 8;
    }
    flag = currentbyte & 1;
    currentbyte = currentbyte >> 1;
    if (flag) {
      r3 = get_u8_or_eof(r);
      if (r3 == EOF) {
        return output;
      }
      int high_byte = get_u8_or_eof(r);
      if (high_byte == EOF) {
        return output;
      }
      offset = ((high_byte & 0xFF) << 8) | (r3 & 0xFF);
      if (offset == 0) {
        return output;
      }
      r3 = r3 & 0x00000007;
      r5 = (offset >> 3) | 0xFFFFE000;
      if (r3 == 0) {
        flag = 0;
        r3 = get_u8_or_eof(r);
        if (r3 == EOF) {
          return output;
        }
        r3 = (r3 & 0xFF) + 1;
      } else {
        r3 += 2;
      }
    } else {
      r3 = 0;
      for (x = 0; x < 2; x++) {
        bitpos--;
        if (bitpos == 0) {
          currentbyte = get_u8_or_eof(r);
          if (currentbyte == EOF) {
            return output;
          }
          bitpos = 8;
        }
        flag = currentbyte & 1;
        currentbyte = currentbyte >> 1;
        offset = r3 << 1;
        r3 = offset | flag;
      }
      offset = get_u8_or_eof(r);
      if (offset == EOF) {
        return output;
      }
      r3 += 2;
      r5 = offset | 0xFFFFFF00;
    }
    if (r3 == 0) {
      continue;
    }
    t = r3;
    for (x = 0; x < t; x++) {
      output += output.at(output.size() + r5);
      if (max_size && (output.size() > max_size)) {
        throw runtime_error("maximum output size exceeded");
      }
    }
  }
}

size_t prs_decompress_size(const string& data, size_t max_size) {
  size_t output_size = 0;
  StringReader r(data.data(), data.size());

  int32_t r3;
  int bitpos = 9;
  int16_t currentbyte; // int16_t because it can be -1 when EOF occurs
  int flag;
  int offset;
  unsigned long x;

  currentbyte = get_u8_or_eof(r);
  if (currentbyte == EOF) {
    return output_size;
  }

  for (;;) {
    bitpos--;
    if (bitpos == 0) {
      currentbyte = get_u8_or_eof(r);
      if (currentbyte == EOF) {
        return output_size;
      }
      bitpos = 8;
    }
    flag = currentbyte & 1;
    currentbyte = currentbyte >> 1;
    if (flag) {
      int ch = get_u8_or_eof(r);
      if (ch == EOF) {
        return output_size;
      }
      output_size++;
      if (max_size && (output_size > max_size)) {
        throw runtime_error("maximum output size exceeded");
      }
      continue;
    }
    bitpos--;
    if (bitpos == 0) {
      currentbyte = get_u8_or_eof(r);
      if (currentbyte == EOF) {
        return output_size;
      }
      bitpos = 8;
    }
    flag = currentbyte & 1;
    currentbyte = currentbyte >> 1;
    if (flag) {
      r3 = get_u8_or_eof(r);
      if (r3 == EOF) {
        return output_size;
      }
      int high_byte = get_u8_or_eof(r);
      if (high_byte == EOF) {
        return output_size;
      }
      offset = ((high_byte & 0xFF) << 8) | (r3 & 0xFF);
      if (offset == 0) {
        return output_size;
      }
      r3 = r3 & 0x00000007;
      if (r3 == 0) {
        flag = 0;
        r3 = get_u8_or_eof(r);
        if (r3 == EOF) {
          return output_size;
        }
        r3 = (r3 & 0xFF) + 1;
      } else {
        r3 += 2;
      }
    } else {
      r3 = 0;
      for (x = 0; x < 2; x++) {
        bitpos--;
        if (bitpos == 0) {
          currentbyte = get_u8_or_eof(r);
          if (currentbyte == EOF) {
            return output_size;
          }
          bitpos = 8;
        }
        flag = currentbyte & 1;
        currentbyte = currentbyte >> 1;
        offset = r3 << 1;
        r3 = offset | flag;
      }
      offset = get_u8_or_eof(r);
      if (offset == EOF) {
        return output_size;
      }
      r3 += 2;
    }
    if (r3 == 0) {
      continue;
    }
    output_size += r3;
    if (max_size && (output_size > max_size)) {
      throw runtime_error("maximum output size exceeded");
    }
  }
}
