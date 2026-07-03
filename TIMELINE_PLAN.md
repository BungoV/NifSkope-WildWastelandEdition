# Timeline feature plan

Working copy: `E:\Projects\ClaudeNifskope` (fo76utils/nifskope develop + custom animation timeline).
Build: MSYS2 UCRT64 shell launched from PowerShell (`$env:MSYSTEM='UCRT64'; $env:CHERE_INVOKING='1'; & C:\msys64\usr\bin\bash.exe -lc "qmake6 && make -j 8"`).
Test files: `E:\Projects\Fallout 4 Mods\mods\X01Tesla\meshes\actors\powerarmor\x01\*_Tesla_VFX.nif` (autoLoop sequence, 11 lanes, 339 keys).

## Status legend
[x] done in v1 (2026-07-03) · [ ] to build

## v1 (shipped)
- [x] Timeline dock: lanes per controller/interpolator, keyframe diamonds, playhead sync, click-to-select both ways
- [x] Sequence combo (NiControllerSequence list) rebuilding lanes from Controlled Blocks
- [x] Graph pane sampling via Controller::interpolate (quadratic/const/linear correct), interp type labels
- [x] B-spline lanes as range bars; text key lanes with labels

## Display & layout
- [ ] Sync sequence selection with anim toolbar combo (GLView::setSceneSequence / sequenceChanged, match by name)
- [ ] Lane labels from controller: shortened type names (NiLightDimmerController -> "Light Dimmer"); shader float controllers show Controlled Variable enum text; resizable label gutter; hover marquee-scroll + tooltip for long names
- [ ] Toolbar row: sequence combo, filter box, transport (prev/play/next), time field, sec/frames@fps toggle, snap magnet + time/value steps, normalize toggle, follow-playhead toggle, inspector collapse; overflow via QToolBar
- [ ] Ruler: text-key markers (right-click add/rename/delete, Ctrl+,/. jump), preview/loop range band (drag to set, playback loops it), playhead
- [ ] Summary row (all keys aggregated, draggable columns)
- [ ] Controller Start/Stop range shading per lane; flag out-of-range keys; draggable range edges
- [ ] Frames mode (ruler + snapping at chosen fps, default 30)
- [ ] Lane filter box matching node name + controller type + sequence node names
- [ ] Auto-filter lanes by selected geometry node (viewport/block list selection), toggleable  (task #32)
- [ ] Mute (controller Active flag) / lock icons per lane
- [ ] Normalized graph view toggle
- [ ] Color gradient lane strips (color channels), alpha via checkerboard; composite alpha-driving float channels
- [ ] Bool lanes as on/off bars; float lanes as sparkline or 0-1 intensity strip; key shape by interp type (diamond=linear, circle=quadratic, square=const); per-lane override menu; cache strips
- [ ] Show sequence membership of selected node in inspector, clickable  (task #20)

## Editing
- [ ] Key inspector panel (right side, collapsible): Time, Value comps, Forward/Backward, TBC, text; ordered by workflow; edits undoable via model
- [ ] Controller properties in inspector: Start, Stop, Frequency, Phase, Cycle (Loop/Reverse/Clamp bits 1-2), Active (bit 3)
- [ ] Drag keys in lanes (time) and graph (time+value); snap toggle + steps; clamp between neighbors; single undo step per drag
- [ ] Delete key (Del), clear all keys (context menu)
- [ ] Insert key: double-click (sampled value), I = at playhead
- [ ] Tangent handles (draggable) for quadratic keys
- [ ] Multi-select: rubber band, shift-click; group drag/delete
- [ ] Copy/paste keys at playhead (also across compatible channels); Shift+D duplicate
- [ ] Scale selection around playhead (S key, numeric entry)
- [ ] Easing presets (context menu): Flatten / Smooth / Linearize / Ease in / Ease out
- [ ] Interpolation type switching per channel with translation (Linear->Quadratic slope-preserving tangents; drop/zero on downgrade; no XYZ<->Quat)
- [ ] Arrow-key nudge by snap step
- [ ] Copy whole channel keys between interpolators (type-compatible)  (task #31)

## Playback & navigation
- [ ] Space play/pause, Shift+Left/Right start/end, current-time field
- [ ] , / . prev/next key of selected lane
- [ ] Home = frame all, . (numpad) = frame selected
- [ ] Auto-follow playhead when playing

## Spells (Block List right-click)
- [ ] Animation > Setup controllers: per-block-type compatible controller list (multi-select), creates controller+interpolator+data with defaults; add to chosen/new NiControllerSequence (ControlledBlock names/links/priority); auto palette entry; transform via NiMultiTargetTransformController Extra Targets; create manager/palette if missing; supports adding already-rigged node to more sequences with fresh interpolators (optional clone of key data); single undo
- [ ] Animation > Remove from animation: checkbox list of ControlledBlocks per sequence, controllers/interpolators/data, palette entry, Extra Targets slot; cleanup orphans; single undo
- [ ] Animation lint: mismatched ControlledBlock Node Name strings, missing palette entries, out-of-range keys, unsorted times, null targets, freq 0, sequence range not covering keys, missing start/end text keys; clickable findings
- [ ] Rename sync: on node rename offer updating ControlledBlock strings + palette
- [ ] Duplicate sequence (deep copy, fresh interpolators/data); scale sequence times dialog
- [ ] Bake B-spline interpolator to keyframe interpolator (sample over start/stop)

## Viewport
- [ ] Solo/preview-only selected node subtree (toggle + shortcut), hides obstructing/transparent geometry  (task #29)
- [ ] Timeline key click expands + scrolls to key row in Block Details  (task #30)

## Interchange & persistence
- [ ] CSV export/import per interpolator; self-documenting header comments; validation on import; single undo
- [ ] QSettings persistence: snap, fps, mode toggles, inspector state, label width
- [ ] release/TIMELINE_SHORTCUTS.txt + release/TIMELINE_README.txt

## Portability
- [ ] Git repo: baseline = pristine fo76utils develop snapshot; feature/timeline branch; commit per phase; `git format-patch` gives portable series for future NifSkope versions
