// Derived from renodx (https://github.com/clshortfuse/renodx) as vendored by Bottled Shaders
// (https://github.com/InTheBottle/Bottled-Shaders),
// features/Post Processing/Shaders/PostProcessing/ColorGrading/Include/renodx/tonemap/neutwo.hlsl
// at commit 700607fb347c761f3b76088bd494c8e930a9c95f, and the NeutwoTonemap caller in
// ColorGrading/colorgrading.hlsl there, licensed MIT. See THIRD_PARTY.md.
//
// Copyright (C) 2026 Carlos Lopez
// SPDX-License-Identifier: MIT
//
// MIT License
//
// Copyright (c) 2025 Carlos Lopez Jr.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// THE FULL NEUTWO CURVE (2026-10-19). F4FS_HdrPeakRolloff in TonemapCS.hlsl is
// Neutwo's peak-only form on the largest channel, used as an HDR rolloff for the
// legacy curves. This is the tonemapper itself as Bottled exposes it: the
// peak-and-clip form on BT.709 luminance, so the scene value `clip` lands exactly
// on `peak` and mid-tones pass almost untouched. Host mirror:
// f4fs::post::TonemapNeutwo (src/Post/Tonemap.cpp).
#ifndef F4FS_POST_TONEMAP_RENODX_NEUTWO_HLSLI
#define F4FS_POST_TONEMAP_RENODX_NEUTWO_HLSLI

// f_c(x) = c p x / sqrt(x x (c c - p p) + c c p p)
float F4FS_NeutwoPeakClip(float x, float peak, float clip)
{
	const float p = peak;
	const float c = clip;
	const float cc = c * c;
	const float pp = p * p;
	const float xx = x * x;
	const float numerator = c * p * x;
	const float denominatorSquared = mad(xx, (cc - pp), cc * pp);
	return numerator * rsqrt(denominatorSquared);
}

// renodx::tonemap::neutwo::BT709(color, peak, clip): one scale from the BT.709
// luminance (renodx::color::y::from::BT709), applied to all three channels.
float3 F4FS_NeutwoBT709(float3 color, float peak, float clip)
{
	const float y = dot(color, float3(0.2126390059f, 0.7151686788f, 0.0721923154f));
	const float newY = F4FS_NeutwoPeakClip(y, peak, clip);
	const float scale = y != 0.0f ? (newY / y) : 1.0f;
	return color * scale;
}

#endif
