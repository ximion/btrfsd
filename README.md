# Btrfsd - Tiny Btrfs maintenance daemon
[![Build Test](https://github.com/ximion/btrfsd/actions/workflows/build-test.yml/badge.svg)](https://github.com/ximion/btrfsd/actions/workflows/build-test.yml)

Btrfsd is a lightweight daemon that takes care of all Btrfs filesystems on a Linux system.

It can:
* Check for detected errors and broadcast a warning if any were found,
  or optionally send an email
* Perform scrub periodically if the system is not on battery
* Optionally run balancing operations periodically

The daemon is explicitly designed to be run on any system, from a small notebook to a large
storage server. Depending on the system, it should make the best possible decision for
running maintenance jobs, but may also be tweaked by the user.
If no Btrfs filesystems are found, the daemon will be completely inert.

Btrfsd is called every hour via a systemd timer with the lowest CPU priority, so its impact
on system performance should be extremely low.
You can tweak the daemon's default settings by editing `/etc/btrfsd/settings.conf`,
check `man btrfsd(8)` for more documentation.


# Installation

Btrfsd is packaged in a few Linux distributions for easy installation:

[![Packaging status](https://repology.org/badge/vertical-allrepos/btrfsd.svg)](https://repology.org/project/btrfsd/versions)


# Building

Btrfsd can be built & installed from source using the Meson build system.
It requires GLib, JSON-GLib, libsystemd, libmount and btrfs-progs.

On Debian-based systems, you can install all dependencies via:
```bash
sudo apt install btrfs-progs docbook-xsl libglib2.0-dev libjson-glib-dev libsystemd-dev meson xsltproc
```

You can the build the daemon:
```bash
mkdir build && cd build
meson setup --buildtype=debugoptimized ..
ninja
ninja install
```
