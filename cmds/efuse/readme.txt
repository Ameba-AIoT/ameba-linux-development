[Description]

This is Realtek Amebad2 otp command bin, implemented as app module.

Run:
This efuse command is just an example for userspace.

Instructions:
efuse rraw/rmap [addr hex] [len]
efuse wraw/wmap [addr hex] [len] [val hex]

1. read physical test: efuse rraw 7f4 4
2. read logical test: efuse rmap 320 4
3. write physical test: efuse wraw 7f4 4 1a1b1c1d
4. write logical test: efuse wmap le0 4 10111213
