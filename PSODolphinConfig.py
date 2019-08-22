#!/bin/env python3

import argparse
import os
import pwd
import re
import stat
import subprocess
import sys
import time


def change_user(username):
  try:
    user_info = pwd.getpwnam(username)
  except KeyError:
    print("user %s does not exist" % (username,))
    raise
  os.setregid(user_info.pw_gid, user_info.pw_gid)
  os.setreuid(user_info.pw_uid, user_info.pw_uid)


def main(argv):
  parser = argparse.ArgumentParser(description="Runs Dolphin as a non-privileged user with access to the privileged tap0 network interface. Run this script with sudo; it will run Dolphin as the user you sudo'ed from and provide it access to the tap interface. It will also automatically configure the tap interface when Dolphin first opens it.")
  parser.add_argument(
    "--dolphin-path", "-d",
    action="store",
    dest="dolphin_path",
    default="./Dolphin.app/Contents/MacOS/Dolphin",
    help="Path to Dolphin executable. Defaults to Dolphin.app/Contents/MacOS/Dolphin in current directory.",
  )
  parser.add_argument(
    "--memwatch-path", "-m",
    action="store",
    dest="memwatch_path",
    default="memwatch",
    help="Path to memwatch executable. Defaults to memwatch (assumes it's installed somewhere on $PATH).",
  )
  parser.add_argument(
    "--tap-interface", "-I",
    action="store",
    dest="tap_interface",
    default="/dev/tap0",
    help="Tap interface to use. Defaults to /dev/tap0.",
  )
  parser.add_argument(
    "--tap-ip", "-i",
    action="store",
    dest="tap_ip",
    default="192.168.0.5/24",
    help="IP address and subnet bits to assign to tap interface. Defaults to 192.168.0.5/24. This should match the gateway and DNS server addresses configured in PSO's network settings.",
  )
  args = parser.parse_args()

  try:
    username = os.environ['SUDO_USER']
    print(username)
  except KeyError:
    print('$SUDO_USER not set; use `sudo -E`')
    return 1

  tap_match = re.match(r'^/dev/tap([0-9]+)$', args.tap_interface)
  if tap_match is None:
    print('tap interface name must begin with /dev/tap')
    return 1
  tap_interface_number = int(tap_match.group(1))
  tap_name = 'tap%d' % (tap_interface_number,)

  # 1. open tap and configure it
  print("starting and configuring " + tap_name)
  tap_fd = os.open(args.tap_interface, os.O_RDWR)
  os.set_inheritable(tap_fd, True)
  subprocess.check_call(['ifconfig', tap_name, args.tap_ip], stderr=subprocess.DEVNULL)
  subprocess.check_call(['ifconfig', tap_name, 'up'], stderr=subprocess.DEVNULL)

  # 2. fork a Dolphin process, dropping privileges first
  print("starting dolphin")
  dolphin_proc = subprocess.Popen([args.dolphin_path],
      preexec_fn=lambda: change_user(username), pass_fds=(tap_fd,))
  time.sleep(1)

  # 3. create a temp file for dolphin to open instead of /dev/tapN
  print("creating temp file")
  tmpfile_fd = os.open("/tmp/dnet", os.O_CREAT | os.O_RDWR, stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO)
  os.close(tmpfile_fd)
  os.chmod("/tmp/dnet", stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO)

  # 4. modify dolphin's memory to make it upen the temp file
  print("redirecting /dev/tap0 for dolphin")
  addresses_str = subprocess.check_output([args.memwatch_path, str(dolphin_proc.pid), 'find', '\"/dev/tap0\"'], stderr=subprocess.DEVNULL)
  for line in addresses_str.splitlines():
    tokens = line.split()
    if len(tokens) != 3:
      continue
    print("redirecting /dev/tap0 to /tmp/dnet at %s in dolphin" % (tokens[1],))
    subprocess.check_call([args.memwatch_path, str(dolphin_proc.pid), "access", tokens[1], "rwx"], stderr=subprocess.DEVNULL)
    subprocess.check_call([args.memwatch_path, str(dolphin_proc.pid), "write", tokens[1], "\"/tmp/dnet\""], stderr=subprocess.DEVNULL)
    subprocess.check_call([args.memwatch_path, str(dolphin_proc.pid), "access", tokens[1], "r-x"], stderr=subprocess.DEVNULL)

  # step 5: use lsof to find out when dolphin opens /tmp/dnet
  print("waiting for temp file to open")
  dolphin_tap0_fd = -1
  while dolphin_tap0_fd < 0:
    time.sleep(1)
    result = subprocess.check_output(["lsof", "-p", str(dolphin_proc.pid)], stderr=subprocess.DEVNULL)
    for line in result.splitlines():
      if b'/tmp/dnet' not in line:
        continue

      fd_str = line.split()[3]
      dolphin_tap0_fd = int(fd_str[0:-len(fd_str.lstrip(b'0123456789'))])
      print("found open tap fd %d in dolphin" % (dolphin_tap0_fd,))

  # step 6: use memwatch to move the tap fd into place
  print("replacing temp fd %d with tap fd %d in dolphin" % (dolphin_tap0_fd,
      tap_fd))
  assembly_contents = b"""start:
  mov rax, 0x000000000200005A  # dup2(from_fd, to_fd)
  mov rdi, %d
  mov rsi, %d
  syscall

  # close the original fd. note that rdi is preserved during the syscall so we
  # don't need to reload it
  mov rax, 0x0000000002000006  # close(fd)
  syscall

  ret
""" % (tap_fd, dolphin_tap0_fd)
  subprocess.run([args.memwatch_path, str(dolphin_proc.pid), "--", "run", "-"], input=assembly_contents, stderr=subprocess.DEVNULL)

  # ok we're done; dolphin is running as a non-privileged user with the tap open
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
