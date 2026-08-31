/***** BEGIN LICENSE BLOCK *****

BSD License

Copyright (c) 2005-2015, NIF File Format Library and Tools
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
3. The name of the NIF File Format Library and Tools project may not be
used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

***** END LICENCE BLOCK *****/

#ifndef MATERIAL_H
#define MATERIAL_H

#include "data/niftypes.h"

#include <QObject>
#include <QByteArray>
#include <QDataStream>
#include <QString>


//! @file material.h Material, ShaderMaterial, EffectMaterial

class Material : public QObject
{
	Q_OBJECT

	friend class Renderer;
	friend class NifModel;

public:
	Material();

	bool isValid() const { return readable; }
	bool hasAlphaBlend() const { return (bAlphaBlend != 0); }
	bool hasAlphaTest() const { return (bAlphaTest != 0); }
	bool hasDecal() const { return (bDecal != 0); }
	const QStringList & textures() const { return textureList; }
	bool isEffectMaterial() const { return isBGEM; }
	bool isShaderMaterial() const { return isBGSM; }
	quint32 fileVersion() const { return version; }
	Vector2 uvOffset() const { return Vector2( fUOffset, fVOffset ); }
	Vector2 uvScale() const { return Vector2( fUScale, fVScale ); }
	float alpha() const { return fAlpha; }
	quint32 alphaSourceBlend() const { return iAlphaSrc; }
	quint32 alphaDestinationBlend() const { return iAlphaDst; }
	quint8 alphaTestThreshold() const { return iAlphaTestRef; }
	float refractionPower() const { return fRefractionPower; }
	float environmentMapScale() const { return fEnvironmentMappingMaskScale; }
	Color3 emittanceColor() const { return cEmittanceColor; }
	quint32 commonShaderFlags1() const {
		return ( tileFlags & 3U ) | ( bAlphaBlend ? 0x0004U : 0U ) | ( bAlphaTest ? 0x0008U : 0U )
			| ( bZBufferWrite ? 0x0010U : 0U ) | ( bZBufferTest ? 0x0020U : 0U )
			| ( bScreenSpaceReflections ? 0x0040U : 0U ) | ( bWetnessControl_ScreenSpaceReflections ? 0x0080U : 0U )
			| ( bDecal ? 0x0100U : 0U ) | ( bTwoSided ? 0x0200U : 0U ) | ( bDecalNoFade ? 0x0400U : 0U )
			| ( bNonOccluder ? 0x0800U : 0U ) | ( bRefraction ? 0x1000U : 0U ) | ( bRefractionFalloff ? 0x2000U : 0U )
			| ( bEnvironmentMapping ? 0x4000U : 0U ) | ( bGrayscaleToPaletteColor ? 0x8000U : 0U );
	}
	static void createMaterialData( QByteArray & data, const NifModel * nif, const QModelIndex & index );

protected:
	bool openFile( const QString & name, const NifModel * nif, const QModelIndex & index );
	bool openData( const QByteArray & data );
	virtual bool readFile( QDataStream & in );

	QStringList textureList;

	// Is not JSON format or otherwise unreadable
	bool readable = false;
	bool isBGEM = false;
	bool isBGSM = false;

	// BGSM & BGEM shared variables

	quint32 version = 0;
	quint32 tileFlags = 0;
	bool bTileU = false;
	bool bTileV = false;
	float fUOffset = 0;
	float fVOffset = 0;
	float fUScale = 1.0;
	float fVScale = 1.0;
	float fAlpha = 1.0;
	quint8 bAlphaBlend = 0;
	quint32 iAlphaSrc = 0;
	quint32 iAlphaDst = 0;
	quint8 iAlphaTestRef = 255;
	quint8 bAlphaTest = 0;
	quint8 bZBufferWrite = 1;
	quint8 bZBufferTest = 1;
	quint8 bScreenSpaceReflections = 0;
	quint8 bWetnessControl_ScreenSpaceReflections = 0;
	quint8 bDecal = 0;
	quint8 bTwoSided = 0;
	quint8 bDecalNoFade = 0;
	quint8 bNonOccluder = 0;
	quint8 bRefraction = 0;
	quint8 bRefractionFalloff = 0;
	float fRefractionPower = 0;
	quint8 bEnvironmentMapping = 0;
	float fEnvironmentMappingMaskScale = 1.0;
	quint8 bGrayscaleToPaletteColor = 1.0;
	quint8 ucMaskWrites = 63;

