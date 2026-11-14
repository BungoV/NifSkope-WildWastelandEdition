// Copyright 2026 Adobe (underlying Airy reference); F4FS modifications.
// SPDX-License-Identifier: Apache-2.0
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy at http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.
//
// OpenPBR thin-film interference: a complex-Airy transfer-matrix evaluation at
// three fixed wavelengths (610, 550, 460 nm), copied from PR #111
// (openpbr_surface.hlsli, openpbr_film_domain.hlsli). Also compiled as C++ by
// src/Materials/OpenPbrMath.h, so it uses only the common subset of HLSL and C++.
// OpenPbrThinFilm needs a positive thickness; callers skip the film otherwise.
#ifndef F4FS_OPENPBR_FILM_HLSLI_INCLUDED
#define F4FS_OPENPBR_FILM_HLSLI_INCLUDED

#ifndef __cplusplus
#define OPENPBR_LOOP [loop]
#else
#define OPENPBR_LOOP
#endif
static const float OpenPbrPi = 3.14159265358979323846f;

float OpenPbrFinite(float v, float fallback) { return isfinite(v) ? v : fallback; }
float OpenPbrUnit(float v) { return saturate(OpenPbrFinite(v,0.0f)); }
// Authored indices have no physical upper bound. Computed weighted indices use
// zero as a separate perfect-reflector endpoint, consumed before this validator.
float OpenPbrIor(float v) { return isfinite(v) && v>0 ? v : 1.0f; }
bool OpenPbrIsPerfectReflectorEndpoint(float relativeIor) { return relativeIor==0; }

