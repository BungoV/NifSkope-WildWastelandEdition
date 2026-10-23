#ifndef CELLLENSFLARE_H
#define CELLLENSFLARE_H

/* lane LIGHTS3 (LENSFLARE1): the lens flares of placed lights whose LIGH names a LENS (LNAM), drawn as screen
 * sprites after the tone map (post-process only: nothing is drawn in the world). Look row "Lens flares"
 * (WW/CellLensFlares, pin WW_CELL_LENSFLARE), Off by default, live.
 *
 * Sourced from the exe (private notes, notes/lights3): per light, alpha = 1 - clamp(max(d - radius x DNAM, 0) / 256)
 * (d the camera distance); a spot also fades off its cone; the light must be in front (dot with the view axis > 0).
 * A sprite's colour = tint x opacity x pow(that dot, 255^angularFade - 1) x alpha x (white + (light colour - white)
 * x CNAM); its size (width, height) with the screen's aspect; Rotates turns it to the light's screen angle.
 * At most 16 flare lights (the nearest; the game's pick order is unknown).
 * Inferred, not read: the sprite centre at the light's screen point x (1 - 2 x Position), the quad's corners at
 * +-1, the occlusion test (a 5 x 5 depth kernel round the light; the game averages its own test over 4 frames),
 * the colour added to the tone-mapped frame. Red WW_CELL_LENSFLARE_RED=noocclude (every flare unoccluded). */

#include <QString>
#include <QtGlobal>

class ESMFile;
class Scene;

//! read a LENS into the pass's table once; false when `form` is not a LENS or has no sprites
bool wwCellLensKnown( quint32 form, ESMFile & esm );
//! inside the linear frame, before its tone map: keep the opaque depth for the occlusion test
void wwCellLensFlareGrab( Scene * scene );
//! after the tone map: add the flares; returns the sprites drawn
int wwCellLensFlareDraw( Scene * scene );
//! a one-line census for the cell report (row state, lights with a lens, drawn last frame)
QString wwCellLensFlareEcho( Scene * scene );

#endif // CELLLENSFLARE_H
