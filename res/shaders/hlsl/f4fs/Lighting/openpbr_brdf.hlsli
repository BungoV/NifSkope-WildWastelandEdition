// Copyright 2026 Adobe (EON reference portions). Apache-2.0.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.
// OpenPBR Surface 1.1.1 lighting core. One OpenPBR_* function per OpenPBR_*
// function in openpbr_brdf.hlsli, plus F82 metal Fresnel, EON diffuse and the
// table-driven GGX energy compensation.
#ifndef OPENPBR_BRDF_HLSLI_INCLUDED
#define OPENPBR_BRDF_HLSLI_INCLUDED

#include "../Lighting/openpbr_brdf_tables.hlsli"

static const float OPENPBR_PI = 3.14159265358979323846;
static const float OPENPBR_INV_PI = 1.0 / OPENPBR_PI;
static const float OPENPBR_BRDF_EPSILON = 1e-4;
// Fallout's direct-light colors were calibrated for diffuse = albedo * NoL,
// without Lambert's 1/pi. Treat them as irradiance and convert to radiance
// before evaluating a normalized Cook-Torrance BRDF. This keeps diffuse at the
// engine's established exposure while putting specular in the same units.
static const float OPENPBR_ENGINE_LIGHT_TO_RADIANCE = OPENPBR_PI;
// OpenPBR base_diffuse_roughness default; the direct diffuse lobe uses it (no per-pixel channel carries an authored value).
static const float OPENPBR_BASE_DIFFUSE_ROUGHNESS = 0.0;
// Fallout world units are approximately 1.43 cm. A four-unit local emitter is
// a conservative ~5.7 cm bulb/filament envelope; the angular-radius cap avoids
// singular near-field highlights when geometry intersects the light origin.
static const float OPENPBR_LOCAL_EMITTER_RADIUS_WORLD = 4.0;
static const float OPENPBR_MAX_EMITTER_ANGULAR_RADIUS = 0.25;
static const float OPENPBR_SUN_ANGULAR_RADIUS = 0.004642576; // 0.266 degrees

float OpenPBR_Pow5(float value)
{
    float value2 = value * value;
    return value2 * value2 * value;
}

float OpenPBR_D_GGX(float NdotH, float roughness)
{
    float alpha = saturate(roughness) * saturate(roughness);
    float alpha2 = alpha * alpha;
    // Avoid cancellation at the mirror direction. The old 1e-4 denominator
    // floor retained only 0.168% of the peak at the runtime roughness floor.
    float NoH2 = saturate(NdotH) * saturate(NdotH);
    precise float denominator = (1.0 - NoH2) + NoH2 * alpha2;
    denominator = max(denominator, 1e-12);
    return alpha2 * OPENPBR_INV_PI / (denominator * denominator);
}

// The original denominator floor, kept where no finite-emitter roughness filtering
// runs first: BGSM22 (the reference renderer's arithmetic) and skin, whose sun and
// local lights would otherwise peak about 600x higher at full gloss.
float OpenPBR_D_GGXCapped(float NdotH, float roughness)
{
    float alpha = saturate(roughness) * saturate(roughness);
    float alpha2 = alpha * alpha;
    float denominator = max(NdotH * NdotH * (alpha2 - 1.0) + 1.0, OPENPBR_BRDF_EPSILON);
    return alpha2 * OPENPBR_INV_PI / (denominator * denominator);
}

// Height-correlated Smith GGX visibility, i.e. G2 / (4 NoL NoV).
float OpenPBR_Vis(float NdotL, float NdotV, float roughness)
{
    float NoL = saturate(NdotL);
    float NoV = saturate(NdotV);
    float alpha = saturate(roughness) * saturate(roughness);
    float alpha2 = alpha * alpha;
    float lambdaV = NoL * sqrt(NoV * NoV * (1.0 - alpha2) + alpha2);
    float lambdaL = NoV * sqrt(NoL * NoL * (1.0 - alpha2) + alpha2);
    return 0.5 / max(lambdaV + lambdaL, OPENPBR_BRDF_EPSILON);
}

float3 OpenPBR_F_Schlick(float3 F0, float VdotH)
{
    float vhComplement = 1.0 - saturate(VdotH);
    float f = OpenPBR_Pow5(vhComplement);
    return F0 + (1.0 - F0) * f;
}