// Complex arithmetic, needed to retain phase under dielectric TIR and for metals.
float2 OpenPbrCMul(float2 a,float2 b) {return float2(a.x*b.x-a.y*b.y,a.x*b.y+a.y*b.x);}
float2 OpenPbrCAdd(float2 a,float2 b) {return float2(a.x+b.x,a.y+b.y);}
float2 OpenPbrCSub(float2 a,float2 b) {return float2(a.x-b.x,a.y-b.y);}
float2 OpenPbrCScale(float2 a,float b) {return float2(a.x*b,a.y*b);}
float2 OpenPbrCDiv(float2 a,float2 b)
{
    float scale=max(abs(b.x),abs(b.y));
    if(scale==0) return float2(0,0);
    a=float2(a.x/scale,a.y/scale);b=float2(b.x/scale,b.y/scale);
    float d=b.x*b.x+b.y*b.y;
    return float2((a.x*b.x+a.y*b.y)/d,(a.y*b.x-a.x*b.y)/d);
}
float2 OpenPbrCSqrt(float2 a)
{
    // Preserve the real film wavevector without normalization/division noise;
    // even one extra ULP becomes visible after a long coherent optical path.
    if(a.y==0) return a.x>=0?float2(sqrt(a.x),0):float2(0,sqrt(-a.x));
    float scale=max(abs(a.x),abs(a.y));
    if(scale==0) return float2(0,0);
    float x=a.x/scale,y=a.y/scale,m=sqrt(x*x+y*y),factor=sqrt(scale);
    float large=sqrt(.5f*(m+abs(x))),smallComponent=abs(y)/(2*large);
    return x>=0?float2(factor*large,(a.y<0?-1.0f:1.0f)*factor*smallComponent):
        float2(factor*smallComponent,(a.y<0?-1.0f:1.0f)*factor*large);
}
// Polarized lossless-film amplitudes use complex longitudinal wavevectors.
// An evanescent film transmits by frustrated TIR; only an opaque/infinite film
// is a perfect reflector. Returned x/y are power reflectance/transmittance.
float2 OpenPbrAiryRTCoherent(float mu,float eta0,float eta1,float2 eta2,float thicknessNm,float wavelength,bool pPolarization)
{
    // Transfer-matrix form of complex Airy. A finite evanescent film can tunnel:
    // first-interface TIR is not a unit-reflection shortcut. Work with normal
    // wavevectors, and sin(delta)/delta to retain the critical-kz=0 limit.
    mu=OpenPbrUnit(abs(mu));
    // The uniform-medium (or zero-film index-matched) transparent limit also
    // defines the otherwise undefined zero-normal-flux endpoint continuously.
    if(eta2.x==eta0 && eta2.y==0 && (eta1==eta0 || thicknessNm<=0)) return float2(0,1);
    float normal0=eta0*mu,normal02=normal0*normal0,eps1=eta1*eta1;
    // Factored square differences preserve a uniform medium at tiny grazing
    // cosines; eta^2-eta0^2*(1-mu^2) loses the entire normal component there.
    float normal12=(eta1-eta0)*(eta1+eta0)+normal02;
    float2 eps2=OpenPbrCMul(eta2,eta2);
    float2 normal22=float2((eta2.x-eta0)*(eta2.x+eta0)-eta2.y*eta2.y+normal02,2*eta2.x*eta2.y);
    float2 kz1=OpenPbrCSqrt(float2(normal12,0));
    float2 kz2=OpenPbrCSqrt(normal22);
    if(kz2.y<0) kz2=OpenPbrCScale(kz2,-1);
    float2 q0=float2(pPolarization?mu/eta0:eta0*mu,0);
    float2 q2=pPolarization?OpenPbrCDiv(kz2,eps2):kz2;
    float k=2*OpenPbrPi*max(thicknessNm,0.0f)/max(wavelength,1e-5f);
    float2 delta=OpenPbrCScale(kz1,k);
    // Scale cos/sin by exp(-Im(delta)); the common factor cancels, avoiding
    // overflow for optically thick evanescent films while preserving tunneling.
    float decay=exp(-2*max(delta.y,0.0f));
    float2 cs=float2(0.5f*(1+decay)*cos(delta.x),0.5f*(decay-1)*sin(delta.x));
    float2 sn=float2(0.5f*(1+decay)*sin(delta.x),0.5f*(1-decay)*cos(delta.x));
    float2 sinc;
    if(delta.x*delta.x+delta.y*delta.y<1e-6f)
        sinc=OpenPbrCScale(OpenPbrCSub(float2(1,0),OpenPbrCScale(OpenPbrCMul(delta,delta),1.0f/6.0f)),exp(-max(delta.y,0.0f)));
    else sinc=OpenPbrCDiv(sn,delta);
    float2 q1sin=OpenPbrCScale(sinc,k*normal12/(pPolarization?eps1:1.0f));
    float2 crossSin=OpenPbrCScale(OpenPbrCMul(OpenPbrCMul(q0,q2),sinc),k*(pPolarization?eps1:1.0f));
    float2 difference=OpenPbrCSub(q1sin,crossSin),sum=OpenPbrCAdd(q1sin,crossSin);
    float2 numerator=OpenPbrCAdd(OpenPbrCMul(OpenPbrCSub(q0,q2),cs),float2(-difference.y,difference.x));
    float2 denominator=OpenPbrCSub(OpenPbrCMul(OpenPbrCAdd(q0,q2),cs),float2(-sum.y,sum.x));
    float2 r=OpenPbrCDiv(numerator,denominator);
    float2 t=OpenPbrCDiv(OpenPbrCScale(q0,2*exp(-max(delta.y,0.0f))),denominator);
    float transmitted=q0.x>1e-12f?max(0.0f,q2.x/q0.x)*(t.x*t.x+t.y*t.y):0;
    return float2(saturate(r.x*r.x+r.y*r.y),saturate(transmitted));
}

