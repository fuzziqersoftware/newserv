#pragma once

#include <stdint.h>

#include <phosg/Encoding.hh>
#include <phosg/Strings.hh>

#include "Text.hh"

struct HDLCHeader {
  uint8_t start_sentinel1; // 0x7E
  uint8_t address; // 0xFF usually
  uint8_t control; // 0x03 for PPP
  be_uint16_t protocol;
} __packed_ws__(HDLCHeader, 5);

struct LCPHeader {
  uint8_t command;
  uint8_t request_id;
  be_uint16_t size;
} __packed_ws__(LCPHeader, 4);

struct PAPHeader {
  uint8_t command;
  uint8_t request_id;
  be_uint16_t size;
} __packed_ws__(PAPHeader, 4);

struct IPCPHeader {
  uint8_t command;
  uint8_t request_id;
  be_uint16_t size;
} __packed_ws__(IPCPHeader, 4);

struct EthernetHeader {
  parray<uint8_t, 6> dest_mac;
  parray<uint8_t, 6> src_mac;
  be_uint16_t protocol;
} __packed_ws__(EthernetHeader, 0x0E);

struct ARPHeader {
  be_uint16_t hardware_type;
  be_uint16_t protocol_type; // same as EthernetHeader::protocol
  uint8_t hwaddr_len;
  uint8_t paddr_len;
  be_uint16_t operation;
} __packed_ws__(ARPHeader, 8);

struct IPv4Header {
  uint8_t version_ihl;
  uint8_t tos;
  be_uint16_t size;
  be_uint16_t id;
  be_uint16_t frag_offset;
  uint8_t ttl;
  uint8_t protocol;
  be_uint16_t checksum;
  be_uint32_t src_addr;
  be_uint32_t dest_addr;
} __packed_ws__(IPv4Header, 0x14);

struct UDPHeader {
  be_uint16_t src_port;
  be_uint16_t dest_port;
  be_uint16_t size;
  be_uint16_t checksum;
} __packed_ws__(UDPHeader, 8);

struct TCPHeader {
  enum Flag {
    NS = 0x0100,
    CWR = 0x0080, // congestion window reduced
    ECE = 0x0040, // ECN capable / congestion experienced
    URG = 0x0020, // urgent pointer used
    ACK = 0x0010, // ack_num is valid
    PSH = 0x0008, // sending data
    RST = 0x0004, // reset (hard disconnect)
    SYN = 0x0002, // synchronize sequence numbers (open connection)
    FIN = 0x0001, // close (normal disconnect)
  };

  be_uint16_t src_port;
  be_uint16_t dest_port;
  be_uint32_t seq_num;
  be_uint32_t ack_num;
  be_uint16_t flags;
  be_uint16_t window;
  be_uint16_t checksum;
  be_uint16_t urgent_ptr;
} __packed_ws__(TCPHeader, 0x14);

struct DHCPHeader {
  uint8_t opcode = 0;
  uint8_t hardware_type = 1; // 1 = Ethernet
  uint8_t hardware_address_length = 6; // 6 for Ethernet
  uint8_t hops = 0;
  be_uint32_t transaction_id = 0;
  be_uint16_t seconds_elapsed = 0;
  be_uint16_t flags = 0;
  be_uint32_t client_ip_address = 0;
  be_uint32_t your_ip_address = 0;
  be_uint32_t server_ip_address = 0;
  be_uint32_t gateway_ip_address = 0;
  parray<uint8_t, 0x10> client_hardware_address;
  parray<uint8_t, 0xC0> unused_bootp_legacy;
  be_uint32_t magic = 0x63825363;
  // Options follow here, terminated with FF
} __packed_ws__(DHCPHeader, 0xF0);

struct FrameInfo {
  enum class LinkType {
    ETHERNET = 0,
    HDLC,
  };

  enum class Protocol {
    NONE = 0,
    LCP,
    PAP,
    IPCP,
    IPV4,
    ARP,
    // TODO: Some less-common protocols that we might want to support:
    //   Ether  / HDLC   = proto
    //   0x8035 / ?????? = RARP
    //   0x809B / 0x0029 = AppleTalk
    //   0x80F3 / ?????? = AppleTalk ARP
    //   0x8137 / 0x002B = IPX
  };

  LinkType link_type = LinkType::ETHERNET;

  // Exactly one of these headers is valid
  const EthernetHeader* ether = nullptr;
  uint16_t ether_protocol = 0;
  const HDLCHeader* hdlc = nullptr;
  uint16_t hdlc_checksum = 0;

  // One of these may be non-null if hdlc is valid
  const LCPHeader* lcp = nullptr;
  const PAPHeader* pap = nullptr;
  const IPCPHeader* ipcp = nullptr;

  // At most one of these is not null
  const IPv4Header* ipv4 = nullptr;
  const ARPHeader* arp = nullptr;

  // One of these may be not null if this->ipv4 is not null
  const UDPHeader* udp = nullptr;
  const TCPHeader* tcp = nullptr;

  const void* header_start = nullptr;
  const void* payload = nullptr;
  size_t total_size = 0;
  size_t tcp_options_size = 0;
  size_t payload_size = 0;

  FrameInfo() = default;
  FrameInfo(LinkType link_type, const std::string& data);
  FrameInfo(LinkType link_type, const void* data, size_t size);

  std::string header_str() const;

  inline phosg::StringReader read_payload() const {
    return phosg::StringReader(this->payload, this->payload_size);
  }

  void truncate(size_t new_total_size);

  size_t size_from_header() const;

  static uint16_t computed_ipv4_header_checksum(const IPv4Header& ipv4);
  uint16_t computed_ipv4_header_checksum() const;
  static uint16_t computed_udp4_checksum(const IPv4Header& ipv4, const UDPHeader& udp, const void* data, size_t size);
  uint16_t computed_udp4_checksum() const;
  static uint16_t computed_tcp4_checksum(const IPv4Header& ip, const TCPHeader& tcp, const void* data, size_t size);
  uint16_t computed_tcp4_checksum() const;

  static uint16_t computed_hdlc_checksum(const void* data, size_t size);
  uint16_t computed_hdlc_checksum() const;
  uint16_t stored_hdlc_checksum() const;
};