// OpenPBR Surface v1.1.1, Metal / F82-tint equations, ASWF revision
// f8d6d947dfae4c9b599965a86c22826ea7a8dbfb. edgeTint is specular_color at
// cos(theta)=1/7, NOT F90. Apply specular_weight to the entire lobe after
// evaluating this function.
float3 OpenPBR_F82Metal(float3 F0, float3 edgeTint, float VdotH)
{
    float mu = saturate(VdotH);
    float complement = 1.0 - mu;
    const float referenceMu = 1.0 / 7.0;
    const float referenceComplement = 6.0 / 7.0;
    float correction = mu * OpenPBR_Pow5(complement) * complement /
        (referenceMu * OpenPBR_Pow5(referenceComplement) * referenceComplement);
    return max(0.0.xxx, OpenPBR_F_Schlick(F0, mu) - correction *
        OpenPBR_F_Schlick(F0, referenceMu) * (1.0 - saturate(edgeTint)));
}

// Disney/Burley diffuse factor relative to Lambert. This is evaluated only
// for analytic lights. Fallout's ambient cube and irradiance paths do not
// carry the NoL-dependent preintegration required for Burley indirect diffuse.
float OpenPBR_Fd_Burley(float NdotV, float NdotL, float LdotH, float roughness)
{
    float NoV = saturate(NdotV);
    float NoL = saturate(NdotL);
    float LoH = saturate(LdotH);
    float perceptualRoughness = saturate(roughness);
    float f90 = 0.5 + 2.0 * perceptualRoughness * LoH * LoH;
    float lightScatter = 1.0 + (f90 - 1.0) * OpenPBR_Pow5(1.0 - NoL);
    float viewScatter = 1.0 + (f90 - 1.0) * OpenPBR_Pow5(1.0 - NoV);
    return lightScatter * viewScatter;
}

float OpenPBR_FonAlbedo(float cosine, float roughness)
{
    float mu = saturate(cosine);
    float r = saturate(roughness);
    float s = sqrt(max(0.0, 1.0 - mu * mu));
    float g = s * (acos(mu) - s * mu) + (2.0 / 3.0) * (s * mu * (1.0 + s + s * s) / (1.0 + s) - s);
    return (1.0 + r * OPENPBR_INV_PI * g) / (1.0 + (0.5 - 2.0 / (3.0 * OPENPBR_PI)) * r);
}

// Energy-preserving Oren-Nayar (Portsmouth, Kutz, Hill 2025), with Adobe's
// single-pi f_ms. Takes cos(view, normal), cos(light, normal) and cos(view, light).
float3 OpenPBR_Eon(float3 color, float roughness, float NdotV, float NdotL, float VdotL)
{
    float cv = max(saturate(NdotV), OPENPBR_BRDF_EPSILON);
    float cl = max(saturate(NdotL), OPENPBR_BRDF_EPSILON);
    float3 rho = saturate(color);
    float r = saturate(roughness);
    float s = VdotL - cv * cl;
    float st = s > 0.0 ? s / max(cv, cl) : s;
    float af = 1.0 / (1.0 + (0.5 - 2.0 / (3.0 * OPENPBR_PI)) * r);
    float3 ss = rho * OPENPBR_INV_PI * af * (1.0 + r * st);
    float ev = OpenPBR_FonAlbedo(cv, r);
    float el = OpenPBR_FonAlbedo(cl, r);
    float ea = af * (1.0 + (2.0 / 3.0 - 28.0 / (15.0 * OPENPBR_PI)) * r);
    float3 rms = rho * rho * ea / (1.0 - rho * (1.0 - ea));
    return ss + rms * (OPENPBR_INV_PI * (1.0 - ev) * (1.0 - el) / max(1e-7, 1.0 - ea));
}