float OpenPbrComplexMagnitude(float2 z)
{
    float s=max(abs(z.x),abs(z.y));
    if(s==0) return 0;
    float x=z.x/s,y=z.y/s;
    return s*sqrt(x*x+y*y);
}
float OpenPbrInterfaceReflectance(float2 u0,float log0,float2 u1,float log1)
{
    float scale=max(log0,log1);
    float2 a=OpenPbrCScale(u0,exp2(log0-scale)),b=OpenPbrCScale(u1,exp2(log1-scale));
    float2 r=OpenPbrCDiv(OpenPbrCSub(a,b),OpenPbrCAdd(a,b));
    return saturate(r.x*r.x+r.y*r.y);
}
float2 OpenPbrAiryRTScaled(float mu,float eta0,float eta1,float2 eta2,float thickness,float wavelength,bool pol,bool thicknessIsNanometers)
{
    mu=OpenPbrUnit(abs(mu));
    if(eta2.x==eta0 && eta2.y==0 && (eta1==eta0 || thickness==0)) return float2(0,1);
    if(mu==0) return float2(1,0);
    float ln0=log2(eta0),ln1=log2(eta1);
    // Film wavevector uses only exterior and film, never the substrate scale.
    // Decide sign locally before taking logarithms: subtracting large rounded
    // log indices would erase a nextafter-sized contrast near critical angle.
    float pairScale=max(eta0,eta1),n0s=eta0/pairScale,n1s=eta1/pairScale;
    float normal0=n0s*mu,normal12=(n1s-n0s)*(n1s+n0s)+normal0*normal0;
    bool evanescent=normal12<0,critical=normal12==0;
    float lk1=critical?-10000:log2(pairScale)+.5f*log2(abs(normal12));
    if(mu==1) {evanescent=false;critical=false;lk1=ln1;}
    if(eta1==eta0) {evanescent=false;critical=false;lk1=ln0+log2(mu);}
    float lq1=lk1-(pol?2*ln1:0);
    float2 uq1=evanescent?float2(0,1):float2(1,0);
    if(critical) uq1=float2(0,0);
    float lq0=(pol?-ln0:ln0)+log2(mu);
    // Independently scaled substrate longitudinal wavevector/admittance.
    float substrateScale=max(eta2.x,eta2.y),s2=max(substrateScale,eta0),ns0=eta0/s2;
    float logScale2=log2(s2),nr=eta2.x/s2,ni=eta2.y/s2,normal02=ns0*mu;
    float2 z2=float2((nr-ns0)*(nr+ns0)-ni*ni+normal02*normal02,2*nr*ni);
    float2 k2=OpenPbrCSqrt(z2);
    float m2=OpenPbrComplexMagnitude(k2),lq2=m2>0?logScale2+log2(m2):-10000;
    float2 uq2=m2>0?OpenPbrCScale(k2,1/m2):float2(0,0);
    if(mu==1) {
        float nx=eta2.x/substrateScale,ny=eta2.y/substrateScale,nm=sqrt(nx*nx+ny*ny);
        uq2=float2(nx/nm,ny/nm);lq2=log2(substrateScale)+log2(nm);
    }
    if(pol) {
        float ns=max(eta2.x,eta2.y),nx=eta2.x/ns,ny=eta2.y/ns,nm=sqrt(nx*nx+ny*ny);
        float2 un=float2(nx/nm,ny/nm);
        uq2=OpenPbrCDiv(uq2,OpenPbrCMul(un,un));
        lq2-=2*(log2(ns)+log2(nm));
    }
    if(eta2.x==eta0 && eta2.y==0) {uq2=float2(1,0);lq2=lq0;}
    if(thickness==0) {
        float r=OpenPbrInterfaceReflectance(float2(1,0),lq0,uq2,lq2);
        return float2(r,1-r);
    }
    // Keep the caller's original thickness units. A nm -> um -> nm round trip
    // can gain one GPU reciprocal-rounding ULP and visibly shift a coherent film.
    float logK=log2(thickness)+log2((thicknessIsNanometers?2:2000)*OpenPbrPi/wavelength);
    float logDelta=logK+lk1;
    // Float phase uncertainty grows with optical path. Smoothly approach a
    // phase-averaged RGB response over 2^15..2^17 radians, never clamp thickness.
    // This is an explicit unresolved-phase approximation, not spectral parity.
    float phaseBlend=(!evanescent && !critical)?smoothstep(15.0f,17.0f,logDelta):0;
    float2 averaged=float2(0,0);
    if(phaseBlend>0) {
        float r01=OpenPbrInterfaceReflectance(float2(1,0),lq0,uq1,lq1);
        float r12=OpenPbrInterfaceReflectance(uq1,lq1,uq2,lq2);
        float a=1-r01,b=1-r12,den=a+r01*b;
        averaged=den>0?float2(saturate(r01+a*a*r12/den),saturate(a*b/den)):float2(1,0);
        if(phaseBlend==1) return averaged;
    }
    // exp(-2*64) is below binary32's smallest subnormal: tunneling power has
    // reached its representable zero, separately from propagating phase mixing.
    if(evanescent && logDelta>=6) return float2(1,0);
    float delta=critical?0:exp2(logDelta);
    // Preserve the established coherent matrix where all intermediates fit.
    if(eta0>=.01f && eta0<=100 && eta1>=.01f && eta1<=100 &&
       eta2.x>=.01f && eta2.x<=100 && eta2.y<=100 && mu>=1e-8f && thickness<=(thicknessIsNanometers?1e7f:10000)) {
        float2 resolved=OpenPbrAiryRTCoherent(mu,eta0,eta1,eta2,thicknessIsNanometers?thickness:thickness*1000,wavelength,pol);
        return float2(lerp(resolved.x,averaged.x,phaseBlend),lerp(resolved.y,averaged.y,phaseBlend));
    }
    float decay=evanescent?exp(-2*delta):1;
    float2 cs=evanescent?float2(.5f*(1+decay),0):float2(cos(delta),0);
    float2 sn=evanescent?float2(0,.5f*(1-decay)):float2(sin(delta),0);
    float2 sinc;
    if(delta<.001f) sinc=float2((1+(evanescent?1:-1)*delta*delta/6)*(evanescent?exp(-delta):1),0);
    else sinc=evanescent?float2(sn.y/delta,0):float2(sn.x/delta,0);
    // q1*sin(delta) and q0*q2*sin(delta)/q1 retain the exact critical limit.
    float la=critical?-10000:logK+2*lk1-(pol?2*ln1:0);
    float lb=lq0+lq2+logK+(pol?2*ln1:0);
    float scale=max(max(lq0,lq2),max(la,lb));
    float2 q0=OpenPbrCScale(float2(1,0),exp2(lq0-scale));
    float2 q2=OpenPbrCScale(uq2,exp2(lq2-scale));
    float2 qa=OpenPbrCScale(sinc,(evanescent?-1:1)*exp2(la-scale));
    float2 qb=OpenPbrCScale(OpenPbrCMul(uq2,sinc),exp2(lb-scale));
    float2 difference=OpenPbrCSub(qa,qb),sum=OpenPbrCAdd(qa,qb);
    float2 numerator=OpenPbrCAdd(OpenPbrCMul(OpenPbrCSub(q0,q2),cs),float2(-difference.y,difference.x));
    float2 denominator=OpenPbrCSub(OpenPbrCMul(OpenPbrCAdd(q0,q2),cs),float2(-sum.y,sum.x));
    float2 r=OpenPbrCDiv(numerator,denominator);
    float dn=denominator.x*denominator.x+denominator.y*denominator.y;
    float transmitted=dn>0?4*max(uq2.x,0.0f)*exp2(lq0+lq2-2*scale-(evanescent?2*delta*1.44269504089f:0))/dn:0;
    float2 coherent=float2(saturate(r.x*r.x+r.y*r.y),saturate(transmitted));
    return float2(lerp(coherent.x,averaged.x,phaseBlend),lerp(coherent.y,averaged.y,phaseBlend));
}
float2 OpenPbrAiryRTMicrometers(float mu,float eta0,float eta1,float2 eta2,float thickness,float wavelength,bool pol)
{
    return OpenPbrAiryRTScaled(mu,eta0,eta1,eta2,max(thickness,0.0f),wavelength,pol,false);
}
float2 OpenPbrAiryRT(float mu,float eta0,float eta1,float2 eta2,float thicknessNm,float wavelength,bool pol)
{
    return OpenPbrAiryRTScaled(mu,eta0,eta1,eta2,max(thicknessNm,0.0f),wavelength,pol,true);
}

