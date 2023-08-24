# Btrfsd - Tiny Btrfs maintenance daemon
[![Build Test](https://github.com/ximion/btrfsd/actions/workflows/build-test.yml/badge.svg)](https://github.com/ximion/btrfsd/actions/workflows/build-test.yml)

Btrfsd is a lightweight daemon that takes care of all Btrfs filesystems on a Linux system.

It will:
 * Check `stats` for errors and broadcast a warning if any were found, or send an email
 * Perform `scrub` periodically if system is not on battery
 * Run `balance` (rarely, if system is not on battery)

The daemon is explicitly designed to be run on any system, from a small notebook to a large
storage server. Depending on the system, it should make the best possible decision for
running maintenance jobs, but may also be tweaked by the user.
If no Btrfs filesystems are found, the daemon will be completely inert.

Btrfsd is called every hour by a systemd timer with the lowest CPU priority, so its impact
on system performance should be extremely low.
You can tweak the daemon's default settings by editing `/etc/btrfsd/settings.conf`.