// Energy left for the diffuse lobe after the view/light-dependent Fresnel
// reflection has been accounted for, using the EON diffuse lobe. The base-color
// path already removes the metallic share. Fallout's light-to-radiance conversion
// cancels the 1/pi normalization.
float3 OpenPBR_DirectDiffuseWeight(
    float3 F0,
    float NdotV,
    float NdotL,
    float LdotH,
    float diffuseRoughness)
{
    // For a half-vector pair, dot(V, L) = 2 (L.H)^2 - 1.
    float VdotL = clamp(2.0 * saturate(LdotH) * saturate(LdotH) - 1.0, -1.0, 1.0);
    float3 diffuseBrdf = saturate(1.0 - OpenPBR_F_Schlick(F0, LdotH))
                       * OpenPBR_Eon(1.0.xxx, diffuseRoughness, NdotV, NdotL, VdotL);
    return diffuseBrdf * OPENPBR_ENGINE_LIGHT_TO_RADIANCE;
}

// Lazarov split-sum environment BRDF scale/bias. Apply the returned pair as
// F0 * x + y to form the indirect specular lobe weight.
float2 OpenPBR_EnvBRDF(float roughness, float NdotV)
{
    float4 r = roughness * float4(-1.0, -0.0275, -0.572, 0.022)
             + float4(1.0, 0.0425, 1.04, -0.04);
    float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    return float2(-1.04, 1.04) * a004 + r.zw;
}

// Interface to the GGX white-furnace energy table. Bilinear in perceptual
// roughness (rows) and cos(theta) (columns); the 32x32 grid is generated by
// tests/OpenPbrSurfaceGenerate.py.
float OpenPBR_GgxEnergy(float roughness, float NdotV)
{
    float x = saturate(NdotV) * 31.0;
    float y = saturate(roughness) * 31.0;
    int ix = min(int(x), 30);
    int iy = min(int(y), 30);
    float fx = x - float(ix);
    float fy = y - float(iy);
    int i00 = iy * 32 + ix;
    return lerp(lerp(OpenPbrGgxEnergyTable[i00].x, OpenPbrGgxEnergyTable[i00 + 1].x, fx),
                lerp(OpenPbrGgxEnergyTable[i00 + 32].x, OpenPbrGgxEnergyTable[i00 + 33].x, fx), fy);
}

// The table's other two columns, same bilinear fetch: the GGX-weighted means of (1 - v.h)^5 and
// v.h (1 - v.h)^6, i.e. the Schlick and F82 correction terms integrated over the lobe.
float2 OpenPBR_GgxFresnelMoments(float roughness, float NdotV)
{
    float x = saturate(NdotV) * 31.0;
    float y = saturate(roughness) * 31.0;
    int ix = min(int(x), 30);
    int iy = min(int(y), 30);
    float fx = x - float(ix);
    float fy = y - float(iy);
    int i00 = iy * 32 + ix;
    return lerp(lerp(OpenPbrGgxEnergyTable[i00].yz, OpenPbrGgxEnergyTable[i00 + 1].yz, fx),
                lerp(OpenPbrGgxEnergyTable[i00 + 32].yz, OpenPbrGgxEnergyTable[i00 + 33].yz, fx), fy);
}

// What the F82 edge tint removes from the split-sum environment response F0 * A + B: the lobe integral of
// OpenPBR_F82Metal's correction term. Exact against the table's moments; zero for a white tint.
float3 OpenPBR_EnvF82Correction(float3 F0, float3 edgeTint, float roughness, float NdotV)
{
    [branch] if (all(edgeTint >= 1.0.xxx))
        return 0.0.xxx;
    const float referenceMu = 1.0 / 7.0;
    const float referenceComplement = 6.0 / 7.0;
    float denominator = referenceMu * OpenPBR_Pow5(referenceComplement) * referenceComplement;
    float m6 = OpenPBR_GgxFresnelMoments(roughness, NdotV).y;
    return (m6 / denominator) * OpenPBR_F_Schlick(F0, referenceMu) * (1.0 - saturate(edgeTint));
}

// Turquin/Filament-style multiple-scattering energy compensation driven by the
// tabulated single-scatter albedo E. For F0=1 the single-scatter response is E;
// multiplying by 1/E restores the white-furnace result. The floor bounds the
// amplification where E approaches zero at grazing angles.
float3 OpenPBR_MultiscatterEnergyCompensation(float3 F0, float roughness, float NdotV)
{
    float directionalAlbedo = max(OpenPBR_GgxEnergy(roughness, NdotV), 0.05);
    return 1.0 + saturate(F0) * (rcp(directionalAlbedo) - 1.0);
}

