# RAKT power-on rack

![RAKT boot animation](images/rakt-boot/boot.gif)

The FM-1 opens on a graphite faceplate with a cream RAKT wordmark. The mixer and
main output slide into the cabinet from opposite sides. Chords, Bass, Drums and Layer
run through a short lamp sweep, then MAIN OUT displays READY. The visual intro takes
1.68 seconds after initialization. A key, button or encoder skips it. Motion Off
shows the finished rack during startup. The optional Dm7 boot strum is controlled by the Boot Snd setting.

The 240 × 240 intro uses the indexed framebuffer and integer drawing primitives.
The settled frame is cached during the hold. The host test exercises animation,
clipping, Motion Off, Safe Mode, input skip, palette restore and a delayed startup.

```sh
python3 tools/build.py --gen-only
sh tools/test_rakt_boot.sh
```

The test exports native frames to `build/rakt-boot/`. The still used by the Firmware
Hub is [the READY frame](images/rakt-boot/ready.png).
