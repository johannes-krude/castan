//===-- ktest2pcap.cpp ----------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <assert.h>
#include <string>

#include "klee/Internal/ADT/KTest.h"

#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <pcap/pcap.h>

uint16_t checksum(const void *buf, size_t len) {
  unsigned long sum = 0;
  const uint16_t *pos = (const uint16_t *)buf;

  while (len > 1) {
    sum += *pos++;
    if (sum & 0x80000000)
      sum = (sum & 0xFFFF) + (sum >> 16);
    len -= 2;
  }

  while (sum >> 16)
    sum = (sum & 0xFFFF) + (sum >> 16);

  return ~sum;
}

int main(int argc, char **argv) {
  assert((argc >= 3 && argc <= 5) &&
         "Usage: ktest2pcap <ktest-file> <pcap-file> [nic id] [num packets]");

  KTest *input = kTest_fromFile(argv[1]);
  assert(input && "Error loading ktest file.");

  pcap_t *pcap = pcap_open_dead(DLT_EN10MB, 65536);
  pcap_dumper_t *out = pcap_dump_open(pcap, argv[2]);
  assert(out && "Error opening pcap file.");

  int selected_nic = 0;
  if (argc >= 4) {
    selected_nic = atoi(argv[3]);
  }

  long num_packets = 0;
  if (argc >= 5) {
    num_packets = atol(argv[4]);
  }

  int current_nic = 0;
  for (unsigned i = 0; i < input->numObjects; i++) {
    KTestObject *o = &input->objects[i];

    if (std::string(o->name) == "VIGOR_DEVICE") {
      current_nic = *((int *)o->bytes);
    } else if (std::string(o->name) == "castan_packet" ||
               std::string(o->name) == "user_buf") {
      if (current_nic != selected_nic) {
        fprintf(stderr, "Skipping packet for NIC %d\n", current_nic);
        continue;
      }

      struct pcap_pkthdr pkthdr = {
          .ts = {.tv_sec = 0, .tv_usec = 0},
          .caplen = o->numBytes,
          .len = o->numBytes,
      };

      // Fix checksums.
      // Check if IP packet.
      assert(pkthdr.caplen >= sizeof(struct ether_header) &&
             "Truncated Ethernet frame.");
      if (ntohs(((struct ether_header *)o->bytes)->ether_type) !=
          ETHERTYPE_IP) {
        fprintf(stderr, "Packet with unsupported l3-type: %d\n",
                ntohs(((struct ether_header *)o->bytes)->ether_type));
      } else {
        // Extract IP header.
        assert(pkthdr.caplen >=
                   sizeof(struct ether_header) + sizeof(struct ip) &&
               "Truncated IP packet.");
        struct ip *ip = (struct ip *)(o->bytes + sizeof(struct ether_header));
        if (ip->ip_v != 4) {
          fprintf(stderr, "Unsupported IP version: %d\n", ip->ip_v);
        } else {
          assert(ip->ip_hl >= 5 && "Truncated IP packet.");

          ip->ip_sum = 0;

          switch (ip->ip_p) {
          case 0x06: { // TCP
            assert(pkthdr.caplen >= sizeof(struct ether_header) +
                                        ip->ip_hl * 4 + sizeof(struct tcphdr) &&
                   "Truncated TCP segment.");
            assert(ip->ip_len >= ip->ip_hl * 4 + sizeof(struct tcphdr) &&
                   "Truncated TCP segment.");
            struct tcphdr *tcp =
                (struct tcphdr *)(((char *)ip) + ip->ip_hl * 4);
            assert(tcp->doff >= 5 && "Truncated TCP segment.");

            tcp->check = 0;
          } break;
          case 0x11: { // UDP
            assert(pkthdr.caplen >= sizeof(struct ether_header) +
                                        ip->ip_hl * 4 + sizeof(struct udphdr) &&
                   "Truncated UDP datagram.");
            assert(ip->ip_len >= ip->ip_hl * 4 + sizeof(struct udphdr) &&
                   "Truncated UDP datagram.");
            struct udphdr *udp =
                (struct udphdr *)(((char *)ip) + ip->ip_hl * 4);
            assert(ntohs(udp->len) >= 8 && "Truncated UDP datagram.");

            udp->check = 0;
          } break;
          default:
            fprintf(stderr, "Packet with unsupported transport protocol: %d\n",
                    ip->ip_p);
          }

          ip->ip_sum = checksum(ip, ip->ip_hl * 4);
        }
      }

      pcap_dump((u_char *)out, &pkthdr, o->bytes);

      if (--num_packets == 0) {
        break;
      }
    }
  }

  pcap_close(pcap);
  pcap_dump_close(out);

  return 0;
}