	Color3 cEmittanceColor = Color3( 0.0f, 0.0f, 0.0f );

	quint8 bGlowmap = 0;

	float fLumEmittance = 100.0;
	float fAdaptativeEmissive_ExposureOffset = 13.5;
	float fAdaptativeEmissive_FinalExposureMin = 2.0;
	float fAdaptativeEmissive_FinalExposureMax = 3.0;
};


class ShaderMaterial : public Material
{
	Q_OBJECT

	friend class Renderer;
	friend class BSShaderLightingProperty;
	friend class BSLightingShaderProperty;
	friend class NifModel;

public:
	ShaderMaterial( const QString & name, const NifModel * nif, const QModelIndex & index );
	//! Parse straight from file bytes, no NifModel resource context needed.
	explicit ShaderMaterial( const QByteArray & data );
	quint32 shaderFlags2() const {
		return ( bEnableEditorAlphaRef ? 0x00000001U : 0U ) | ( bTranslucency ? 0x00000002U : 0U )
			| ( bTranslucencyThickObject ? 0x00000004U : 0U ) | ( bTranslucencyMixAlbedoWithSubsurfaceCol ? 0x00000008U : 0U )
			| ( bSpecularEnabled ? 0x00000010U : 0U ) | ( bPBR ? 0x00000020U : 0U ) | ( bCustomPorosity ? 0x00000040U : 0U )
			| ( bAnisoLighting ? 0x00000080U : 0U ) | ( bEmitEnabled ? 0x00000100U : 0U )
			| ( bModelSpaceNormals ? 0x00000200U : 0U ) | ( bExternalEmittance ? 0x00000400U : 0U )
			| ( bUseAdaptativeEmissive ? 0x00000800U : 0U ) | ( bReceiveShadows ? 0x00001000U : 0U )
			| ( bHideSecret ? 0x00002000U : 0U ) | ( bCastShadows ? 0x00004000U : 0U )
			| ( bDissolveFade ? 0x00008000U : 0U ) | ( bAssumeShadowmask ? 0x00010000U : 0U )
			| ( bGlowmap ? 0x00020000U : 0U ) | ( bHair ? 0x00040000U : 0U ) | ( bTree ? 0x00080000U : 0U )
			| ( bFacegen ? 0x00100000U : 0U ) | ( bSkinTint ? 0x00200000U : 0U ) | ( bTessellate ? 0x00400000U : 0U )
			| ( bSkewSpecularAlpha ? 0x00800000U : 0U ) | ( bTerrain ? 0x01000000U : 0U )
			| ( bRimLighting ? 0x02000000U : 0U ) | ( bSubsurfaceLighting ? 0x04000000U : 0U )
			| ( bBackLighting ? 0x08000000U : 0U ) | ( bEnvironmentMappingWindow ? 0x10000000U : 0U )
			| ( bEnvironmentMappingEye ? 0x20000000U : 0U );
	}
	Color3 specularColor() const { return cSpecularColor; }
	float specularStrength() const { return fSpecularMult; }
	float smoothness() const { return fSmoothness; }
	float fresnelPower() const { return fFresnelPower; }
	float emittanceMultiple() const { return fEmittanceMult; }
	float grayscaleToPaletteScale() const { return fGrayscaleToPaletteScale; }
	QString rootMaterialPath() const { return sRootMaterialPath; }

protected:
	bool readFile( QDataStream & in ) override final;

	quint8 bEnableEditorAlphaRef = 0;
	quint8 bRimLighting = 0;
	float fRimPower = 0;
	float fBacklightPower = 0;
	quint8 bSubsurfaceLighting = 0;
	float fSubsurfaceLightingRolloff = 0.0;
	quint8 bSpecularEnabled = 1;
	Color3 cSpecularColor;
	float fSpecularMult = 0;
	float fSmoothness = 0;
	float fFresnelPower = 0;
	float fWetnessControl_SpecScale = 0;
	float fWetnessControl_SpecPowerScale = 0;
	float fWetnessControl_SpecMinvar = 0;
	float fWetnessControl_EnvMapScale = 0;
	float fWetnessControl_FresnelPower = 0;
	float fWetnessControl_Metalness = 0;
	QString sRootMaterialPath;
	quint8 bAnisoLighting = 0;
	quint8 bEmitEnabled = 0;
	float fEmittanceMult = 0;
	quint8 bModelSpaceNormals = 0;
	quint8 bExternalEmittance = 0;
	quint8 bBackLighting = 0;
	quint8 bReceiveShadows = 1;
	quint8 bHideSecret = 0;
	quint8 bCastShadows = 1;
	quint8 bDissolveFade = 0;
	quint8 bAssumeShadowmask = 0;
	quint8 bEnvironmentMappingWindow = 0;
	quint8 bEnvironmentMappingEye = 0;
	quint8 bHair = 0;
	Color3 cHairTintColor = Color3( 0.0f, 0.0f, 0.0f );
	quint8 bTree = 0;
	quint8 bFacegen = 0;
	quint8 bSkinTint = 0;
	quint8 bTessellate = 0;
	float fDisplacementTextureBias = 0;
	float fDisplacementTextureScale = 0;
	float fTessellationPnScale = 0;
	float fTessellationBaseFactor = 0;
	float fTessellationFadeDistance = 0;
	float fGrayscaleToPaletteScale = 0;
	quint8 bSkewSpecularAlpha = 0;

