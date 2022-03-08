#pragma once

#include <stdint.h>

#include <phosg/Encoding.hh>



struct EthernetHeader {
  uint8_t dest_mac[6];
  uint8_t src_mac[6];
  be_uint16_t protocol;
} __attribute__((packed));

struct ARPHeader {
  be_uint16_t hardware_type;
  be_uint16_t protocol_type; // same as EthernetHeader::protocol
  uint8_t hwaddr_len;
  uint8_t paddr_len;
  be_uint16_t operation;
} __attribute__((packed));

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
} __attribute__((packed));

struct UDPHeader {
  be_uint16_t src_port;
  be_uint16_t dest_port;
  be_uint16_t size;
  be_uint16_t checksum;
} __attribute__((packed));

struct TCPHeader {
  enum Flag {
    NS  = 0x0100,
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
} __attribute__((packed));



struct FrameInfo {
  // This is always valid
  const EthernetHeader* ether;
  uint16_t ether_protocol;

  // At most one of these is not null
  const IPv4Header* ipv4;
  const ARPHeader* arp;

  // One of these may be not null if this->ipv4 is not null
  const UDPHeader* udp;
  const TCPHeader* tcp;

  const void* header_start;
  const void* payload;
  size_t total_size;
  size_t tcp_options_size;
  size_t payload_size;

  FrameInfo();
  FrameInfo(const std::string& data);
  FrameInfo(const void* data, size_t size);

  std::string header_str() const;

  void truncate(size_t new_total_size);

  size_t size_from_header() const;

  static uint16_t computed_ipv4_header_checksum(const IPv4Header& ipv4);
  uint16_t computed_ipv4_header_checksum() const;
  static uint16_t computed_udp4_checksum(
      const IPv4Header& ipv4, const UDPHeader& udp, const void* data, size_t size);
  uint16_t computed_udp4_checksum() const;
  static uint16_t computed_tcp4_checksum(
      const IPv4Header& ip, const TCPHeader& tcp, const void* data, size_t size);
  uint16_t computed_tcp4_checksum() const;
};
