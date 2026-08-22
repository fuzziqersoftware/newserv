#include <phosg/UnitTest.hh>

#include "QuestScript.hh"
#include "RareItemSet.hh"

void run_static_tests() {
  phosg::log_info_f("-- Quest opcode definitions");
  check_quest_opcode_definitions();

  phosg::log_info_f("-- RareItemSet rate calculations");
  for (size_t z = 0; z < 0x100; z++) {
    uint8_t reencoded = RareItemSet::compress_rate(RareItemSet::expand_rate(z));
    if ((reencoded < 0x28) && (z < 0x28)) {
      expect_eq(z & 0x07, reencoded & 0x07);
    } else if (z == 0x28) {
      expect_eq(0x07, reencoded);
    } else if ((z & 0x07) == 0) {
      expect_eq(z - 1, reencoded);
    } else {
      expect_eq(z, reencoded);
    }
  }

  phosg::log_info_f("-- All static tests passed");
}
