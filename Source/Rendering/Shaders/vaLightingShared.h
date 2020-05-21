///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016, Intel Corporation
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of 
// the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
// SOFTWARE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Author(s):  Filip Strugar (filip.strugar@intel.com)
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "vaSharedTypes.h"

#ifndef VA_LIGHTING_SHARED_H
#define VA_LIGHTING_SHARED_H


// IBL integration algorithm
#define IBL_INTEGRATION_PREFILTERED_CUBEMAP         0
#define IBL_INTEGRATION_IMPORTANCE_SAMPLING         1

#define IBL_INTEGRATION                             IBL_INTEGRATION_PREFILTERED_CUBEMAP

// IBL irradiance source
#define IBL_IRRADIANCE_SH                           0
#define IBL_IRRADIANCE_CUBEMAP                      1

#define IBL_IRRADIANCE_SOURCE                       IBL_IRRADIANCE_CUBEMAP


#ifndef VA_COMPILED_AS_SHADER_CODE
namespace Vanilla
{
#endif

struct ShaderLightDirectional
{
    static const int    MaxLights                       = 16;

    vaVector3           Color;
    float               Intensity;                      // premultiplied by exposure

    vaVector3           Direction;
    float               Dummy1;

    vaVector4           SunAreaLightParams;             // special case Sun area light - if Sun.w is 0 then it's not the special case area light (sun)
};
//struct ShaderLightPoint - no longer used, just using spot instead
//{
//    static const int    MaxLights                       = 16;
//
//    vaVector3           Intensity;
//    float               Size;                           // distance from which to start attenuating or compute umbra/penumbra/antumbra / compute specular (making this into a 'sphere' light) - useful to avoid near-infinities for when close-to-point lights
//    vaVector3           Position;
//    float               EffectiveRadius;                // distance at which it is considered that it cannot effectively contribute any light and can be culled
//};
struct ShaderLightSpot
{
    static const int    MaxLights                       = 32;

    vaVector3           Color;							// stored as linear, tools should show srgb though
    float               Intensity;                      // premultiplied by exposure

    vaVector3           Position;
    float               Range;                          // distance at which it is considered that it cannot effectively contribute any light (also used for shadows)
    vaVector3           Direction;
    float               Size;                           // distance from which to start attenuating or compute umbra/penumbra/antumbra / compute specular (making this into a 'sphere' light) - useful to avoid near-infinities for when close-to-point lights
    float               SpotInnerAngle;                 // angle from Direction below which the spot light has the full intensity (a.k.a. inner cone angle)
    float               SpotOuterAngle;                 // angle from Direction below which the spot light intensity starts dropping (a.k.a. outer cone angle)
    
    float               CubeShadowIndex;                // if used, index of cubemap shadow in the cubemap array texture; otherwise -1
    float               Dummy1;
};

struct IBLProbeConstants
{
    vaMatrix4x4             WorldToGeometryProxy;               // used for parallax geometry proxy
    vaMatrix4x4             WorldToFadeoutProxy;                // used to transition from (if enabled) local to (if enabled) distant IBL regions
    vaVector4               DiffuseSH[9];

    uint                    Enabled;
    float                   PreExposedLuminance;
    float                   MaxReflMipLevel;
    float                   Pow2MaxReflMipLevel;                // = (float)(1 << (uint)MaxMipLevel)

    vaVector3               Position;                           // cubemap capture position
    float                   ReflMipLevelClamp;                  // either == to MaxReflMipLevel, or slightly lower to reduce impact of low resolution at the last cube MIP

    vaVector3               Extents;                            // a.k.a. size / 2
    float                   UseProxy;
};

struct LightingShaderConstants
{
    static const int        MaxShadowCubes                  = 8;   // so the number of cube faces is x6 this - lots of RAM

    vaVector4               AmbientLightIntensity;                  // TODO: remove this

    // See vaFogSphere for descriptions
    vaVector3               FogCenter;
    int                     FogEnabled;

    vaVector3               FogColor;
    float                   FogRadiusInner;

    float                   FogRadiusOuter;
    float                   FogBlendCurvePow;
    float                   FogBlendMultiplier;
    float                   FogRange;                   // FogRadiusOuter - FogRadiusInner

    vaMatrix4x4             EnvmapRotation;             // ideally we shouldn't need this but at the moment we support this to simplify asset side...
    
    int                     EnvmapEnabled;
    float                   EnvmapMultiplier;
    uint                    LightCountDirectional;
    uint                    LightCountSpotAndPoint;

    uint                    LightCountSpotOnly;
    float                   ShadowCubeDepthBiasScale;           // scaled by 1.0 / (float)m_shadowCubeResolution
    float                   ShadowCubeFilterKernelSize;         // scaled by 1.0 / (float)m_shadowCubeResolution
    float                   ShadowCubeFilterKernelSizeUnscaled; // same as above but not scaled by 1.0 / (float)m_shadowCubeResolution

    vaVector2               AOMapTexelSize;                     // one over texture resolution
    int                     AOMapEnabled;
    int                     Padding0;


    ShaderLightDirectional  LightsDirectional[ShaderLightDirectional::MaxLights];
    //ShaderLightPoint        LightsPoint[ShaderLightPoint::MaxLights];
    ShaderLightSpot         LightsSpotAndPoint[ShaderLightSpot::MaxLights];

    IBLProbeConstants       LocalIBL;
    IBLProbeConstants       DistantIBL;

    //
    // vaVector4               ShadowCubes[MaxShadowCubes];    // .xyz is cube center and .w is unused at the moment
};

#ifndef VA_COMPILED_AS_SHADER_CODE
} // namespace Vanilla
#endif


#endif // #ifndef VA_LIGHTING_SHARED_H
