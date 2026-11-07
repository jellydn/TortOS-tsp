# Smart Pro port regeneration

This is new source based on upstream aafe6dee0d8221642e68d8b17fb17c6c5a39f8e1,
the retained offline-test-v2 runtime, and successful earlier port patches. It is
not an identical recovery of the lost checkout. It targets TG5040 Smart Pro,
not Smart Pro S. The image integration and donor binaries are separate inputs.

`make -f mk/native.mk DEVICE=smartpro` builds the launcher with the SDL/ALSA
port. The cross build selects the same source with `DEVICE=smartpro` and needs
a compatible development sysroot. Runtime SDL/DDK/kernel must not be replaced
with desktop build products. Brightness uses MENU+volume; the guide retains
Brick F1/F2 and Pixel2 FN labels. WiFi and Bluetooth capabilities return false.

`make -f mk/native.mk DEVICE=smartpro check-smartpro` exercises the public SDL
virtual-joystick path with asymmetric A/B, X/Y and overlapping hat/stick inputs.
The previous index1->ACCEPT table fails the B-back assertion. Only this port's
launcher A/B entries change. Diatom logical game controls and X/Y are unchanged.

The owner observed physical B selecting despite the B-back label. The delivered
launcher mapped index1 to ACCEPT, but no labeled physical SDL trace proves which
index each physical key produces. Pinned MinUI dbf89435a5179788418ff3573042cf9c7e75d9e0
declares A1/B0, so its stock mapping must not be called proof of this runtime.
The revised table is a hardware-test candidate, not a completed physical fix.
On the next test, press physical A once, then B once, without a game running;
record the labeled SDL button index and resulting accept/back action. A must
accept and B must back. If the indices differ, resolve this port's table from
that trace rather than swapping other platforms or emulator buttons.

The persistent offline image must keep SD identity guards, ext4 data, conditional
seed, READY gate, owned restarts, offline service restrictions and bounded
shutdown separately. This source commit does not claim those image gates passed.