	quint8 bPBR = 0;

	quint8 bTranslucency = 0;
	quint8 bTranslucencyThickObject = 0;
	quint8 bTranslucencyMixAlbedoWithSubsurfaceCol = 0;
	Color3 cTranslucencySubsurfaceColor = Color3( 0.0f, 0.0f, 0.0f );
	float fTranslucencyTransmissiveScale = 0.0;
	float fTranslucencyTurbulence = 0.0;

	quint8 bCustomPorosity = 0;
	float fPorosityValue = 0.0;

	quint8 bUseAdaptativeEmissive = 0;

	quint8 bTerrain = 0;
	float fTerrainThresholdFalloff = 0.0;
	float fTerrainTilingDistance = 0.0;
	float fTerrainRotationAngle = 0.0;
};


class EffectMaterial : public Material
{
	Q_OBJECT

	friend class Renderer;
	friend class BSShaderLightingProperty;
	friend class BSEffectShaderProperty;
	friend class NifModel;

public:
	EffectMaterial( const QString & name, const NifModel * nif, const QModelIndex & index );
	quint32 effectShaderFlags2() const {
		return ( bEnvironmentMapping ? 0x0001U : 0U ) | ( bBloodEnabled ? 0x0002U : 0U )
			| ( bEffectLightingEnabled ? 0x0004U : 0U ) | ( bFalloffEnabled ? 0x0008U : 0U )
			| ( bFalloffColorEnabled ? 0x0010U : 0U ) | ( bGrayscaleToPaletteAlpha ? 0x0020U : 0U )
			| ( bSoftEnabled ? 0x0040U : 0U ) | ( bGlowmap ? 0x0080U : 0U )
			| ( bEffectPbrSpecular ? 0x0100U : 0U ) | ( bGlassEnabled ? 0x0200U : 0U );
	}
	Color3 baseColor() const { return cBaseColor; }
	float baseColorScale() const { return fBaseColorScale; }
	float falloffStartAngle() const { return fFalloffStartAngle; }
	float falloffStopAngle() const { return fFalloffStopAngle; }
	float falloffStartOpacity() const { return fFalloffStartOpacity; }
	float falloffStopOpacity() const { return fFalloffStopOpacity; }
	float lightingInfluence() const { return fLightingInfluence; }
	quint8 environmentMapMinLod() const { return iEnvmapMinLOD; }
	float softFalloffDepth() const { return fSoftDepth; }

protected:
	bool readFile( QDataStream & in ) override final;

	quint8 bBloodEnabled = 0;
	quint8 bEffectLightingEnabled = 0;
	quint8 bFalloffEnabled = 0;
	quint8 bFalloffColorEnabled = 0;
	quint8 bGrayscaleToPaletteAlpha = 0;
	quint8 bSoftEnabled = 0;
	Color3 cBaseColor;
	float fBaseColorScale = 1.0;
	float fFalloffStartAngle = 1.0;
	float fFalloffStopAngle = 0;
	float fFalloffStartOpacity = 1.0;
	float fFalloffStopOpacity = 0;
	float fLightingInfluence = 1.0;
	quint8 iEnvmapMinLOD = 0;
	float fSoftDepth = 100.0;

	quint8 bEffectPbrSpecular = 0;
	// version >= 21
	quint8 bGlassEnabled = 0;
	Color3 cGlassFresnelColor;
	float fGlassRefractionScaleBase = 0.0f;
	float fGlassBlurScaleBase = 0.0f;
	// version >= 22
	float fGlassBlurScaleFactor = 0.0f;
};


#endif // MATERIAL_H
