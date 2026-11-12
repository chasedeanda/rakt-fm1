# CORD Studio M1 listening build

This is the 4/4 Shoegaze / Dream Pop pilot on `codex/cord-studio-v1`. It uses the four existing FM-1 parts and the accompaniment's 96 PPQN clock. It reads the existing progression catalogue, retains the written qualities and slash bass of compatible sourced entries, and can color diatonic triads with a scale-compatible seventh or ninth. The initial tempo on a fresh session is 104 BPM.

## Current development build

Development packages use the CORD name in the filename and in firmware INFO. The zoomed panels use an amp faceplate with a cabinet grille, a signal lamp and dial pointers that track the actual values. The camera and shared instrument selection carry over when switching between Studio and Sound. The design takes its grouped controls and lamp from Spark and its front-panel treatment from the Neural DSP amp views. [Spark 2](https://help.positivegrid.com/hc/en-us/articles/47900829169165-Panel-Overview) · [Tone King](https://neuraldsp.com/manual/tone-king-imperial-mkii-x) · [Plini](https://neuraldsp.com/plugins/archetype-plini) · [Morgan](https://neuraldsp.com/plugins/morgan-amps-suite)

Studio’s overview puts key, tempo, scale and meter on K1–K4. The display stays visible across Studio, Sound, Record, Help and the mixer. It shows the current chord as a Roman numeral. A held manual chord takes precedence over the backing harmony. Studio supports 3/4, 4/4 and 6/8. Playing meter changes land at the next bar and key or scale changes land at the next chord.

The Studio overview puts the musician amps first, followed by color-coded Harmony, Shaping, Genre and ARP pedals. ALGORITHM moves through all eight panels, and SEQ is a shortcut through the pedals. Press SEL to zoom in. Turn PRESETS to change the genre. The [CORD Studio controls guide](RAKT-STUDIO-CONTROLS.md) explains the knobs, buttons and timing for all four workspaces.

The **Keys** footer and accent color identify which musician ALGORITHM has selected. The FM-1 keyboard plays that musician, and plays the current kit when Drums is selected. The highlighted musician’s sounding notes fade in and out on the key LEDs. Pitches outside the keyboard range fold by octaves, and Drums lights the upper hit keys. The ARP pedal on Chords or Layer offers Off, Keys and Band. Keys arpeggiates what you hold. Band arpeggiates that musician’s backing part on the Studio clock. SELECT switches the ARP Pattern and Timing pages.

The Sound amp shows all page names and the current page number. Turn SELECT inside the zoomed amp to move through Tone, Envelope, Sends and FX Presets. Drums offers Tone, Sends and FX Presets. Press SEL to zoom out from Tone, Envelope or Sends, where SELECT returns to the workspace tabs. Zoomed Studio uses SELECT for Part and Harmony. On FX Presets, K1 or PRESETS browses the shared effects bank, SEL applies, SAVE opens management and FX opens the shared EQ and effects editor. Tone shows the instrument or kit name. K1 on the Sound overview switches between Genre and All sounds.

With Genre selected, press EDIT on the Sound overview to load all four sounds for the current genre. On Tone, K1 or PRESETS browses the selected musician's sounds, including earlier choices. EDIT changes the pitched sounds right away and queues the kit for the next bar while playing. Levels, pan, sends and the shared FX preset stay in place.

| Genre | Chords | Bass | Drums | Layer |
| --- | --- | --- | --- | --- |
| Dream Pop | Cabinet Pad | Roundwire | Room | Glass Thread |
| Indie | Jangle Organ | Pickup Bass | Room | Glass Thread |
| New Wave | Pulse Stack | Sync Relay | X Tech | Crystal Step |
| Pop Punk | Amp Stack | Power Pick | Room | Wire Lead |
| Lo-Fi | Tape EP | Felt Sub | Tape | Dust Tines |
| Ambient Soundscape | Prism Cloud | Floor Swell | Glass | Halo Bloom |

GLO opens the mixer. Hold upper keys 1–4 to mute and 5–8 to solo in Chords, Bass, Drums, Layer order. Releasing the key restores the underlying mix. ALGORITHM selects the strip, the four knobs set levels, EDIT latches mute and SEL latches solo. On the mixer or zoomed Studio Drums panel, PRESETS left requests a fill and right drops or returns the drums on a bar boundary. SAVE opens Global settings from the mixer. HOME returns to Studio. Chord Lab is available through EDIT on Help’s zoomed Harmony panel. The development build retains the boot artwork and omits the legacy Home animation.

### Dream Pop after the hardware feedback

The first pass kept the groove and phrase shapes nearly constant across takes. The revised roll chooses a different arrangement feel, with new drum accents, bass phrasing, chord lengths, and Layer entries. Three feels build an eight-bar harmonic phrase from constrained degree and duration choices. A fourth draws from the CORD progression catalog. The audio transport and the player’s selected key stay in place. The rest of the genre profiles are first-pass material.

The genre work starts from CORD’s existing library and original composition rules. The follow-up desk review used Gordon Raphael’s account of *Is This It* for Indie, David Byrne’s account of *Remain in Light* for New Wave, Jerry Finn’s recording interview for Pop Punk, Questlove’s discussion of Dilla for Lo-Fi, and Brian Eno’s account of *Music for Airports* for Ambient. These are listening references and production accounts. The generator contains no song transcriptions or Spotify-derived data. [Raphael](https://insider.dbsinstitute.ac.uk/gordon-raphael-the-inside-story-on-producing-the-strokes) · [Byrne](https://lcweb2.loc.gov/static/programs/national-recording-preservation-board/documents/DavidByrneInterview.pdf) · [Finn](https://www.worldradiohistory.com/Archive-All-Audio/EQ-Magazine/EQ-2001-09.pdf) · [Questlove](https://www.redbullmusicacademy.com/lectures/questlove-new-york-2013/?template=RBMA_Lecture%2Ftranscript) · [Eno](https://www.abc.net.au/listen/programs/musicshow/brian-eno/3125602)

The continuous Dream Pop render makes three band rolls at approximately 0:18, 0:37 and 0:55. It uses the firmware audio engine. The on-device listening review is still open.

## First pilot notes

## How each part uses the JIANT reference

| Player | What we studied in JIANT | What CORD does |
| --- | --- | --- |
| Chords | `gvoice` searches inversions and registers for small voice movement, favors common tones and penalizes crowded low voicings. `gen_pads` chooses a repeating articulation figure for a style. | Uses CORD's inversion search and a register floor, carries shared pitches through a chord change, and plays seeded Sustain or Soft pulse figures with their own accent and gate contours. Added color on generic seeds comes from CORD's scale and masks. [Voicing](https://github.com/juanjiant-bit/JIANT/blob/main/firmware/src/gen.c#L296) · [Pad phrases](https://github.com/juanjiant-bit/JIANT/blob/main/firmware/src/gen.c#L809) |
| Bass | `gen_bass` resolves an archetype's notes over the chord, validates approaches against the next sounding note, varies weak events and relates the line to the kick grid. | Uses 12 original weighted riffs across Hook and Counterline. Riffs can double the kick, answer just after it, or sustain over it. Short diatonic or chromatic pickups aim at the next actual bass pitch, including slash notes and octaves. Phrase development changes the reply while keeping the opening anchor. [Bass generation](https://github.com/juanjiant-bit/JIANT/blob/main/firmware/src/gen.c#L670) |
| Drums | `gen_drums` uses hit priority and density, keeps stronger beats, adds lighter notes and fills, and offsets the backbeat. It exposes the kick grid for the bassist. | Keeps two shared kick anchors and a stable backbeat. Soft backbeat and Open hats choose original placements for secondary kicks, hats and rim fills. Activity brings in weaker events before changing the core rhythm, and the snare plays a few clock ticks late. [Drum generation](https://github.com/juanjiant-bit/JIANT/blob/main/firmware/src/gen.c#L425) |
| Layer | `gen_arp` plays a contour over voiced chord tones with repeating rests and gates. `gen_lead` develops a motif into an answering phrase. | Uses one independent voice owner for either Arp or Sparse motif. Seeded two-bar contours leave recurring gaps and change a tone in the reply. The part has its own register, accents and gates, and can regenerate under a held Chords part. [Arp generation](https://github.com/juanjiant-bit/JIANT/blob/main/firmware/src/gen.c#L871) · [Melodic phrase generation](https://github.com/juanjiant-bit/JIANT/blob/main/firmware/src/gen.c#L1083) |

JIANT's [genre data model](https://github.com/juanjiant-bit/JIANT/blob/main/firmware/src/gen_tables.c#L379) describes harmony, groove, playing styles and sound roles together. CORD's pilot applies that principle to one genre, using original phrases and existing CORD tones. The first Studio PLAY can offer Ghost Choir, Tape Sub and Dream Pluck on a fresh unit. Saved, edited, browsed and currently played parts keep their patches. The existing manual startup sounds remain available before Studio PLAY.

## Native workspaces

The fixed top row shows **Studio · Sound · Record · Help**. Turn **SELECT** to switch tabs, turn **ALGORITHM** to pick a musician, and press **SCL/SEL** to zoom in or out. The same musician follows you across Studio, Sound and Record. **HOME** returns to Studio overview. Motion Off snaps directly between views.

- **Studio** uses PLAY to start or stop the band. **EDIT** regenerates the band from overview or the focused musician in detail. The band change lands on the next bar. A single player can commit on the next sixteenth. Detail has style, activity, level and mute. OCT+/− keep their playing-octave action.
- **Sound** shows the four current sounds. Tone gives each part preset or kit, level and pan. Sends shows each musician’s drive, chorus, delay and reverb graphs. EDIT on the overview loads the genre palette and EDIT inside Tone changes the selected musician.
- **Record** shows the four generated phrase lanes with a playhead and a two-bar view in detail. REC starts a manual take. The Saved takes panel uses K1 to select a slot and a three-second SAVE hold to clear it. USB audio captures the full generated mix.
- **Help** includes a live Harmony lesson. Zoom into Harmony, play a key for its role, and use K1 to switch Auto or Manual. EDIT opens Chord Lab.

## Listening

`sh tools/test_studio.sh --listening` creates 10 eight-bar original takes and 10 matched Bass-only replacements in `build/studio/`. The regen request lands late in bar two. Pair 03 also has separate Chords, Drums and Layer replacements against the same original. `long-form.wav` is a 24-bar take for hearing phrase development. The examples come from the firmware host renderer and use the same level and settings within each comparison.

The traces compare note-on and note-off events on all four tracks. The host renders recorded zero overload sheds. Two seed pairs did record one or two voice-budget releases per take, so include those in the on-device check. Each replacement changes its own events and preserves the other three event streams. The USB checks compare host physical PCM and MIDI events with capture active and idle at 44.1 and 48 kHz. Listen on the FM-1 with guitar to judge whether the bass has a hook, the drums and bass share a pocket, and Chords and Layer leave enough room. Measure input latency and check audio deadlines on hardware during repeated mutes, regeneration and held manual notes.

## Adding a Studio genre

Add the profile to [`cf_studio_profiles.h`](../firmware/src/cf_studio_profiles.h). Give it a stable ID and choose its progression tags, two styles per part, sounds, and how it plays. The pocket composer uses the profile's bass riffs, chord and kick patterns, and part behaviors. Dream Pop and Pop Punk select their existing specialized composers through the same table.

Run `sh tools/test_studio.sh` to build and validate the host renderer. Run `build/host/cf_studio_test --list-genres` to list the registered IDs, or `python3 tools/render_studio.py --genre new-wave` to render one profile. The renderer writes a WAV, a note-event CSV, and a summary under `build/studio/listening-pass/`. The full listening pass discovers the same genres as the firmware picker.
