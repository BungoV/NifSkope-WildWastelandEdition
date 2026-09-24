
/*! Harness pins and census of lane PBRR0 (docs/NIFSKOPE_PBR_RENDERER.md s9).
 *  WW_PBRM_MODE / WW_PBRM_AUTOREPLACE override the cached menu values above and
 *  still pass through the feature gate; WW_RENDER_PARTICLES is read by the shot
 *  hook (unset = the old forced-on). WW_PBRM_CENSUS=<absolute path> writes one
 *  line per drawn shape: the route that SERVED it, the file, the .pbrm envelope
 *  and the refusal reason, with every pin echoed in the header. */
class BSShaderLightingProperty;
bool wwRenderParticlesPin();
bool wwPbrmCensusArmed();
void wwPbrmCensus( const QString & shapeName, const char * kind, const BSShaderLightingProperty * sp,
	const QString & servedProgram );
