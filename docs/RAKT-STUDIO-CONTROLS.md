# RAKT Studio controls

Turn **SELECT** on an overview to switch between Studio, Sound, Record and Help. Turn **ALGORITHM** to move between units. Each tab keeps two neighboring full-width units in view and scrolls when the selection moves past an edge. Switching tabs recalls the rack position. **SEQ** jumps to the outboard gear in Studio and steps through its units.

The arrow and the light strip on the right show the selected unit and its position in the rack. The control strip below the bay shows K1–K4. Press **SEL** to zoom in. In detail, **SELECT** turns pages when a unit has them and switches tabs when it has one page. **ALGORITHM** moves between units. In the zoomed ARP panel, it switches between Chords and Layer. **HOME** backs out to the same unit. Press **HOME** again from the overview to return to Studio. **SEL** applies the shown choice on panels that say **SEL Apply**.

## Studio

**PLAY** starts or stops the band. The header shows key, scale, meter, tempo, the current chord and a segmented output meter. The chord readout follows a held manual chord while you play one.

### Rack design

The rack shares the startup screen's graphite rails, slotted screws, recessed displays, and four-pixel gaps. The units have their own faceplate heights, finishes, and controls.

| Unit | Hardware | Readout |
| --- | --- | --- |
| Chords | Bronze face, stitched grille, polished rotary caps | Style or patch and an amber signal lamp |
| Bass | Dark slate face and inset rubber knobs | Style or patch and a green activity display |
| Drums | Taller steel-grey face with a row of trigger lamps | Style or kit, plus Kick, Snare, Hats and Percussion hits |
| Layer | Slim plum face with horizontal faders | Style or patch and the part's level |
| Harmony, Shaping, Genre, ARP and Progression | Separate nickel, green, blue, violet and graphite plates | Beat, output, program, arpeggiator step and chord slot |
| Mix | Four-channel fader bank from the startup rack | Set levels and muted channels |
| Record | Full-width, two-bar lanes and a Saved Takes unit | Notes, hits, playhead and take slots |

The right-side rack index shows which units are visible. Tabs slide into the cabinet. A detail page slides its controls beneath the fixed faceplate. Motion Off settles the layout immediately.

### Genre

Highlight the Genre unit with **ALGORITHM** or **SEQ** and turn **K1** or **PRESETS** to choose a genre. **K2** selects a genre family. **PRESETS** also works from the Studio overview. Genre changes the harmonic and rhythmic vocabulary of the whole band, including the two playing styles available for each musician. A genre change queues a new take for the next bar during playback. Your current tempo continues while the band plays. Open **Sound → Sound Library** in Genre mode and press **SEL** to load the curated palette, or press **EDIT** on the Sound overview to cycle through its alternatives.

| Genre | Playing character |
| --- | --- |
| Dream Pop | Sustained or pulsing chords, melodic bass, a soft beat and sparse upper figures |
| Indie | Chopped or ringing chords, offbeat bass, room drums and a chime line |
| New Wave | Short chord stabs, octave bass, a machine beat and stepped synth figures |
| Pop Punk | Palm-muted attacks, open chords, bass riffs locked to the kick, half-time and straight grooves, and a clean picked Layer |
| Lo-Fi | Soft keys, a late bass pocket, half-time drums and short melodic fragments |
| Ambient Soundscape | Slow harmony, pedal bass, sparse percussion and evolving swells |

### Harmony and Shaping

Highlight **Harmony** with **ALGORITHM** or **SEQ** to change key, tempo, scale, or meter. Highlight **Shaping** for Space, Tempo, Energy, and Evolve. Turn **SELECT** within a zoomed musician to move through Part, Harmony, and Evolve.

| Page | Knob | What it changes |
| --- | --- | --- |
| Harmony | K1 Key | Transposes the band to a new tonic. During playback the change lands at the next chord. |
| Harmony | K2 Tempo | Adjusts the internal tempo. When following an external MIDI clock, the source controls the tempo. |
| Harmony | K3 Scale | Changes the scale used by harmony and the generator. During playback it lands at the next chord. |
| Harmony | K4 Meter | Chooses 3/4, 4/4 or 6/8. During playback it lands at the next bar. |
| Shaping | K1 Space | Scales the audible chorus, delay and reverb together. Raise the relevant send on Sound → Sends if a part is dry. |
| Shaping | K2 Tempo | The same tempo control as on Harmony. |
| Shaping | K3 Energy | Raises or lowers the effective Activity of free musicians around their current settings. 50% is neutral. A phrase can gain or shed events while keeping the edits Evolve has made. |
| Shaping | K4 Evolve | Sets how often one musician automatically varies its phrase. 0% keeps the current phrases and cancels automatic edits waiting to land. It considers one musician every 8 bars at 1–49%, every 4 bars at 50–79%, and every 2 bars at 80–100%. Ambient doubles those intervals. Held or muted musicians and musicians with Activity below 35% are skipped. |

