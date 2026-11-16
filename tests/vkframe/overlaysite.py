"""Lane VKFLIGHT4 item 1, site check: the app wires the QPainter overlay to the Vulkan blend.
glview.cpp: the QPainter's device comes from WwVkPresent::painterTarget into a named variable, and painterDone gets that
variable after painter.end() (before it, the image would be read half painted). vkpresent.cpp: painterTarget returns
overlayTarget's image before the GL paint device, and frame() says the `vk overlay:` line.
python -I overlaysite.py <tree>: reads the files as they are; the red is the same call on a copy of the files from a
revision before the wiring (src/glview.cpp + src/features/vkframe/vkpresent.cpp under <copy>/), which must FAIL."""
import re
import sys


def read(p):
    with open(p, 'rb') as f:
        return f.read().decode('utf-8', 'replace').replace('\r\n', '\n')


def main():
    root = sys.argv[1]
    glv = read(root + '/src/glview.cpp')
    vkp = read(root + '/src/features/vkframe/vkpresent.cpp')
    fails = 0
    checks = 0

    def check(ok, name, said):
        nonlocal fails, checks
        checks += 1
        print('OVERLAYSITE %s %s: %s' % (name, 'OK' if ok else 'FAIL', said))
        if not ok:
            fails += 1

    m = re.search(r'QPaintDevice \* (\w+) = WwVkPresent::painterTarget\( this \);\s*\n\s*QPainter painter\( (\w+) \);', glv)
    check(bool(m) and m.group(1) == m.group(2), 'glview-device-named',
          'painterTarget -> %s, QPainter on %s' % ((m.group(1), m.group(2)) if m else ('none', 'none')))
    dev = m.group(1) if m else 'painterDevice'
    ends = [x.start() for x in re.finditer(r'painter\.end\(\);', glv)]
    dones = [x.start() for x in re.finditer(r'WwVkPresent::painterDone\( ' + dev + r' \);', glv)]
    check(len(ends) == 1 and len(dones) == 1 and dones[0] > ends[0] and dones[0] - ends[0] < 200, 'glview-done-after-end',
          'painter.end %d, painterDone(%s) %d, gap %s' % (len(ends), dev, len(dones),
                                                        (dones[0] - ends[0]) if ends and dones else 'n/a'))
    body = re.search(r'\nQPaintDevice \* painterTarget\( QOpenGLWindow \* w \)\n\{(.*?)\n\}', vkp, re.S)
    b = body.group(1) if body else ''
    i_img = b.find('overlayTarget(')
    i_gl = b.find('dev.get()')
    check(i_img >= 0 and i_gl >= 0 and i_img < i_gl, 'painter-target-image-first',
          'overlayTarget at %d, GL device at %d in painterTarget' % (i_img, i_gl))
    check('"vk overlay: drawn %1, gl %2"' in vkp, 'frame-says-vk-overlay', 'the vk overlay: line in frame()')
    print('OVERLAYSITE %d checks, %d failed' % (checks, fails))
    return 0 if fails == 0 and checks > 0 else 1


sys.exit(main())
