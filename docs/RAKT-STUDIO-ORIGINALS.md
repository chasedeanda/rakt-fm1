# RAKT Studio Originals

Eighteen original VA patches give each Studio genre a new Chords, Bass and Layer trio. Find them in **Sound → Tone** with **Genre** selected. They follow the genre’s earlier featured sounds in the part’s K1 browse order and enter the same **EDIT** shuffle. **Sound Library → SEL Apply** continues to recall the featured set.

| Genre | Chords | Bass | Layer |
| --- | --- | --- | --- |
| Dream Pop | Glide Haze | Velvet Pick | Slow Spark |
| Indie | Twin Chops | Palm Wire | Bridge Chime |
| New Wave | Afterimage | Mono Shift | Chrome Arp |
| Pop Punk | Stack Bite | Break Pick | Clean Octave |
| Lo-Fi | Worn Reed | Dust Round | Grain Keys |
| Ambient Soundscape | Tidal Drone | Deep Anchor | Air Lattice |

## Design references

- **Dream Pop** uses independent pitch drift on two oscillators for Glide Haze and a slower upper envelope on Slow Spark. Velvet Pick holds a centered low sine under a brief pulse transient. The pitch movement and softened onset translate aspects of Kevin Shields’s glide-guitar account into VA synthesis. [Kevin Shields interview](https://www.fender.com/articles/fender-performances/my-bloody-valentines-kevin-shields-on-the-jazzmaster-and-his-iconic-albums)
- **Indie** pairs Twin Chops’ short, driven chord envelope with Bridge Chime’s upper-register transient. Palm Wire keeps the bass body mostly fundamental while a higher partial adds the pick. The two upper sounds were designed to leave space for the separate lines Gordon Raphael describes on *Is This It*. [Gordon Raphael interview](https://insider.dbsinstitute.ac.uk/gordon-raphael-the-inside-story-on-producing-the-strokes)
- **New Wave** pairs pulse-width and amplitude modulation in Afterimage, envelope-driven pulse width and resonance in Mono Shift, and a short ring-modulated accent in Chrome Arp. The synth technique comes from period PWM controls and the role separation follows the layered repetitive parts David Byrne describes. [Roland JUNO-60 manual](https://cdn.roland.com/assets/media/pdf/JUNO-60_OM.pdf) · [David Byrne interview](https://lcweb2.loc.gov/static/programs/national-recording-preservation-board/documents/DavidByrneInterview.pdf)
- **Pop Punk** uses short, driven chord strokes in Stack Bite, a bright pick attack over a low sine in Break Pick, and a clean, short octave voice in Clean Octave. Tom DeLonge’s account of complementary guitar and bass parts informed the split between sustained weight and attack. [Tom DeLonge interview](https://www.musicradar.com/news/tom-delonge-interview-blink-182)
- **Lo-Fi** gives Worn Reed independent pitch and amplitude drift, gives Dust Round a stable low fundamental with a decaying octave, and adds quiet vinyl impulses and pitch motion to Grain Keys. Tape saturation and time variation informed the recipe choices. [Strymon Deco manual](https://www.strymon.net/manuals/Deco_v2_UserManual_RevD.pdf)
- **Ambient Soundscape** lets Tidal Drone’s oscillator shapes move at different rates. Deep Anchor holds a stable pitch with a slower upper partial, and Air Lattice lets sparse upper partials bloom over time. The slow changes follow Brian Eno’s account of steady-state texture in his ambient work. [Brian Eno interview](https://www.abc.net.au/listen/programs/musicshow/brian-eno/3125602)

## Original release measurements

These measurements describe the first release of the originals pack. The revised featured tones and Clean Octave are included when the pack is regenerated.

The solo pairs use the RAKT firmware audio engine with the shared FX bypassed and fixed levels, pan, pitches and velocity. The nine-second fixture holds a four-note chord or one role-specific note for 2.5 seconds, then plays three short notes at increasing velocities. “Attack” measures RMS at 25–125 ms relative to the 1.6–2.4 s hold. “Tail” measures 100–500 ms after key-up against the hold. “Upper energy” is the first-order high-pass (1 kHz for Chords and Layer, 200 Hz for Bass) energy ratio on the same early window. The bass harmonic figures compare power in harmonics 2–8 to the fundamental of MIDI note 36 with a Hann window and bins within 4% of each harmonic.

| Original | Featured comparison | Measured difference in the solo pair |
| --- | --- | --- |
| Glide Haze | Cabinet Pad | Attack 7.1 dB lower relative to its hold and upper energy 3.7 dB higher |
| Velvet Pick | Roundwire | Sustained bass harmonics 14.9 dB lower relative to the fundamental |
| Slow Spark | Glass Thread | Attack 2.2 dB lower relative to its hold and upper energy 1.8 dB higher |
| Twin Chops | Jangle Organ | Attack 3.6 dB higher relative to its hold |
| Palm Wire | Pickup Bass | Sustained bass harmonics 11.8 dB lower relative to the fundamental |
| Bridge Chime | Glass Thread | Tail 45.2 dB lower relative to its hold |
| Afterimage | Pulse Stack | Attack 2.1 dB lower relative to its hold, with a 6.0 dB RMS range during sustain |
| Mono Shift | Sync Relay | Attack 2.4 dB higher relative to its hold and a 5.6 dB higher peak |
| Chrome Arp | Crystal Step | Attack 6.8 dB higher and tail 34.9 dB lower relative to its hold |
| Stack Bite | Barbed Amp | Hold 2.4 dB stronger with a peak 3.8 dB lower |
| Break Pick | Rust Pick | Peak 3.1 dB higher and held level 3.2 dB lower |
| Octave Cry | Fret Howl | Tail 29.9 dB lower relative to its hold |
| Worn Reed | Tape EP | Tail 20.5 dB lower relative to its hold |
| Dust Round | Felt Sub | Sustained bass harmonics 23.6 dB higher relative to the fundamental |
| Grain Keys | Dust Tines | Hold 7.0 dB stronger and tail 30.9 dB lower relative to that hold |
| Tidal Drone | Prism Cloud | 8.6 dB RMS range during sustain, 1.8 dB more than the featured pad |
| Deep Anchor | Floor Swell | Attack 10.4 dB higher relative to its hold |
| Air Lattice | Halo Bloom | Attack 11.5 dB lower relative to its hold |

The four-bar band pairs use seed 102 with the same genre, tempo, progression, drum kit, part levels and FX settings. Across the six comparisons, all 1,356 MIDI note-on/off events matched and the audio differed in every pair. Each new mix rendered with zero clipped samples, overload sheds and voice-budget kills.

| Band pair | Matched MIDI note events | Originals peak |
| --- | ---: | ---: |
| Dream Pop | 190 | −14.1 dBFS |
| Indie | 270 | −14.1 dBFS |
| New Wave | 330 | −14.1 dBFS |
| Pop Punk | 286 | −14.1 dBFS |
| Lo-Fi | 222 | −14.1 dBFS |
| Ambient Soundscape | 58 | −17.4 dBFS |

Factory IDs 131–148 append to the VA list. The later Pick Chime, Clean Octave, Still Air and Soft Halo occupy IDs 149–152. The Studio test checks genre membership, parameter ranges and distinct patch data. The linked FM-1 preview uses 551,228 bytes of XIP, with 93,424 bytes of ordinary RAM and 316,792 bytes of the shared pool. Musical judgment and real-time performance on physical hardware are still open for audition.

## Reproduce the listening pack

Run these commands with the generated firmware headers in `build/gen`.

    sh tools/test_studio.sh
    python3 tools/render_studio.py --originals

The renderer writes 36 solo WAVs, 12 band WAVs, paired MIDI-event traces, and `evidence.json` with the raw measurements and hashes. The listening archive is `build/studio/originals/cord-studio-originals.zip`.
