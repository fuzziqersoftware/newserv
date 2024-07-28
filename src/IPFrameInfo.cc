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

FrameInfo::FrameInfo(LinkType link_type, const string& data)
    : FrameInfo(link_type, data.data(), data.size()) {}

FrameInfo::FrameInfo(LinkType link_type, const void* header_start, size_t size)
    : FrameInfo() {
  this->link_type = link_type;
  this->header_start = header_start;
  this->total_size = size;
  this->payload_size = size;

  phosg::StringReader r(header_start, size);

  // Parse link-layer header
  Protocol proto = Protocol::NONE;
  switch (this->link_type) {
    case LinkType::ETHERNET:
      this->payload_size -= sizeof(EthernetHeader);
      this->ether = &r.get<EthernetHeader>();
      this->ether_protocol = this->ether->protocol;
      // Unwrap VLAN tags if necessary
      while ((this->ether_protocol == 0x8100) || (this->ether_protocol == 0x88A8)) {
        r.skip(2);
        this->ether_protocol = r.get_u16b();
        this->payload_size -= 4;
      }
      switch (this->ether_protocol) {
        case 0x0800:
          proto = Protocol::IPV4;
          break;
        case 0x0806:
          proto = Protocol::ARP;
          break;
      }
      break;

    case LinkType::HDLC:
      this->payload_size -= (sizeof(HDLCHeader) + 3); // Trim off checksum and end sentinel
      this->hdlc = &r.get<HDLCHeader>();
      this->hdlc_checksum = r.pget_u16b(r.where() + this->payload_size);
      switch (this->hdlc->protocol) {
        case 0xC021:
          proto = Protocol::LCP;
          break;
        case 0xC023:
          proto = Protocol::PAP;
          break;
        case 0x8021:
          proto = Protocol::IPCP;
          break;
        case 0x0021:
          proto = Protocol::IPV4;
          break;
      }
      break;

    default:
      throw logic_error("invalid link type");
  }

  // Parse inner protocol headers
  switch (proto) {
    case Protocol::NONE:
      throw runtime_error("unknown protocol");
    case Protocol::LCP:
      this->payload_size -= sizeof(LCPHeader);
      this->lcp = &r.get<LCPHeader>();
      break;
    case Protocol::PAP:
      this->payload_size -= sizeof(PAPHeader);
      this->pap = &r.get<PAPHeader>();
      break;
    case Protocol::IPCP:
      this->payload_size -= sizeof(IPCPHeader);
      this->ipcp = &r.get<IPCPHeader>();
      break;
    case Protocol::IPV4:
      this->ipv4 = &r.get<IPv4Header>();
      if (this->payload_size < this->ipv4->size) {
        throw invalid_argument("ipv4 header specifies size larger than frame");
      }
      this->payload_size = this->ipv4->size - sizeof(IPv4Header);

      if (this->ipv4->protocol == 0x06) {
        this->tcp = &r.get<TCPHeader>();
        size_t tcp_header_size = (this->tcp->flags >> 12) * 4;
        if (tcp_header_size < sizeof(TCPHeader) || tcp_header_size > this->payload_size) {
          throw invalid_argument("frame is too small for tcp4 header with options");
        }
        this->tcp_options_size = tcp_header_size - sizeof(TCPHeader);
        this->payload_size -= tcp_header_size;
        r.skip(tcp_header_size - sizeof(TCPHeader));

      } else if (this->ipv4->protocol == 0x11) {
        this->payload_size -= sizeof(UDPHeader);
        this->udp = &r.get<UDPHeader>();
      }
      break;
    case Protocol::ARP:
      this->payload_size -= sizeof(ARPHeader);
      this->arp = &r.get<ARPHeader>();
      break;
  }

  this->payload = r.getv(this->payload_size);
}

string FrameInfo::header_str() const {
  if (!this->ether && !this->hdlc) {
    return "<invalid-frame-info>";
  }

  string ret;
  if (this->ether) {
    ret = phosg::string_printf(
        "ETHER:%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX->%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
        this->ether->src_mac[0], this->ether->src_mac[1], this->ether->src_mac[2],
        this->ether->src_mac[3], this->ether->src_mac[4], this->ether->src_mac[5],
        this->ether->dest_mac[0], this->ether->dest_mac[1], this->ether->dest_mac[2],
        this->ether->dest_mac[3], this->ether->dest_mac[4], this->ether->dest_mac[5]);
  } else if (this->hdlc) {
    ret = phosg::string_printf("HDLC:%02hhX/%02hhX", this->hdlc->address, this->hdlc->control);
  } else {
    return "<invalid-frame-info>";
  }

  if (this->arp) {
    ret += phosg::string_printf(
        ",ARP,hw_type=%04hX,proto_type=%04hX,hw_addr_len=%02hhX,proto_addr_len=%02hhX,op=%04hX",
        this->arp->hardware_type.load(), this->arp->protocol_type.load(), this->arp->hwaddr_len, this->arp->paddr_len, this->arp->operation.load());

  } else if (this->ipv4) {
    ret += phosg::string_printf(
        ",IPv4,size=%04hX,src=%08" PRIX32 ",dest=%08" PRIX32,
        this->ipv4->size.load(), this->ipv4->src_addr.load(), this->ipv4->dest_addr.load());

    if (this->udp) {
      ret += phosg::string_printf(
          ",UDP,src_port=%04hX,dest_port=%04hX,size=%04hX",
          this->udp->src_port.load(), this->udp->dest_port.load(), this->udp->size.load());

    } else if (this->tcp) {
      ret += phosg::string_printf(
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
      ret += phosg::string_printf(",proto=%02hhX", this->ipv4->protocol);
    }

  } else {
    if (this->ether) {
      ret += phosg::string_printf(",proto=%04hX", this->ether->protocol.load());
    } else if (this->hdlc) {
      ret += phosg::string_printf(",proto=%04hX", this->hdlc->protocol.load());
    }
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

uint16_t FrameInfo::computed_hdlc_checksum(const void* vdata, size_t size) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(vdata);
  uint16_t crc = 0xFFFF;
  for (size_t z = 0; z < size; z++) {
    crc ^= data[z];
    for (size_t b = 0; b < 8; b++) {
      crc = (crc & 1) ? ((crc >> 1) ^ 0x8408) : (crc >> 1);
    }
  }
  return ~crc;
}

uint16_t FrameInfo::computed_hdlc_checksum() const {
  if (!this->hdlc) {
    throw logic_error("cannot compute HDLC checksum for non-HDLC frame");
  }
  return this->computed_hdlc_checksum(&this->hdlc->address, this->total_size - 4);
}

uint16_t FrameInfo::stored_hdlc_checksum() const {
  return *reinterpret_cast<const le_uint16_t*>(reinterpret_cast<const uint8_t*>(this->header_start) + (this->total_size - 3));
}
