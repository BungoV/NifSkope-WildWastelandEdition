"""The directions a card's frames were photographed from, printed as the
`WW_IMPOSTOR_ORBIT_VIEWS` list.

A frame of an octahedral card IS an orthographic photograph of the mesh taken
along one direction. Viewed again from that same direction the card must
reproduce the photograph: the neighbouring frames carry barycentric weight
zero, and the parallax step moves the sample along the view ray, which under an
orthographic camera at a bake direction is antiparallel to the frame's own
forward -- so it moves the sample along an axis `frameUvOf` projects away.
Nothing about the height channel can change the picture there.

That makes the bake directions the one place where the drawer has a KNOWN
ANSWER, and the answer comes from the mesh, not from the card. The directions
themselves come out of the `.lodm`'s own grid size by the hemi-octahedral map
the bake used, so this script cannot flatter a card either.

    python impostor_bake_views.py <id>_oct.lodm      -> az:el,az:el,...
"""
import sys, os, json, math


def views(N):
    out = []
    for j in range(N):
        for i in range(N):
            u = i / float(N - 1) * 2.0 - 1.0
            v = j / float(N - 1) * 2.0 - 1.0
            x = (u + v) * 0.5
            y = (u - v) * 0.5
            z = 1.0 - abs(x) - abs(y)
            n = math.sqrt(x * x + y * y + z * z)
            x, y, z = x / n, y / n, z / n
            el = math.degrees(math.asin(max(-1.0, min(1.0, z))))
            az = math.degrees(math.atan2(y, x)) % 360.0
            out.append('%.4f:%.4f' % (az, el))
    return out


def main(argv):
    if len(argv) < 2:
        print('usage: impostor_bake_views.py <id>_oct.lodm')
        return 2
    raw = open(argv[1], 'rb').read()
    j = json.loads(raw[raw.index(b'{'):].decode('utf-8'))['card']
    v = views(int(j['oct']))
    # duplicates are real: the hemi-octahedron folds its four corners onto the
    # same direction, and a list with them removed would not be "one view per
    # frame" any more. The caller asserts the COUNT, so keep every one.
    sys.stdout.write(','.join(v))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
