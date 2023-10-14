Version 0.2.1
-------------
Released: 2023-10-14

Bugfixes:
 * utils: Don't free result twice when checking if device is on battery

Version 0.2.0
-------------
Released: 2023-08-30

Features:
 * Don't run balance periodically by default
 * Autodetect btrfs executable location by default
 * Don't run in containers, run 5min after boot

Bugfixes:
 * scheduler: Take the launch time as reference time, so actions
   are performed correctly every hour and will not be ignored.
 * Don't make systemd timer persistent (so we are not run immediately
   after every boot)

Miscellaneous:
 * Add build/install instructions to README

Version 0.1.0
-------------
Released: 2023-08-24

Notes:
 * Initial release
