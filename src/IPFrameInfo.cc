#include "IPFrameInfo.hh"

#include <inttypes.h>

#include <phosg/Strings.hh>

using namespace std;

static inline uint16_t collapse_checksum(uint32_t sum) {
  // It's impossible for this to be necessary more than twice: the first
  // addition can carry out at most a single bit.
  sum = (sum & 0xFFFF) + (sum >> 16);
  return (sum & 0xFFFF) + (sum >> 16);
}

FrameInfo::FrameInfo()
    : ether(nullptr),
      ether_protocol(0),
      ipv4(nullptr),
      arp(nullptr),
      udp(nullptr),
      tcp(nullptr),
      header_start(nullptr),
      payload(nullptr),
      total_size(0),
      tcp_options_size(0),
      payload_size(0) {}

FrameInfo::FrameInfo(const string& data) : FrameInfo(data.data(), data.size()) {}

FrameInfo::FrameInfo(const void* header_start, size_t size)
    : ether(nullptr),
      ether_protocol(0),
      ipv4(nullptr),
      arp(nullptr),
      udp(nullptr),
      tcp(nullptr),
      header_start(header_start),
      payload(nullptr),
      total_size(size),
      tcp_options_size(0),
      payload_size(size) {

  // Parse ethernet header
  if (this->payload_size < sizeof(EthernetHeader)) {
    throw invalid_argument("frame is too small for ethernet");
  }
  this->payload_size -= sizeof(EthernetHeader);
  this->ether = reinterpret_cast<const EthernetHeader*>(header_start);
  this->ether_protocol = this->ether->protocol;

  // Figure out the protocol
  const be_uint16_t* u16data = reinterpret_cast<const be_uint16_t*>(this->ether + 1);
  while ((this->ether_protocol == 0x8100) || (this->ether_protocol == 0x88A8)) {
    if (this->payload_size < 4) {
      throw invalid_argument("VLAN tags exceed frame size");
    }
    this->ether_protocol = u16data[1];
    u16data += 2;
    this->payload_size -= 4;
  }

  // TODO: Some less-common protocols that we might want to support:
  // 0x8035 = RARP
  // 0x809B = AppleTalk
  // 0x80F3 = AppleTalk ARP
  // 0x8137 = IPX
  // 0x9000 = loopback

  // Parse protocol headers if possible
  if (this->ether_protocol == 0x0800) { // IPv4
    if (this->payload_size < sizeof(IPv4Header)) {
      throw invalid_argument("frame is too small for ipv4 header");
    }
    this->ipv4 = reinterpret_cast<const IPv4Header*>(u16data);
    if (this->payload_size < this->ipv4->size) {
      throw invalid_argument("ipv4 header specifies size larger than frame");
    }
    this->payload_size = this->ipv4->size - sizeof(IPv4Header);

    if (this->ipv4->protocol == 0x06) {
      if (this->payload_size < sizeof(TCPHeader)) {
        throw invalid_argument("frame is too small for tcp4 header");
      }
      this->tcp = reinterpret_cast<const TCPHeader*>(this->ipv4 + 1);
      size_t tcp_header_size = (this->tcp->flags >> 12) * 4;
      if (tcp_header_size < sizeof(TCPHeader) || tcp_header_size > this->payload_size) {
        throw invalid_argument("frame is too small for tcp4 header with options");
      }
      this->tcp_options_size = tcp_header_size - sizeof(TCPHeader);
      this->payload_size -= tcp_header_size;
      this->payload = reinterpret_cast<const uint8_t*>(this->tcp) + tcp_header_size;

    } else if (this->ipv4->protocol == 0x11) {
      if (this->payload_size < sizeof(UDPHeader)) {
        throw invalid_argument("frame is too small for udp4 header");
      }
      this->payload_size -= sizeof(UDPHeader);
      this->udp = reinterpret_cast<const UDPHeader*>(this->ipv4 + 1);
      this->payload = this->udp + 1;

    } else {
      this->payload = this->ipv4 + 1;
    }

  } else if (this->ether_protocol == 0x0806) { // ARP
    if (this->payload_size < sizeof(const ARPHeader)) {
      throw invalid_argument("frame is too small for arp header");
    }
    this->payload_size -= sizeof(ARPHeader);
    this->arp = reinterpret_cast<const ARPHeader*>(u16data);
    this->payload = this->arp + 1;

  } else {
    throw runtime_error("unknown protocol");
  }
}

string FrameInfo::header_str() const {
  if (!this->ether) {
    return "<invalid-frame-info>";
  }

  string ret = string_printf(
      "%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX->%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
      this->ether->src_mac[0], this->ether->src_mac[1], this->ether->src_mac[2],
      this->ether->src_mac[3], this->ether->src_mac[4], this->ether->src_mac[5],
      this->ether->dest_mac[0], this->ether->dest_mac[1], this->ether->dest_mac[2],
      this->ether->dest_mac[3], this->ether->dest_mac[4], this->ether->dest_mac[5]);

  if (this->arp) {
    ret += string_printf(
        ",ARP,hw_type=%04hX,proto_type=%04hX,hw_addr_len=%02hhX,proto_addr_len=%02hhX,op=%04hX",
        this->arp->hardware_type.load(), this->arp->protocol_type.load(), this->arp->hwaddr_len, this->arp->paddr_len, this->arp->operation.load());

  } else if (this->ipv4) {
    ret += string_printf(
        ",IPv4,size=%04hX,src=%08" PRIX32 ",dest=%08" PRIX32,
        this->ipv4->size.load(), this->ipv4->src_addr.load(), this->ipv4->dest_addr.load());

    if (this->udp) {
      ret += string_printf(
          ",UDP,src_port=%04hX,dest_port=%04hX,size=%04hX",
          this->udp->src_port.load(), this->udp->dest_port.load(), this->udp->size.load());

    } else if (this->tcp) {
      ret += string_printf(
          ",TCP,src_port=%04hX,dest_port=%04hX,seq=%08" PRIX32 ",ack=%08" PRIX32 ",flags=%04hX(",
          this->tcp->src_port.load(), this->tcp->dest_port.load(), this->tcp->seq_num.load(), this->tcp->ack_num.load(), this->tcp->flags.load());
      if (this->tcp->flags & TCPHeader::Flag::FIN) {
        ret += "FIN,";
      }
      if (this->tcp->flags & TCPHeader::Flag::SYN) {
        ret += "SYN,";
      }
      if (this->tcp->flags & TCPHeader::Flag::RST) {
        ret += "RST,";
      }
      if (this->tcp->flags & TCPHeader::Flag::PSH) {
        ret += "PSH,";
      }
      if (this->tcp->flags & TCPHeader::Flag::ACK) {
        ret += "ACK";
      }
      ret += ')';

    } else {
      ret += string_printf(",proto=%02hhX", this->ipv4->protocol);
    }

  } else {
    ret += string_printf(",proto=%04hX", this->ether->protocol.load());
  }

  return ret;
}

