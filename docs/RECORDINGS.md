# Three recent recordings

From Studio, press **REC** to capture Chords, Bass, Layer and Drums. From a stop,
the band and recording start together. During playback, REC arms the next
downbeat and the band keeps playing. Generated notes, arpeggios, live keys and
drum hits are captured with their timing and velocity. Takes also keep the
starting tempo, meter, part levels, Space and pitched sound selections.

Press **REC** again to finish and stop the band. Playback loops in whole bars,
with silence after an early stop. A take ends automatically after eight bars or
when it fills the event buffer. Hold REC to discard the current take and keep
playing. Cancellation preserves all previous recordings.

The default keeps the **latest three** completed takes. The newest is always
**1**, the previous 1 becomes 2, and the previous 2 becomes 3. A successful fourth
take replaces the oldest. An empty or cancelled take does not push anything out.
The ordering survives a restart without copying all three patterns around RAM.

In the **Record** rack, turn ALGORITHM to **Saved Takes**. Turn K1 to choose
Recording 1, 2 or 3, then press PLAY to start or stop it. The lanes show the
captured notes and hits. Older manual recordings also play from this rack.

Recording data saves automatically after playback stops and the instrument has
been quiet briefly. Stop playback before powering off. No separate SAVE gesture
is required to keep a completed take. The brief quiet interval keeps flash-sector
erases out of live playback.

## Keep recordings until cleared

In **GLO → Recordings**, turn **AUTO DEL** Off. A full set then refuses another
take and tells you to clear a recording first. With AUTO DEL On (the default),
completed new takes replace the oldest automatically.

To clear one manually, stop playback, select it in Saved Takes and **hold SAVE for
three seconds**. Releasing early or selecting a different recording cancels the
clear gesture. Clearing has no Undo. Holding REC cancels a new take; it does not
undo earlier recording or editing changes. Variation retains its separate small
previous-result control.

## GarageBand and backups

Use USB audio to record the live stereo mix, including effects and knob moves.
Takes replay note and drum events using the selected sounds and current effects.
Firmware backups preserve device data for restore on an FM-1.

Use the updated web or Python installer to back up the new recordings. An older
installer may not recognize their new backup IDs. Legacy A–H pattern/song objects
remain archived and exportable; A–C initially populate the three recent slots.
D–H and the old arrangement are never silently mapped into the three live slots.
An older firmware plus the original backup can recover them. Existing D–H sound
references and tweaked-sound records are retained rather than reclaimed.

## Developer notes

The runtime take count is three and the legacy archive count remains eight.
`CTK1` version 2 stores a variable-length performance of up to 940 events
at 96 ticks per beat. Version 1 stores the 2,556-byte manual pattern record
and remains readable. Logical age is separate from physical slot IDs.
Backup IDs 71–73 hold the three physical
slots; 74 holds the checked recording policy. Modern restore clients apply old
archive chunks before active takes regardless of JSON object order.

Each recording uses one existing storage A/B transaction in sectors E6000–EBFFF;
policy uses EC000–EDFFF. These allocations are beyond updater staging E0000–E4FFF
and below globals FC000–FEFFF. Firmware/update-loader addresses and all legacy
object addresses are unchanged. Backup LIST/PUT wait during a live replacement;
unfinished data is not saved over an earlier take. Unknown recording versions are
kept unavailable until explicitly cleared, rather than silently resurrecting an
older archived take.

`sh tools/test_recordings.sh` runs the focused recording, storage power-cut,
flash-boundary and web/Python restore-order specs. The full feature regression
suite runs in CI. Physical device listening/input/timing remains a release check.