// The pre-OpenPBR fit from the split-sum pair: BGSM v22+ ambient specular, which the reference renderer's maps were
// authored against, uses it in place of the tabulated compensation.
float3 OpenPBR_MultiscatterEnergyCompensationLegacy(float3 F0, float2 dfg)
{
    float directionalAlbedo = max(saturate(dfg.x + dfg.y), 0.05);
    return 1.0 + saturate(F0) * (rcp(directionalAlbedo) - 1.0);
}

// Jimenez et al. multi-bounce AO approximation used by Skyrim Community
// Shaders. Material AO is an indirect-light property; direct lights never use
// this factor.
float3 OpenPBR_MultiBounceAO(float3 baseColor, float ao)
{
    float3 a = 2.0404 * baseColor - 0.3324;
    float3 b = -4.7951 * baseColor + 0.6417;
    float3 c = 2.7552 * baseColor + 0.6903;
    return max(ao.xxx, ((ao * a + b) * ao + c) * ao);
}

// Roughness/view-aware material specular occlusion. This prevents probe
// highlights from remaining fully exposed in occluded creases without making
// direct specular depend on AO.
float OpenPBR_SpecularOcclusion(float NdotV, float roughness, float ao)
{
    // Lagarde-style specular AO. The exponent approaches zero as roughness
    // rises, so broad rough reflections inherit the authored occlusion while
    // smooth reflections retain their directional visibility.
    float exponent = exp2(-16.0 * saturate(roughness) - 1.0);
    return saturate(pow(saturate(NdotV) + saturate(ao), exponent)
                  - 1.0 + saturate(ao));
}

float3 OpenPBR_DirectSpecular(float3 F0, float roughness, float NdotL, float NdotV, float NdotH, float VdotH)
{
    float3 energyCompensation =
        OpenPBR_MultiscatterEnergyCompensation(F0, roughness, NdotV);
    return OpenPBR_D_GGX(NdotH, roughness)
         * OpenPBR_Vis(NdotL, NdotV, roughness)
         * OpenPBR_F_Schlick(F0, VdotH)
         * energyCompensation
         * saturate(NdotL)
         * OPENPBR_ENGINE_LIGHT_TO_RADIANCE;
}

// the reference renderer's direct-light model, for pixels the prepass marked with
// F4FS_BGSM22_PBR_CODE (Fusion's PBR port notes, section 6). GGX D keeps
// its native floor. Fresnel is the spherical-gaussian Schlick fit with the reference renderer's -1
// constant where UE4 has its own, visibility is Smith/Schlick-GGX with
// k = alpha / 2, diffuse is plain Lambert with no Fresnel or Burley weight,
// and BGSM v22+ has no multiscatter compensation.
float3 OpenPBR_F_SphericalGaussian(float3 F0, float VdotH)
{
    float v = saturate(VdotH);
    return F0 + (1.0 - F0) * exp2((-5.55473 * v - 1.0) * v);
}

float OpenPBR_Vis_SchlickGGX(float NdotL, float NdotV, float roughness)
{
    float NoL = saturate(NdotL);
    float NoV = saturate(NdotV);
    float alpha = saturate(roughness) * saturate(roughness);
    float k = alpha * 0.5;
    float gL = NoL * (1.0 - k) + k;
    float gV = NoV * (1.0 - k) + k;
    return 0.25 / max(gL * gV, OPENPBR_BRDF_EPSILON);
}

// No engine-light-to-radiance factor here: BGSM v22+ scales D * Vis * F by NdotL and
// the light colour exactly as it scales its Lambert diffuse (DFLight 0088), so
// the pi the PBRM BRDF applies would make BGSM v22+ specular 3.14x too strong.
float3 OpenPBR_DirectSpecularBgsm22(float3 F0, float roughness, float NdotL, float NdotV, float NdotH, float VdotH)
{
    return OpenPBR_D_GGXCapped(NdotH, roughness)
         * OpenPBR_Vis_SchlickGGX(NdotL, NdotV, roughness)
         * OpenPBR_F_SphericalGaussian(F0, VdotH)
         * saturate(NdotL);
}

