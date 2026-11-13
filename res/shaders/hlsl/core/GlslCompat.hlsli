// GLSL built-ins that HLSL spells differently or lacks, for the readable HLSL sources that
// notes/hlslsrc1/tools/glsl2hlsl.py translates from res/shaders (one source per GLSL file).
// Matrices in those sources hold GL's columns as HLSL rows, so every helper here takes and returns that layout.
#ifndef WW_GLSL_COMPAT_HLSLI
#define WW_GLSL_COMPAT_HLSLI

// GLSL mod: x - y * floor(x / y) (HLSL fmod truncates toward zero instead); y may be a scalar
template<typename T, typename U> T mod(T x, U y)
{
    return x - y * floor(x / y);
}

// implicit-lod sampling in a file shared by pixel and other stages: lod 0 outside the pixel stage
#ifdef WW_STAGE_PS
#define WW_SAMPLE(t, s, uv) t.Sample(s, uv)
#define WW_SAMPLE_OFFSET(t, s, uv, o) t.Sample(s, uv, o)
#define WW_SAMPLE_CMP(t, s, uv, r) t.SampleCmp(s, uv, r)
#else
#define WW_SAMPLE(t, s, uv) t.SampleLevel(s, uv, 0)
#define WW_SAMPLE_OFFSET(t, s, uv, o) t.SampleLevel(s, uv, 0, o)
#define WW_SAMPLE_CMP(t, s, uv, r) t.SampleCmpLevelZero(s, uv, r)
#endif

float wwRoundEven(float x) { return round(x); }
float2 wwRoundEven(float2 x) { return round(x); }
float3 wwRoundEven(float3 x) { return round(x); }
float4 wwRoundEven(float4 x) { return round(x); }

// matN(s): s on the diagonal
float2x2 wwMatDiag2(float s) { return float2x2(s, 0, 0, s); }
float3x3 wwMatDiag3(float s) { return float3x3(s, 0, 0, 0, s, 0, 0, 0, s); }
float4x4 wwMatDiag4(float s) { return float4x4(s, 0, 0, 0, 0, s, 0, 0, 0, 0, s, 0, 0, 0, 0, s); }

// matN(smaller matrix): the matrix in the upper left, identity elsewhere
float3x3 wwMatGrow3(float2x2 m) { return float3x3(m[0], 0, m[1], 0, 0, 0, 1); }
float4x4 wwMatGrow4(float3x3 m) { return float4x4(m[0], 0, m[1], 0, m[2], 0, 0, 0, 0, 1); }
float4x4 wwMatGrow4(float2x2 m) { return float4x4(m[0], 0, 0, m[1], 0, 0, 0, 0, 1, 0, 0, 0, 0, 1); }

// outerProduct(c, r): GL column j = c * r[j], i.e. HLSL row j
float2x2 wwOuterProduct(float2 c, float2 r) { return float2x2(c * r.x, c * r.y); }
float3x3 wwOuterProduct(float3 c, float3 r) { return float3x3(c * r.x, c * r.y, c * r.z); }
float4x4 wwOuterProduct(float4 c, float4 r) { return float4x4(c * r.x, c * r.y, c * r.z, c * r.w); }

// inverse: inverse(transpose(M)) == transpose(inverse(M)), so the plain inverse of the HLSL matrix is right
float2x2 wwInverse(float2x2 m)
{
    return float2x2(m[1][1], -m[0][1], -m[1][0], m[0][0]) / determinant(m);
}

float3x3 wwInverse(float3x3 m)
{
    float3 c0 = cross(m[1], m[2]);
    float3 c1 = cross(m[2], m[0]);
    float3 c2 = cross(m[0], m[1]);
    return transpose(float3x3(c0, c1, c2)) / dot(m[0], c0);
}

float4x4 wwInverse(float4x4 m)
{
    float a00 = m[0][0], a01 = m[0][1], a02 = m[0][2], a03 = m[0][3];
    float a10 = m[1][0], a11 = m[1][1], a12 = m[1][2], a13 = m[1][3];
    float a20 = m[2][0], a21 = m[2][1], a22 = m[2][2], a23 = m[2][3];
    float a30 = m[3][0], a31 = m[3][1], a32 = m[3][2], a33 = m[3][3];
    float b00 = a00 * a11 - a01 * a10, b01 = a00 * a12 - a02 * a10, b02 = a00 * a13 - a03 * a10;
    float b03 = a01 * a12 - a02 * a11, b04 = a01 * a13 - a03 * a11, b05 = a02 * a13 - a03 * a12;
    float b06 = a20 * a31 - a21 * a30, b07 = a20 * a32 - a22 * a30, b08 = a20 * a33 - a23 * a30;
    float b09 = a21 * a32 - a22 * a31, b10 = a21 * a33 - a23 * a31, b11 = a22 * a33 - a23 * a32;
    float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
    return float4x4(
        a11 * b11 - a12 * b10 + a13 * b09, a02 * b10 - a01 * b11 - a03 * b09, a31 * b05 - a32 * b04 + a33 * b03, a22 * b04 - a21 * b05 - a23 * b03,
        a12 * b08 - a10 * b11 - a13 * b07, a00 * b11 - a02 * b08 + a03 * b07, a32 * b02 - a30 * b05 - a33 * b01, a20 * b05 - a22 * b02 + a23 * b01,
        a10 * b10 - a11 * b08 + a13 * b06, a01 * b08 - a00 * b10 - a03 * b06, a30 * b04 - a31 * b02 + a33 * b00, a21 * b02 - a20 * b04 - a23 * b00,
        a11 * b07 - a10 * b09 - a12 * b06, a00 * b09 - a01 * b07 + a02 * b06, a31 * b01 - a30 * b03 - a32 * b00, a20 * b03 - a21 * b01 + a22 * b00) / det;
}

