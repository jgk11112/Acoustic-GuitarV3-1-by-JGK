# Acoustic Guitar by JGK — Virtual Guitarist v0.5

A Windows VST3 acoustic-guitar instrument designed around playing complete guitar parts rather than triggering isolated samples.

## Main features in this build

- Full chord selection: 12 roots and Major, Minor, 7, Maj7, Min7, Sus2, Sus4 and Add9.
- Guitar-style six-string voicing instead of piano-style block chords.
- Down and up strumming with adjustable pick travel speed.
- Host-tempo pattern playback at 1/4, 1/8 or 1/16 speed.
- Pattern steps: Down, Up, Palm Mute, Choke, Scratch, Percussion and Rest.
- Fingerpicking performance mode with alternating bass and upper-string picking.
- Guitar-body percussion mode: body, thumb, knuckle and slap sounds.
- Adjustable Humanize, Dynamics, Palm Mute, Tone, Room, Tight/Loose and Output.
- Scratch controls for volume, length, timing and direction/type.
- Movable capo from fret 0 to 12. When off, the visual capo rests on the lyric paper; when enabled it moves onto the guitar neck.
- Chord pads for instant songwriting and auditioning.
- MIDI chord detection for use from FL Studio's piano roll or a MIDI keyboard.
- Custom JGK interface based on the approved guitar/sunset design.

## Build

The included GitHub Actions workflow builds the Windows x64 VST3 automatically.

## Install in FL Studio

After a successful GitHub Actions build, download the `Acoustic-Guitar-by-JGK-VST3` artifact and copy the `.vst3` bundle into:

`C:\Program Files\Common Files\VST3\`

Then open FL Studio -> Options -> Manage plugins -> Find installed plugins.
