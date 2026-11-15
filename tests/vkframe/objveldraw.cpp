/* tests/vkframe/objveldraw.cpp -- lane VKOBJ10 (2026-11-16): the gate for the TAA inputs' moving-object redraw on Vulkan
 * (taainputs.cpp wwTaaInputsPass step 3, taa_objvel.prog; twins objvel_vs / objvel_ps from res/shaders/hlsl/post).
 *
 * BAR, committed before this gate's first run (it is never widened; a seeding change that lets a red reach its branch is
 * allowed, with the reason written here):
 *  - GL is the reference: GL's own taa_objvel.prog draws the fixture's movers into an RG16F vel + RGBA8 mask frame over a
 *    blitted depth, in the pass's state (LEQUAL, depth mask off, glPolygonOffset( -1, -4 ), blend off, cull off, att1
 *    masked to B; coverage pass 1: att0 off, att1 R only). Vulkan draws the same movers through the route into shared twins.
 *  - Depth: the depth attachment is read-only in this pass (GL never writes it): Vulkan's depth after the pass must equal
 *    the blitted input EXACTLY (0 differing texels).
 *  - Mask (RGBA8, values 0 / 255 only): EXACT, 0 differing texels.
 *  - Coverage: the set of texels each side wrote (vel or mask non-zero) EXACT.
 *  - vel (RG16F): max abs difference <= 2 half-steps of the GL value (2^-10 relative, the RG16F rounding of the same
 *    float); a texel past that FAILS. GL vel non-zero on >= 20 % of covered texels or the case is TESTS NOTHING.
 *  - The fixture: rigid movers with clipPrev != clipCur, a skinned mover (client arrays, attr 8 prev != cur), an
 *    alpha-tested mover, a blended mover (coverage passes 0 and 1), one mover coincident with the frame depth so the
 *    depth bias decides it (GL probe: polygon offset off moves its coverage on >= 20 % of its texels, printed).
 *  - Reds, each must FAIL: 1 prev = cur on the skinned mover; 2 pass 1 writes vel; 3 no depth bias; 4 a b31 offset
 *    shift (one row). A refused input (fxMode 2 / 8 / 16, velMode 2) must refuse by name and leave GL's frame untouched.
 *  - Every mover covers >= 64 texels on GL (printed per mover), or the case is TESTS NOTHING.
 */