Press **EDIT** on the Studio overview to generate a new band take for the next bar. Zoom into a musician and press **EDIT** to regenerate only that part on the next sixteenth-note grid point, or eighth-note grid point in 6/8. EDIT also rotates eligible sounds in the genre palette. Pitched patches switch at the musical boundary and drum kits switch at the next bar. Saved user patches, edited patches and sounds you are holding stay selected.

### Custom progression

Highlight **Progression** and press **SEL** to open it. Press **EDIT** to open your draft. The first draft starts with a one-bar tonic chord. In the editor, **ALGORITHM** picks a chord slot and **SELECT** changes pages. The chord page edits degree, quality, color and length. The other pages expose timing, inversion, bass and phrase actions. On **Phrase**, choose Insert or Duplicate with **K1** and press **SEL** to add a slot.

Press **PLAY** in the editor to use the draft for all four musicians. If the band is already playing, the new progression starts at the next bar. The band keeps the draft’s chords, durations and meter when you regenerate a part. Drafts support 3/4, 4/4 and 6/8 in Studio and checkpoint using the existing draft storage when the unit is idle. To switch sources, open the **Progression** faceplate, turn **K1** to Bank or My draft, and press **SEL**.

### Musician panel

| Knob | What it changes |
| --- | --- |
| K1 Style | Chooses between two ways of playing for the selected musician and genre. For example, Indie Bass offers Offbeat and Answer. |
| K2 Activity | Sets how busy that musician plays. The effect depends on the style and can include extra notes, accents and percussion hits. |
| K3 Level | Sets the musician’s mix level. This is the same level on Sound → Tone and Sound → Mix. |
| K4 Sustain on Chords | Sets the envelope level while a chord is held. Sound → Envelope → Release sets how long it rings after note-off. |
| K4 Touch on Bass and Layer | Sets the velocity of generated notes. 50% leaves the programmed velocity as played. |
| K4 Punch on Drums | Sets the velocity of generated drum hits. |

In **New Wave**, select **Bass → Moving pulse** to hear it follow Drums. A kick accents a bass note already on that step. An empty kick can pull a nearby bass note up to two steps earlier, leaving anchors and pickups in place. Switch Drums from **Machine** to **Open hats** to shift the kick and bass accent in the second bar. Evolve shows the resulting phrase under **Kick accents**. Bass follows the planned kick pattern through drum mutes and live fills. Hold keeps the current bass pattern when the drum groove changes.

In **Lo-Fi**, **Bass → Late pocket** places notes just behind kicks they share a step with. The delay follows the planned kick timing. The downbeat anchor, pickups, and offbeat notes keep their own timing. In 3/4 and 6/8, the groove carries the same kick and bass accents onto the shorter grid. Evolve labels the phrase **Behind the kick**. Hold keeps the current timing, and **Walk** plays its own phrase.

In **Indie**, select **Layer → Counterline** and **Chords → Chop** to hear the parts interact. Layer can move a note up to three grid steps to find room after a chord. It shortens the reply if the next chord is close. The Evolve page shows the resulting pattern under **Chord gaps**. Hold keeps that pattern while Chords changes. With **Ring**, Chords Band ARP, or a custom progression draft, Counterline plays its independent phrase.

Indie also has an eight-bar form. On bar 5, Layer rests and Drums leave out the hats while Chords, Bass, kick and snare carry the groove. The band comes back in on bar 6, then Drums play a turnaround in the second half of bar 8. The form starts over with each new take. It runs with Evolve at 0% and gives your live Fill, Drop and Return cues priority on Drums.

In **Lo-Fi**, **Layer → Fragments** plays a short reply in the first available space after the planned snare. It leaves the space open when a nearby key stab would crowd the reply. **Soft bell** keeps its own timing.

In **Dream Pop**, **Layer → Sparse motif** answers gaps in **Soft pulse** chords. With **Sustain**, the motif enters half a quarter-note after a harmonic change, on the next playable grid point. **Arp** keeps its own phrase.

In **Ambient Soundscape**, **Bass → Pedal** sustains the low note through the chord, and **Layer → Swells** brings in a soft chord tone after a harmonic change. The drums keep a quiet kick and leave space for a late percussion accent. **Slow contour** and **Distant notes** add more movement.

In **Pop Punk**, **Layer → Picked hook** plays short root and fifth notes in the gaps of the riff. **Octave answer** gives the second bar a higher reply. The last bar's tom turnaround follows the final shared bass-and-kick riff attack. Pickups and extra Double kick hits keep their own rhythm. Live drum cues take priority.

Style and Activity changes take effect on the next playable grid point. Continuous knobs take single steps when turned slowly and five-unit steps when turned quickly.