float2 OpenPbrMetalIor(float reflectivity,float edgeTint)
{
    // Perfect reflectivity is handled by ThinFilm before this conversion.
    float r=OpenPbrUnit(reflectivity),s=sqrt(r);
    // Adobe's Gulbrandsen conversion is approximate for F82 tint; documented.
    float n=lerp((1+s)/(1-s),(1-r)/(1+r),OpenPbrUnit(edgeTint));
    float k=sqrt(max(0.0f,((n+1)*(n+1)*r-(n-1)*(n-1))/(1-r)));
    return float2(n,k);
}
float3 OpenPbrThinFilm(float cosine,float exteriorIor,float filmIor,float baseIor,
    float thicknessMicrometers,float3 metalF0,float3 edgeTint,float metalness)
{
    float mu=OpenPbrUnit(abs(cosine)),eta0=OpenPbrIor(exteriorIor),eta1=OpenPbrIor(filmIor);
    bool perfect=OpenPbrIsPerfectReflectorEndpoint(baseIor) || (!isfinite(baseIor) && baseIor>0);
    float eta2=perfect?1:OpenPbrIor(baseIor);
    float thickness=max(OpenPbrFinite(thicknessMicrometers,0.0f),0.0f);
    float mixture=OpenPbrUnit(metalness);
    float3 result=float3(0,0,0);
    // Fixed RGB wavelengths are a raster approximation, not spectral integration.
    // Explicit scalar access avoids dynamic vector indexing inside the flow
    // loop, which can otherwise require unrolling this spectral evaluator.
    OPENPBR_LOOP for(int c=0;c<3;++c) {
        float wavelength=c==0?610.0f:(c==1?550.0f:460.0f);
        float channelF0=c==0?metalF0.x:(c==1?metalF0.y:metalF0.z);
        float channelTint=c==0?edgeTint.x:(c==1?edgeTint.y:edgeTint.z);
        bool wantDielectric=mixture<1 && !perfect;
        bool wantMetal=mixture>0 && OpenPbrUnit(channelF0)<1;
        float2 nk=wantMetal?OpenPbrMetalIor(channelF0,channelTint):float2(0,0);
        // One call site: fxc inlines every call, and four copies of the Airy evaluator
        // tripled the compile time of every shader that includes this. Order is s then p.
        float dielectricSum=0,metalSum=0;
        OPENPBR_LOOP for(int j=0;j<4;++j) {
            bool metalBase=j>=2;
            if(metalBase?!wantMetal:!wantDielectric) continue;
            float reflectance=OpenPbrAiryRTMicrometers(mu,eta0,eta1,metalBase?nk:float2(eta2,0),
                thickness,wavelength,(j&1)!=0).x;
            if(metalBase) metalSum+=reflectance;
            else dielectricSum+=reflectance;
        }
        float dielectric=wantDielectric?0.5f*dielectricSum:1;
        float metal=wantMetal?0.5f*metalSum:1;
        float channelResult=lerp(dielectric,metal,mixture);
        if(c==0) result.x=channelResult;
        else if(c==1) result.y=channelResult;
        else result.z=channelResult;
    }
    return result;
}

