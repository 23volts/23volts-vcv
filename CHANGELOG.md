### 1.1.5 (2019-02-16)
- PolyMerge : fix ordering of polyphonic inputs

### 1.1.4 (2019-02-05)
- Morph & Multimap : Fix knob glitch when mapped value closes to 0
- Morph : Disable autolearn when X or Y CV input is connected

### 1.1.3 (2019-01-20)
- Morph : Add option to reverse Y AXIS for MIDI control
- Multimap : Fix bank switching's color update when using CV & FIRST inputs
- Multimap : Set ParamHandle transparency on preset load
- SwitchN1 : Using STEP INC input was causing selector to skip 1st step, now fixed.

### 1.1.2 (2019-01-12)
- Morph : fixed crash happening in module's browser

### 1.1.1 (2019-01-11)
- Multimap : Allow chain assignment. 
- Multimap : Mapping indicators are now displayed in transparency if they don't belong to the active bank. 
- Morph : Allow chain assignment.
- Morph & Multimap : Assigning parameters now doesn't "lock" the parameter on the target module.
- Morph : Add MIDI Learn for X & Y axis

### 1.1.0 (2019-01-03)

- Introducing **Multimap**, a versatile MIDI-Map utility with feedback.
- Introducing **MIDI-PC**,  an utility to send/receive MIDI Program Change messages.
- Introducing **MonoPoly**, a mono to poly signal converter.
- Introducing **PolyMerge**, a poly to poly merger.
- Introducing **PolySplit**, a poly to poly splitter.
- Introducing **Split4**, a 2x4 voices signal splitter.
- Introducing **Merge4**, a 2x4 voices signal merger.
- Cells : spawned cells are now more likely to appear in the center of the grid.
- Cells : increase the range of infinite loop detection.
- Switch N1 : Add Reset Input.
- Morph : A snapshot is now automatically written when XY pad is set on it.
- Morph : X + Y CV inputs now don't affect user's selected pad position.
- Morph : Add Assign mode, which allows directly mapping knobs to other VCV modules.
- All modules : skin update + layout & readability enhancements.

### 1.0.1 (2019-11-14)
- Cells : fixes crash that was happening when Algo CV was exactly 0v.
- Cells : fixed Density, now outputs correct value.
- Morph : fixes font path.

### 1.0.0 (2019-11-13)
Introducing **Cells**, **Morph**, **ClockM8**, **Mem**, **Merge8**, **Split8**, **SwitchN1**.