// OPENPBR_BGSM22_BRANCH = 1 makes the two dispatchers below run only the
// selected BRDF behind a real branch. Each arm's arithmetic is unchanged, so
// the result is the same float; the ternary at 0 evaluates both arms and
// selects. The deferred sun, point and spot passes define it to 1 before their
// first include; every other includer keeps the ternary and its bytecode.
#ifndef OPENPBR_BGSM22_BRANCH
#define OPENPBR_BGSM22_BRANCH 0
#endif

float3 OpenPBR_DirectSpecularFor(bool bgsm22,
    float3 F0, float roughness, float NdotL, float NdotV, float NdotH, float VdotH)
{
#if OPENPBR_BGSM22_BRANCH
    float3 r;
    [branch]
    if (bgsm22)
    {
        r = OpenPBR_DirectSpecularBgsm22(F0, roughness, NdotL, NdotV, NdotH, VdotH);
    }
    else
    {
        r = OpenPBR_DirectSpecular(F0, roughness, NdotL, NdotV, NdotH, VdotH);
    }
    return r;
#else
    return bgsm22
        ? OpenPBR_DirectSpecularBgsm22(F0, roughness, NdotL, NdotV, NdotH, VdotH)
        : OpenPBR_DirectSpecular(F0, roughness, NdotL, NdotV, NdotH, VdotH);
#endif
}

// Lambert's 1/pi cancels against the engine's irradiance-calibrated lights. The EON lobe is driven by
// base_diffuse_roughness, never by the specular roughness: the light passes pass the pixel's layer-table value
// (OpenPBR_LayersDiffuseRoughness), which is the spec default 0 (exactly Lambert) for a pixel without a row.
float3 OpenPBR_DirectDiffuseWeightFor(bool bgsm22,
    float3 F0, float NdotV, float NdotL, float LdotH, float diffuseRoughness)
{
#if OPENPBR_BGSM22_BRANCH
    float3 r;
    [branch]
    if (bgsm22)
    {
        r = float3(1.0, 1.0, 1.0);
    }
    else
    {
        r = OpenPBR_DirectDiffuseWeight(F0, NdotV, NdotL, LdotH, diffuseRoughness);
    }
    return r;
#else
    return bgsm22
        ? float3(1.0, 1.0, 1.0)
        : OpenPBR_DirectDiffuseWeight(F0, NdotV, NdotL, LdotH, diffuseRoughness);
#endif
}

float OpenPBR_FilterPerceptualRoughnessForEmitter(
    float perceptualRoughness, float angularRadiusRadians)
{
    // Convolve GGX alpha (perceptualRoughness^2) with the apparent emitter
    // radius. Only the BRDF-consumed direct-specular roughness is changed: the
    // authored texture, diffuse Burley response, IBL and SSR remain untouched.
    float roughness = saturate(perceptualRoughness);
    float alpha = roughness * roughness;
    float angularRadius = clamp(angularRadiusRadians, 0.0,
        OPENPBR_MAX_EMITTER_ANGULAR_RADIUS);
    float filteredAlpha = sqrt(alpha * alpha + angularRadius * angularRadius);
    return clamp(sqrt(filteredAlpha), roughness, 1.0);
}

float OpenPBR_LocalEmitterAngularRadius(float receiverToLightDistance)
{
    return atan(OPENPBR_LOCAL_EMITTER_RADIUS_WORLD /
        max(receiverToLightDistance, OPENPBR_LOCAL_EMITTER_RADIUS_WORLD));
}

// ---------------------------------------------------------------------------
// Coat and fuzz layers (OpenPBR Surface 1.1.1). The coat equations follow the
// PR #111 helpers (OpenPbrCoatAbsorption, OpenPbrCoatDarkening,
// OpenPbrCoatRoughening); the fuzz lobe is its LTC fit (OpenPbrFuzzBrdf).
// Coat IOR below 1 is clamped to 1. The hemispherical average Fresnel uses the
// Egan-Hilgeman fit instead of a per-pixel integral. Both layers attenuate what
// lies below them: f = f_layer + (1 - E_layer) * f_below.
// ---------------------------------------------------------------------------

float OpenPBR_CoatIor(float ior)
{
    return max(ior, 1.0);
}

float OpenPBR_CoatF0(float ior)
{
    float eta = OpenPBR_CoatIor(ior);
    float r = (eta - 1.0) / (eta + 1.0);
    return r * r;
}

