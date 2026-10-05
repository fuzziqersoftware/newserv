#include <phosg/UnitTest.hh>

#include "QuestScript.hh"
#include "RareItemSet.hh"

void run_static_tests() {
  phosg::log_info_f("-- Quest opcode definitions");
  check_quest_opcode_definitions();

  phosg::log_info_f("-- RareItemSet rate calculations");
  for (size_t z = 0; z < 0x100; z++) {
    uint32_t expanded = RareItemSet::expand_rate(z);
    uint8_t reencoded = RareItemSet::compress_rate(expanded);
    if (z == 0) {
      expect_eq(reencoded, 0);
    } else if ((reencoded < 0x28) && (z < 0x28)) {
      if ((z & 7) == 0) {
        expect_eq(1, reencoded & 7);
      } else {
        expect_eq(z & 7, reencoded & 7);
      }
    } else if (z == 0x28) {
      expect_eq(7, reencoded);
    } else if ((z & 7) == 0) {
      expect_eq(z - 1, reencoded);
    } else {
      expect_eq(z, reencoded);
    }
  }

  phosg::log_info_f("-- All static tests passed");
}
