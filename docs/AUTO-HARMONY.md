# Auto harmony

Current source behavior; the existing 0.6-chase5 release package predates this
simplification.

## Playing

Open **GLO → Harmony** and turn **K1 On**. Use **K1** for On/Off:

- **Off**: normal playing, using the existing chord buttons and saved forms.
- **On**: each root plays a complete major or minor chord. Modifier buttons add
  the notes you select.

**K2 Reset → Defaults** restores all Harmony modifier assignments, inversions and
bass tones, and turns Harmony On. Turn K2 either way to reset. K3 and K4 are unused.

The same K1/K2 actions work on **HOME → Keyboard (6/6)** without leaving the guide.

On is the default for fresh settings; existing saved On/Off preferences are preserved. HOME retains Quantize, Key, Scale and Glide. Use Quantize
Basic or Full and a seven-note scale.

The scale chooses each root and its major/minor quality. Every automatic triad
has a normal fifth; Auto never chooses diminished or augmented chords by itself.
Select **7** for a major seventh on a major chord or a minor seventh on a minor
chord. In E major:

| Played degree | Without a modifier | With 7 selected | Notes with 7 |
| --- | --- | --- | --- |
| 1: E | E | Emaj7 | E G♯ B D♯ |
| 5: B | B | Bmaj7 | B D♯ F♯ A♯ |
| 7: D♯ | D♯m | D♯m7 | D♯ F♯ A♯ C♯ |

In a major key, degrees 1, 4 and 5 are major; 2, 3, 6 and 7 are minor.
Other seven-note scales use the scale third to choose quality: a third smaller
than four semitones gives minor, otherwise major. The chord's remaining intervals
come from that root, so some notes can be outside the selected scale. Full
quantization preserves those complete chords rather than snapping them back.

There are no Basic/Rich/Dreamy/Custom styles or global Auto voicing/modifier
behavior options. Old saved preferences for those options are ignored. Without
a modifier, Auto always plays a triad; extensions come only from the buttons.

Quantize Off and scales with other than seven notes use single-note fallback,
with **AUTO?** and a scale-selection message. Auto stays On so returning to a
supported scale resumes it. An extreme MIDI register that clamps the root outside
the scale also uses this fallback. MIDI Direct remains direct note playback.

## Buttons

Default Top assignments, left to right:

```text
6  7  9 | Sus2 Sus4 | Add2 11 Add9 | Major Minor | HOLD
```

The root mapping is unchanged. The device uses Top Lock: tap a modifier to latch
it; tap it again to clear it. A new modifier gesture replaces the previous one;
hold compatible buttons together to combine them. New roots inherit the selection.
Other supported layouts retain their normal latched or momentary behavior.

- **6** adds the major sixth.
- **7** adds the major seventh for major/augmented chords, the minor seventh for
  minor/diminished/power chords. A suspension keeps the underlying quality's seventh.
- **9** includes 7 and a normal ninth. **Add9** adds the ninth alone.
- **11** includes 7, 9 and 11. **13** includes 7, 9 and 13, without adding 11.
- **Sus2/Sus4** replace the third with a normal second/fourth. **Add2/Add4** keep
  the third.
- **Major/Minor** explicitly choose quality; 7 follows that choice. Dim, Aug and
  Power remain available only as explicitly assigned modifiers.

SEL K1 assigns a modifier. K3 sets that slot's inversion
(Root/Smooth/1st/2nd/3rd); K4 sets its bass tone. K2 is unused. HOLD is not editable
as a modifier. Turning Auto Off returns SEL to the original chord-form editor.

The most recently pressed quality wins; the most recently pressed suspension
wins. Quality and suspension can combine. Conflicting choices show `!` and are
excluded from active lighting. HOME names the actual generated chord.

Root-key LEDs light at full brightness for every pitch in the sounding Auto
chord that has a matching root key in the current key/scale mapping, across
octaves. The top keys continue to show modifier selection. Note lights follow
the generated voicing, stay on under HOLD, and clear when the chord stops.
Unsupported-scale single-note fallback and Auto Off keep their normal lighting.

HOLD still sustains roots independently of modifiers. A new root with none
physically down replaces sustained roots; turning HOLD off releases roots held
only by HOLD. Switching Auto On/Off clears modifier selection and inversion
history while preserving physical roots and HOLD sustain.

Existing RETRIG behavior remains: **Wait** queues modifier changes after its 50 ms
gather window until the next root; **Slide**, **Notes** and **Chord** use their
existing movement/attack policies. While HOLD sustains with no physical root
held, a modifier can change that chord immediately. In Wait, modifier lighting
describes the next chord while the name describes the sounding chord.

Auto generates at most six notes, matching the sequencer's capacity. Root,
third/suspension, seventh, altered fifth and colors take priority over the plain
fifth. Existing synth allocation and overload protection still apply.

## Recording and saved data

Auto recordings capture the actual notes and bass. They stay fixed after changing
Auto On/Off, modifiers, key or scale, including when pattern Follow is enabled.
Existing non-Auto Follow events keep their original behavior.

Legacy settings remain **188 bytes** and pattern events **22 bytes**. Saved
chord forms, sounds and CFQ6 patterns retain their formats. Auto preferences use
the existing checked/versioned **CHH1** record at **0xB4000/0xB5000**, with backup
object **68**. Retired option bytes stay reserved to preserve record compatibility;
they are cleared when loading. Missing/invalid data defaults to On; Safe Mode remains Off.
Old backups without object 68 leave existing Auto preferences alone.

Auto events retain the full interval mask using the high button-field tag and
rootkey 0xFF. Older firmware can still play their fixed notes.

## Verification

`sh tools/test_auto.sh` covers the exact Emaj7 → Bmaj7 → D♯m7 progression through
physical buttons, MIDI output, displayed names, recording and note releases in
both Basic and Full quantization. It also covers major/minor fixtures in multiple
keys/scales, all layouts, modifier pairs, retired preference migration, the
On/Off control, normal playback when Off, HOLD, retriggering and storage.

`sh tools/test_progressions.sh` checks suggestion names and uninterrupted audio.
`sh tools/test_usb.sh --full` checks Auto thirteenth chords plus bass at both capture
rates, with FX and output-mode variations. Hardware listening and performance
checks remain necessary before a release; host checks do not replace them.