// pack / unpack (the forms spirv-cross wrote for the generated ports)
uint wwPackUnorm4x8(float4 value)
{
    uint4 Packed = uint4(round(saturate(value) * 255.0));
    return Packed.x | (Packed.y << 8) | (Packed.z << 16) | (Packed.w << 24);
}

float4 wwUnpackUnorm4x8(uint value)
{
    uint4 Packed = uint4(value & 0xff, (value >> 8) & 0xff, (value >> 16) & 0xff, value >> 24);
    return float4(Packed) / 255.0;
}

uint wwPackSnorm4x8(float4 value)
{
    int4 Packed = int4(round(clamp(value, -1.0, 1.0) * 127.0)) & 0xff;
    return uint(Packed.x | (Packed.y << 8) | (Packed.z << 16) | (Packed.w << 24));
}

float4 wwUnpackSnorm4x8(uint value)
{
    int SignedValue = int(value);
    int4 Packed = int4(SignedValue << 24, SignedValue << 16, SignedValue << 8, SignedValue) >> 24;
    return clamp(float4(Packed) / 127.0, -1.0, 1.0);
}

uint wwPackUnorm2x16(float2 value)
{
    uint2 Packed = uint2(round(saturate(value) * 65535.0));
    return Packed.x | (Packed.y << 16);
}

float2 wwUnpackUnorm2x16(uint value)
{
    uint2 Packed = uint2(value & 0xffff, value >> 16);
    return float2(Packed) / 65535.0;
}

uint wwPackHalf2x16(float2 value)
{
    uint2 Packed = f32tof16(value);
    return Packed.x | (Packed.y << 16);
}

float2 wwUnpackHalf2x16(uint value)
{
    return f16tof32(uint2(value & 0xffff, value >> 16));
}

uint wwBitfieldExtract(uint value, int offset, int bits)
{
    return bits == 0 ? 0u : (value >> uint(offset)) & (bits >= 32 ? 0xffffffffu : ((1u << uint(bits)) - 1u));
}

int wwBitfieldExtract(int value, int offset, int bits)
{
    return bits == 0 ? 0 : (value << (32 - bits - offset)) >> (32 - bits);
}

uint wwBitfieldInsert(uint base, uint insert, int offset, int bits)
{
    uint mask = (bits >= 32 ? 0xffffffffu : ((1u << uint(bits)) - 1u)) << uint(offset);
    return (base & ~mask) | ((insert << uint(offset)) & mask);
}

// textureSize(sampler, lod) and textureQueryLevels
#define WW_TEX_SIZE2(T) \
    int2 wwTextureSize(T t, int lod) { uint w, h, n; t.GetDimensions(uint(lod), w, h, n); return int2(w, h); } \
    int wwTextureLevels(T t) { uint w, h, n; t.GetDimensions(0, w, h, n); return int(n); }
#define WW_TEX_SIZE3(T) \
    int3 wwTextureSize(T t, int lod) { uint w, h, d, n; t.GetDimensions(uint(lod), w, h, d, n); return int3(w, h, d); } \
    int wwTextureLevels(T t) { uint w, h, d, n; t.GetDimensions(0, w, h, d, n); return int(n); }
WW_TEX_SIZE2(Texture2D<float4>)
WW_TEX_SIZE2(Texture2D<uint4>)
WW_TEX_SIZE2(Texture2D<int4>)
WW_TEX_SIZE2(TextureCube<float4>)
WW_TEX_SIZE3(Texture3D<float4>)
WW_TEX_SIZE3(Texture3D<uint4>)
WW_TEX_SIZE3(Texture2DArray<float4>)
WW_TEX_SIZE3(Texture2DArray<uint4>)
WW_TEX_SIZE3(TextureCubeArray<float4>)
#undef WW_TEX_SIZE2
#undef WW_TEX_SIZE3

int2 wwTextureSize(Texture2DMS<float4> t) { uint w, h, s; t.GetDimensions(w, h, s); return int2(w, h); }
int2 wwTextureSize(Texture2DMS<uint4> t) { uint w, h, s; t.GetDimensions(w, h, s); return int2(w, h); }
int wwTextureSize(Buffer<float4> t) { uint n; t.GetDimensions(n); return int(n); }
int wwTextureSize(Buffer<int4> t) { uint n; t.GetDimensions(n); return int(n); }
int wwTextureSize(Buffer<uint4> t) { uint n; t.GetDimensions(n); return int(n); }

#endif