On the musician’s **Evolve** page, turn **K1 Hold** right to keep its current phrase through Energy and automatic Evolve changes. Turn K1 left to return the musician to **Free** and apply the current Energy setting. The two-bar Phrase display shows the generated pattern and its position in playback. The Studio panel shows **Held** while Hold is on. You can still edit Style or Activity or press **EDIT** to regenerate that musician. Hold then protects the edited phrase. Chord changes, arrangement cues, and live controls continue. Stop and PLAY keep Hold for the same take. A new band take or genre releases the holds when it lands.

### GLO Live

Press **GLO** to open the live keyboard controls. **ALGORITHM** switches between Map and State, and **SELECT** adjusts tempo. Press **GLO** or **HOME** to return to the previous panel, or **SAVE** for Settings.

| Keys | Action |
| --- | --- |
| First four black keys | Toggle mute for Chords, Bass, Drums and Layer |
| First four white keys | Solo those musicians while held |
| Black keys 7–10 | Toggle kick, snare, hats and percussion |
| C4 / D4 | Pause or resume / restart |
| E4 / F4 | Unmute all / tap tempo |
| G4 / A4 | Fill while held / queue a one-bar fill |
| B4 | Drop or bring back drums at the next bar |

Pause, restart and tempo follow the external clock when MIDI clock input is active. From the zoomed Studio Drums panel, **PRESETS** left queues a fill and right drops or returns the drums at the next bar.

## Sound

Open **Mix** for Chords, Bass, Drums and Layer levels on K1–K4.

Open **Sound Library** at the end of the Sound rack. **K1 Library** selects the short Genre list or All sounds. Highlight a musician and use **K1 Preset** to browse its tones or kits. With Genre selected, press **SEL** to load the complete palette. Press **EDIT** on the Sound overview or in Sound Library to choose the next tone or kit for each musician. In a musician’s Tone panel, **EDIT** changes that musician only. While playing, the pitched sounds change immediately and the drum kit changes at the next bar. The palette keeps the current levels, pan, sends and each musician’s FX settings.

A highlighted Sound musician shows the Tone controls. Zoom in and turn **SELECT** to reach the other named pages. The page counter shows where you are.

| Page | K1 | K2 | K3 | K4 |
| --- | --- | --- | --- | --- |
| Tone | Preset or drum kit | Mix level | First synth macro, such as filter cutoff on VA tones | Pan |
| Envelope | Attack, the time to reach full level | Decay, the time to fall toward Sustain | Sustain, the held level | Release, the time to fade after note-off |
| Sends | Drive amount | Chorus send | Delay send | Reverb send |
| FX Presets | Browse FX presets for this musician | | | |

Drums has Tone, Sends and FX Presets. **PRESETS** also browses the sound or kit. Sends shows a preview of the selected musician’s drive curve, chorus sweep, delay taps and reverb tail. Each graph responds to that musician’s knob. Press **FX** from any rack tab to open FX Presets. Turn **K1** or **PRESETS** to highlight a preset and press **SEL** to apply it to the highlighted musician. Turn **ALGORITHM** to switch musicians. Each musician keeps its own FX chain through patch changes, new takes, and restarts. Use **SELECT** to turn to Sends or another Sound page. **SAVE** opens preset management. Press **EDIT** on FX Presets to open the EQ and effects editor for the loaded preset. **LFO** from a zoomed pitched musician opens its detailed synth editor.

## Arpeggiator

Press **ARP** to open the Pattern page directly, or find the ARP unit with **SEQ** and open it with **SEL**. The panel starts on Chords when Bass or Drums was selected. Turn **ALGORITHM** to switch between Chords and Layer. **K1 Arp For** chooses Off, Keys or Band. Keys arpeggiates the notes you hold. Band arpeggiates that musician’s generated chord tones with the transport running. **K2 Mode** selects the note order or strum mode, **K3 Octave** sets the range, and **K4 Rate** sets the clock division or the spacing in milliseconds for a strum.

Turn **SELECT** to the timing page. **K1 Gate** sets how much of each step a note occupies, **K2 Swing** shifts alternating steps, **K3 Latch** keeps the arpeggio going after key release, and **K4 Glide** sets the Chords glide time.

## Record and Help

**Record** shows four lanes and the playhead. **SEL** opens a two-bar view of the highlighted musician. Press **REC** to record all four parts and press it again to finish. Hold **REC** to discard the current take. Highlight **Saved Takes** at the end of the rack and turn **K1** to select a take. **PLAY** starts or stops playback. Hold **SAVE** for three seconds to clear the selected take. Use USB audio capture to record the full mix as audio.

**Help** contains Start here, Harmony, Playing and Capture. Turn **ALGORITHM** to select a topic and **SEL** to open it. The Harmony lesson shows the role of the note you play. **K1** switches between Auto and Manual harmony. **EDIT** opens Chord Lab.
