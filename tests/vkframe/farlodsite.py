"""Lane VKFAR28 (2026-11-16): the far LOD rings' app site must leave GL silent where Vulkan drew, and route the RT / DG keys.

python -I tests/vkframe/farlodsite.py <tree>
Reads src/features/far-lod/cellfarinst.cpp. Checks:
  1 the arm and the draw: `farLodVkArm(` (count == 1) and `if ( !farLodVkDraw( *k, cmds ) )` (count == 1);
  2 every GL submit of the rings' colour draw (`submit( r,` after the `farLodVkArm(` line, in the same function) sits on
    the line right after `if ( !farLodVkDraw(` (guarded == submits, submits > 0); the shadow cascades' submit
    (drawCascade, depth only, no Vulkan route) is printed as info;
  3 the RT / DG route (lane VKFAR26 edit): `farLodTwinOf( prog->wwDefs )` == 1, `farLodRtStateOf( a.fn, a.prog, r, a.v )`
    == 1, `farLodRtRun( r, a.v, why )` == 1, the ReSTIR GI refusal (`find( "RESTIRGI" )`) == 1, and no base-key-only
    refusal left (`the twin is the base key only` == 0).
Red (in memory, never on disk): `if ( !farLodVkDraw( *k, cmds ) )` replaced by `farLodVkDraw( *k, cmds );` must FAIL
check 2 (GL left drawing over Vulkan's rings).
Last line `FARLODSITE OK` or `FARLODSITE FAILED`.
"""
import os
import sys

GUARD = 'if ( !farLodVkDraw( *k, cmds ) )'


def colour_submits(text):
    """(submits, guarded, cascade): the rings' colour-draw GL submits, how many sit right under the guard, others."""
    lines = text.splitlines()
    arm = [i for i, ln in enumerate(lines) if 'farLodVkArm(' in ln and 'void' not in ln and 'bool' not in ln]
    submits = guarded = 0
    span = set()
    if len(arm) == 1:
        for j in range(arm[0] + 1, len(lines)):
            if lines[j].startswith('}'):
                break
            span.add(j)
            if lines[j].strip().startswith('submit( r,'):
                submits += 1
                if j > 0 and lines[j - 1].strip() == GUARD:
                    guarded += 1
    other = sum(1 for j, ln in enumerate(lines) if ln.strip().startswith('submit( r,') and j not in span)
    return submits, guarded, other


def check(text, label):
    arms = sum(1 for ln in text.splitlines() if 'farLodVkArm(' in ln and 'void' not in ln and 'bool' not in ln)
    guards = text.count(GUARD)
    submits, guarded, other = colour_submits(text)
    route = {
        'farLodTwinOf( prog->wwDefs )': (text.count('farLodTwinOf( prog->wwDefs )'), 1),
        'farLodRtStateOf( a.fn, a.prog, r, a.v )': (text.count('farLodRtStateOf( a.fn, a.prog, r, a.v )'), 1),
        'farLodRtRun( r, a.v, why )': (text.count('farLodRtRun( r, a.v, why )'), 1),
        'find( "RESTIRGI" )': (text.count('find( "RESTIRGI" )'), 1),
        'the twin is the base key only': (text.count('the twin is the base key only'), 0),
    }
    routeOk = all(n == want for n, want in route.values())
    ok = arms == 1 and guards == 1 and submits > 0 and guarded == submits and routeOk
    print('FARLODSITE %s: arms %d, guards %d, colour submits %d, under the guard %d, other submits %d (shadow cascades); route %s: %s'
          % (label, arms, guards, submits, guarded, other,
             ', '.join('%s %d' % (k, n) for k, (n, _) in route.items()), 'OK' if ok else 'FAIL'))
    return ok


RING_BL = 'if ( mesh->wwFarInstData() && ( !CellFarInst::bindlessTwinAllowed() || CellFarInst::vkRingsKeepBase() ) )'


def check_keep(rend, inst, label):
    """Lane VKFLIGHT4 check 4: the 11-16 flight drew its rings with fo4_cellcsmbl.prog (the bindless twin, key
    WW_CELL_FOGLEAK+WW_CELL_RESTIR+WW_CELL_RTDDGI+WW_CELL_RTDDGI_CASCADES+WW_FRAME_UBO = farLodTwinOf 1, the RT twin) and
    farLodVkArm refused all 10143 by name. renderer.cpp's bindless ask must refuse a far ring while vkRingsKeepBase()
    (the ring keeps fo4_cellcsm.prog), vkRingsKeepBase must read the rings' red and the view, and the refusal names
    the program."""
    n_ring = rend.count(RING_BL)
    n_def = inst.count('bool vkRingsKeepBase()')
    n_red = inst.count('WwVkPresent::shareWanted( "WW_VKVIEW_FARLOD_RED" )')
    n_name = inst.count('std::string( "far program " ) + std::string( prog->name )')
    n_rt = inst.count('"WW_CELL_FOGLEAK+WW_CELL_RESTIR+WW_CELL_RTDDGI+WW_CELL_RTDDGI_CASCADES+WW_FRAME_UBO"')
    ok = n_ring == 1 and n_def == 1 and n_red >= 2 and n_name == 1 and n_rt == 1
    print('FARLODSITE %s keep-base: renderer ring ask %d (want 1), vkRingsKeepBase %d, shareWanted(FARLOD_RED) %d (want >= 2), '
          'named refusal %d, flight key -> RT twin %d: %s' % (label, n_ring, n_def, n_red, n_name, n_rt, 'OK' if ok else 'FAIL'))
    return ok


def main():
    tree = sys.argv[1]
    path = os.path.join(tree, 'src', 'features', 'far-lod', 'cellfarinst.cpp')
    with open(path, 'r', encoding='utf-8', errors='replace', newline='') as fh:
        text = fh.read()
    with open(os.path.join(tree, 'src', 'gl', 'renderer.cpp'), 'r', encoding='utf-8', errors='replace', newline='') as fh:
        rend = fh.read()
    base = check(text, 'base')
    red = check(text.replace(GUARD, 'farLodVkDraw( *k, cmds );'), 'red gl-left-drawing')
    print('FARLODSITE red gl-left-drawing: %s' % ('FAIL (fired)' if not red else 'PASS (dead)'))
    keep = check_keep(rend, text, 'base')
    keepRed = check_keep(rend.replace(' || CellFarInst::vkRingsKeepBase()', ''), text, 'red bindless-ring')
    print('FARLODSITE red bindless-ring: %s' % ('FAIL (fired)' if not keepRed else 'PASS (dead)'))
    ok = base and not red and keep and not keepRed
    print('FARLODSITE %s' % ('OK' if ok else 'FAILED'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