float OpenPBR_RefractionCosine(float mu, float eta)
{
    mu = saturate(abs(mu));
    float inverseEta = 1.0 / eta;
    float normal = mu * inverseEta;
    return sqrt(max(0.0, (1.0 - inverseEta) * (1.0 + inverseEta) + normal * normal));
}

float OpenPBR_DielectricFresnel(float cosine, float eta)
{
    float mu = saturate(abs(cosine));
    if (eta <= 1.0)
        return 0.0;
    float ct = OpenPBR_RefractionCosine(mu, eta);
    float rs = (mu / eta - ct) / max(mu / eta + ct, 1e-7);
    float rp = (mu - ct / eta) / max(mu + ct / eta, 1e-7);
    return saturate(0.5 * (rs * rs + rp * rp));
}

// Directional albedo of the coat's specular lobe, split-sum.
float OpenPBR_CoatAlbedo(float ior, float roughness, float NdotV)
{
    float2 envBrdf = OpenPBR_EnvBRDF(roughness, saturate(NdotV));
    return saturate(OpenPBR_CoatF0(ior) * envBrdf.x + envBrdf.y);
}

float3 OpenPBR_CoatAbsorption(float3 color, float ior, float NdotV, float NdotL)
{
    float eta = OpenPBR_CoatIor(ior);
    float tv = OpenPBR_RefractionCosine(NdotV, eta);
    float tl = OpenPBR_RefractionCosine(NdotL, eta);
    if (tv <= 0.0 || tl <= 0.0)
        return 0.0.xxx;
    // sqrt(color) per normal passage = color^(0.5 * (pathV + pathL)).
    return pow(saturate(color), 0.5 * (1.0 / tv + 1.0 / tl));
}

float OpenPBR_CoatAverageFresnel(float eta)
{
    float invEta = 1.0 / eta;
    return saturate(-1.4399 * invEta * invEta + 0.7099 * invEta + 0.6681 + 0.0636 * eta);
}

// Released 1.1.1 form, no coat_color factor in the denominator.
float3 OpenPBR_CoatDarkening(float3 baseAlbedo, float ior, float baseRoughness, float NdotV)
{
    float eta = OpenPBR_CoatIor(ior);
    float ks = OpenPBR_DielectricFresnel(NdotV, eta);
    float kr = saturate(1.0 - ((1.0 - OpenPBR_CoatAverageFresnel(eta)) / eta) / eta);
    float k = lerp(ks, kr, saturate(baseRoughness));
    float3 rho = saturate(baseAlbedo);
    float3 d = (1.0 - rho) + rho * (1.0 - k);
    return float3(d.x > 0.0 ? (1.0 - k) / d.x : 1.0,
                  d.y > 0.0 ? (1.0 - k) / d.y : 1.0,
                  d.z > 0.0 ? (1.0 - k) / d.z : 1.0);
}

// An index-matched coat does not roughen the base.
float OpenPBR_CoatRoughening(float baseRoughness, float coatRoughness, float weight, float ior)
{
    float rb = saturate(baseRoughness);
    float rc = saturate(coatRoughness);
    float eta = OpenPBR_CoatIor(ior);
    float contrast = 1.0 - min(eta, 1.0 / eta);
    float r4 = rb * rb * rb * rb + 2.0 * contrast * rc * rc * rc * rc;
    return lerp(rb, sqrt(sqrt(min(1.0, r4))), saturate(weight));
}

struct OpenPBR_CoatResult
{
    float3 specular;
    float3 specularTransmission;
    float3 diffuseTransmission;
};

