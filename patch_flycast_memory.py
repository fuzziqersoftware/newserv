#!/bin/env python3

import os
import subprocess
import sys


def get_ip_address(ifname):
  data = subprocess.check_output(['ifconfig', ifname])
  for line in data.splitlines():
    line = line.strip()
    if line.startswith(b'inet '):
      return line.split()[1].decode('ascii')
  raise RuntimeError('cannot get address for interface ' + ifname)


def main(argv):
  if len(argv) < 2:
    raise RuntimeError(f'Usage: {argv[0]} <original-destination> [new-destination]')
  if os.geteuid() != 0:
    raise RuntimeError('You must use sudo to run this script')
  original_destination = argv[1]
  new_destination = argv[2] if len(argv) > 2 else get_ip_address('en0')

  print(f'Finding occurrences of \"{original_destination}\"')
  addresses_str = subprocess.check_output(['memwatch', 'Flycast.app', 'find', f'\"{original_destination}\"'])
  for line in addresses_str.splitlines():
    # line is like '(0) 00007FFF038500A0 (rw-)' (we care only about the address)
    tokens = line.split()
    if len(tokens) != 3:
      continue
    print(f'Replacing \"{original_destination}\" with \"{new_destination}\" at {tokens[1]} in Flycast')
    subprocess.check_call(['memwatch', 'Flycast.app', 'write', tokens[1], f'\"{new_destination}\" 00'])

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