// The factor that turns a Schlick-Fresnel specular lobe into its thin-film version:
// film reflectance over the Schlick reflectance the lobe already carries, blended by weight.
// F0 above 0.16 on the green channel is treated as a metal, anything else as a dielectric
// whose index follows from F0. The film sits under air; thickness is in micrometres.
float3 OpenPbrThinFilmScale(float3 F0, float VdotH, float thickness, float filmIor, float weight)
{
    float mu = saturate(VdotH);
    bool metal = F0.y > 0.16f;
    float f = saturate(F0.y);
    float s = sqrt(min(f, 0.16f));
    float baseIor = (1.0f + s) / (1.0f - s);
    float3 film = OpenPbrThinFilm(mu, 1.0f, filmIor, baseIor, thickness, F0, float3(1, 1, 1), metal ? 1.0f : 0.0f);
    float k = pow(1.0f - mu, 5.0f);
    float w = saturate(weight);
    return float3(
        lerp(1.0f, film.x / max(F0.x + (1.0f - F0.x) * k, 1e-4f), w),
        lerp(1.0f, film.y / max(F0.y + (1.0f - F0.y) * k, 1e-4f), w),
        lerp(1.0f, film.z / max(F0.z + (1.0f - F0.z) * k, 1e-4f), w));
}

#endif
