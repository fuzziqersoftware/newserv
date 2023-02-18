# This gives you a VIP card in PSO Episode 3 USA.

# This patch is only for PSO Episode 3 USA, which means it requires the
# EnableEpisode3SendFunctionCall option to be enabled in config.json. If that
# option is disabled, the Patches menu won't appear for the client. If this
# patch is run on a different client version, it will do nothing.

entry_ptr:
reloc0:
  .offsetof start

start:
  # Note: We don't actually need this for Episode 3, since all Episode 3
  # versions correctly clear the caches before running code from a B2 command.
  # But we leave it in to be consistent with patches for Episodes 1&2.
  .include InitClearCaches

  .include Episode3USAOnly

  # Call seq_var_set(7000) - this gives the local player a VIP card
  li     r3, 7000
  lis    r0, 0x8010
  ori    r0, r0, 0xBD18
  mtctr  r0
  bctr
