NifSkope Animation Timeline - feature overview
===============================================
This build of NifSkope (based on fo76utils/nifskope) adds a Blender-style
animation timeline dock plus animation rigging tools. Everything below is
custom to this build. See TIMELINE_SHORTCUTS.txt for all key bindings.

THE TIMELINE DOCK (View > Show > Timeline)
  - One lane per controller/interpolator, labeled with the target node and a
    readable controller name ("Light03 - Light Dimmer", "Bolt01 - Effect
    Shader: U Offset"). Long names scroll on hover; the name column is
    resizable by dragging its edge.
  - Sequence dropdown: pick a NiControllerSequence to see its controlled
    blocks, synced both ways with the Animation toolbar's sequence combo.
  - Summary row aggregates all keys; clicking selects whole key columns.
  - Ruler: playhead scrubbing, text-key markers (right-click to add),
    Shift+drag preview/loop range, seconds or frames display.
  - Lane visuals by data type: color channels render as a real color gradient
    strip (checkerboard shows alpha), bool channels as on/off bars, float
    channels as sparklines or intensity strips; key marker shape encodes the
    interpolation type. Per-lane override in the right-click menu.
  - Controller Start/Stop range is shaded per lane; its edges are draggable;
    keys outside the range are drawn red (they will not play in engine).
  - Filter box matches node names, controller types and sequence entries.
    The target button auto-filters lanes to the geometry selected in the
    viewport or block list.
  - Mute (controller Active flag) and lock toggles per lane.

THE GRAPH PANE
  - Value curves for the selected lane, sampled with NifSkope's own
    interpolation code, so quadratic/constant/linear segments look exactly
    as they play. X/Y/Z/W and R/G/B/A are color coded; legend shows each
    channel's interpolation type.
  - Keys are draggable in time and value; quadratic keys show draggable
    tangent handles for the primary selected key.
  - Normalize toggle scales each curve to its own range for comparing shapes.

THE INSPECTOR (right side of the dock)
  - Key: time, per-component values, quadratic tangents, TBC parameters or
    text - all editable.
  - Channel: interpolation type dropdown (values are translated when
    converting, e.g. linear -> quadratic computes shape-preserving tangents),
    CSV export/import buttons.
  - Controller: Start/Stop Time, Frequency, Phase, Cycle (Loop/Reverse/
    Clamp) and Active - decoded from the Flags bitfield.
  - "In sequences": every sequence animating the selected node, clickable.

EDITING MODEL
  - Every edit goes through the undo stack: value edits merge per gesture
    (a whole drag is one Ctrl+Z), structural edits (insert/delete keys,
    rigging spells) are single snapshot undo steps.
  - Editing tools: insert (double-click / I), delete, duplicate, copy/paste
    keys at the playhead, whole-channel copy/paste between interpolators,
    multi-select with rubber band, scale around playhead, arrow-key nudge,
    easing presets (flatten/smooth/linearize/ease in/ease out).

CSV ROUND-TRIP
  - "Export channel(s) to CSV" writes a self-describing file (header
    comments explain the format) intended for editing by hand, script or AI.
  - "Import channel(s) from CSV" validates and rebuilds the key arrays.

RIGGING SPELLS (right-click in the Block List, Animation page)
  - Setup Controllers...: pick controller types valid for the block (light
    dimmer/color, shader float with controlled variable, shader color,
    alpha, transform, visibility), then either standalone or wired into an
    existing or new NiControllerSequence. Controller manager, multi-target
    transform controller, object palette and start/end text keys are created
    and updated automatically. Re-run it to add a node to more sequences
    (fresh interpolators per sequence).
  - Remove From Animation...: checklist of everything the manager knows
    about a node (controlled blocks per sequence, controllers, palette
    entry, extra target slot); removes what you tick and cleans up orphans.
  - Duplicate Sequence... / Scale Sequence Times...
  - Bake B-Spline To Keys...: converts a NiBSplineTransformInterpolator into
    an editable NiTransformInterpolator by sampling it.

DIAGNOSTICS
  - Timeline toolbar checkmark = animation lint: broken Node Name strings in
    controlled blocks (with a guided fix), missing palette entries, keys
    outside controller ranges, unsorted key times, zero frequencies,
    missing start/end text keys, degenerate sequence ranges.

VIEWPORT
  - Solo mode (Alt+Q or Render menu): renders only the selected node's
    subtree so transparent or obstructing meshes cannot block your preview.
    Follows the selection until toggled off.

SETTINGS
  - Snap, fps, frames mode, normalize, follow-playhead, label width and
    inspector visibility persist across sessions.