// Coat reflection (already times N.L and the engine light scale, like OpenPBR_DirectSpecular)
// and what it lets through to the base layer's specular and diffuse.
OpenPBR_CoatResult OpenPBR_Coat(
    float3 color, float weight, float coatRoughness, float ior, float darkening,
    float3 baseAlbedo, float baseRoughness,
    float NdotL, float NdotV, float NdotH, float VdotH)
{
    OpenPBR_CoatResult result;
    result.specular = 0.0.xxx;
    result.specularTransmission = 1.0.xxx;
    result.diffuseTransmission = 1.0.xxx;
    float w = saturate(weight);
    if (w <= 0.0)
        return result;
    float eta = OpenPBR_CoatIor(ior);
    float fresnel = OpenPBR_DielectricFresnel(VdotH, eta);
    result.specular = w * fresnel
        * OpenPBR_D_GGX(NdotH, coatRoughness)
        * OpenPBR_Vis(NdotL, NdotV, coatRoughness)
        * saturate(NdotL) * OPENPBR_ENGINE_LIGHT_TO_RADIANCE;
    float albedo = 0.5 * (OpenPBR_CoatAlbedo(ior, coatRoughness, NdotV)
                        + OpenPBR_CoatAlbedo(ior, coatRoughness, NdotL));
    float3 covered = (1.0 - albedo) * OpenPBR_CoatAbsorption(color, ior, NdotV, NdotL);
    float3 dark = OpenPBR_CoatDarkening(baseAlbedo, ior, baseRoughness, NdotV);
    result.specularTransmission = lerp(1.0.xxx, covered, w);
    result.diffuseTransmission = lerp(1.0.xxx, covered * lerp(1.0.xxx, dark, saturate(darkening)), w);
    return result;
}

// Bilinear in roughness (rows) and cos(theta) (columns): the LTC fit (x, y)
// and the directional albedo (z).
float3 OpenPBR_FuzzCoefficients(float roughness, float cosine)
{
    float x = saturate(cosine) * 31.0;
    float y = saturate(roughness) * 31.0;
    int ix = min(int(x), 30);
    int iy = min(int(y), 30);
    float fx = x - float(ix);
    float fy = y - float(iy);
    int i00 = iy * 32 + ix;
    return lerp(lerp(OpenPbrFuzzTable[i00], OpenPbrFuzzTable[i00 + 1], fx),
                lerp(OpenPbrFuzzTable[i00 + 32], OpenPbrFuzzTable[i00 + 33], fx), fy);
}

// v and l in the shading frame (z = normal). Returns f * cos(light); the caller applies the light scale.
float3 OpenPBR_FuzzBrdf(float3 color, float weight, float roughness, float3 v, float3 l)
{
    if (v.z <= 0.0 || l.z <= 0.0)
        return 0.0.xxx;
    float3 c = OpenPBR_FuzzCoefficients(roughness, v.z);
    if (c.x <= 0.0 || c.z <= 0.0)
        return 0.0.xxx;
    float planar = v.x * v.x + v.y * v.y;
    float vx = planar > 1e-12 ? v.x * rsqrt(planar) : 1.0;
    float vy = planar > 1e-12 ? v.y * rsqrt(planar) : 0.0;
    float x = vx * l.x + vy * l.y;
    float y = -vy * l.x + vx * l.y;
    float3 q = float3(c.x * x + c.y * l.z, c.x * y, l.z);
    float d = dot(q, q);
    // The LTC pdf is cos(theta_i) * BRDF; cancel it analytically before dividing.
    return saturate(color) * (saturate(weight) * c.z * OPENPBR_INV_PI * c.x * c.x / max(d * d, 1e-30)) * l.z;
}

struct OpenPBR_FuzzResult
{
    float3 specular;
    float transmission;
};

OpenPBR_FuzzResult OpenPBR_Fuzz(
    float3 color, float weight, float roughness, float3 N, float3 V, float3 L)
{
    OpenPBR_FuzzResult result;
    result.specular = 0.0.xxx;
    result.transmission = 1.0;
    float w = saturate(weight);
    float NdotV = dot(N, V);
    if (w <= 0.0 || NdotV <= 0.0)
        return result;
    float3 t = V - N * NdotV;
    float tt = dot(t, t);
    if (tt > 1e-12)
        t *= rsqrt(tt);
    else
        t = normalize(cross(abs(N.z) < 0.99 ? float3(0, 0, 1) : float3(1, 0, 0), N));
    float3 b = cross(N, t);
    float3 v = float3(dot(V, t), dot(V, b), NdotV);
    float3 l = float3(dot(L, t), dot(L, b), dot(L, N));
    result.specular = OpenPBR_FuzzBrdf(color, w, roughness, v, l) * OPENPBR_ENGINE_LIGHT_TO_RADIANCE;
    // Untinted, including black fuzz.
    result.transmission = 1.0 - saturate(w * OpenPBR_FuzzCoefficients(roughness, NdotV).z);
    return result;
}

#endif