void FrameInfo::truncate(size_t new_total_size) {
  if (new_total_size > this->total_size) {
    throw logic_error("truncate call expands frame size");
  }
  if (new_total_size < this->payload_size) {
    throw logic_error("truncate call destroys part of header");
  }
  size_t delta_bytes = this->total_size - new_total_size;
  this->total_size -= delta_bytes;
  this->payload_size -= delta_bytes;
}

size_t FrameInfo::size_from_header() const {
  if (this->ipv4) {
    return this->ipv4->size;
  } else if (this->arp) {
    return sizeof(ARPHeader) + 2 * (this->arp->hwaddr_len + this->arp->paddr_len);
  } else {
    return 0;
  }
}

uint16_t FrameInfo::computed_ipv4_header_checksum(const IPv4Header& ipv4) {
  return ~collapse_checksum(
      ((ipv4.version_ihl << 8) | ipv4.tos) +
      ipv4.size +
      ipv4.id +
      ipv4.frag_offset +
      ((ipv4.ttl << 8) | ipv4.protocol) +
      (ipv4.src_addr >> 16) +
      (ipv4.src_addr & 0xFFFF) +
      (ipv4.dest_addr >> 16) +
      (ipv4.dest_addr & 0xFFFF));
}

uint16_t FrameInfo::computed_ipv4_header_checksum() const {
  if (!this->ipv4) {
    throw logic_error("cannot compute ipv4 header checksum for non-ipv4 frame");
  }
  return this->computed_ipv4_header_checksum(*this->ipv4);
}

uint16_t FrameInfo::computed_udp4_checksum(
    const IPv4Header& ipv4, const UDPHeader& udp, const void* data, size_t size) {
  uint32_t sum =
      (ipv4.src_addr >> 16) +
      (ipv4.src_addr & 0xFFFF) +
      (ipv4.dest_addr >> 16) +
      (ipv4.dest_addr & 0xFFFF) +
      ipv4.protocol +
      udp.size +
      udp.src_port +
      udp.dest_port +
      udp.size;

  const uint8_t* u8_data = reinterpret_cast<const uint8_t*>(data);
  for (size_t offset = 0; offset + 2 <= size; offset += 2) {
    sum += *reinterpret_cast<const be_uint16_t*>(u8_data + offset);
  }
  if (size & 1) {
    sum += u8_data[size - 1] << 8;
  }
  return ~collapse_checksum(sum);
}

uint16_t FrameInfo::computed_udp4_checksum() const {
  if (!this->ipv4) {
    throw logic_error("cannot compute udp header checksum for non-ipv4 frame");
  }
  if (!this->udp) {
    throw logic_error("cannot compute udp header checksum for non-udp frame");
  }
  return this->computed_udp4_checksum(
      *this->ipv4, *this->udp, this->payload, this->payload_size);
}

uint16_t FrameInfo::computed_tcp4_checksum(
    const IPv4Header& ipv4, const TCPHeader& tcp, const void* data, size_t size) {
  uint16_t tcp_size = ipv4.size - sizeof(IPv4Header);
  uint32_t sum =
      (ipv4.src_addr >> 16) +
      (ipv4.src_addr & 0xFFFF) +
      (ipv4.dest_addr >> 16) +
      (ipv4.dest_addr & 0xFFFF) +
      ipv4.protocol +
      tcp_size +
      tcp.src_port +
      tcp.dest_port +
      (tcp.seq_num >> 16) +
      (tcp.seq_num & 0xFFFF) +
      (tcp.ack_num >> 16) +
      (tcp.ack_num & 0xFFFF) +
      tcp.flags +
      tcp.window +
      tcp.urgent_ptr;

  const uint8_t* u8_data = reinterpret_cast<const uint8_t*>(data);
  for (size_t offset = 0; offset + 2 <= size; offset += 2) {
    sum += *reinterpret_cast<const be_uint16_t*>(u8_data + offset);
  }
  if (size & 1) {
    sum += u8_data[size - 1] << 8;
  }
  return ~collapse_checksum(sum);
}

uint16_t FrameInfo::computed_tcp4_checksum() const {
  if (!this->ipv4) {
    throw logic_error("cannot compute tcp header checksum for non-ipv4 frame");
  }
  if (!this->tcp) {
    throw logic_error("cannot compute tcp header checksum for non-tcp frame");
  }
  return this->computed_tcp4_checksum(
      *this->ipv4, *this->tcp, this->tcp + 1,
      this->payload_size + this->tcp_options_size);
}
