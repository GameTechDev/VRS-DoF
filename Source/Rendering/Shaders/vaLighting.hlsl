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

#include "vaShared.hlsl"
#include "vaLightingShared.h"

#include "vaPoissonDisk8.h"

#ifndef VA_LIGHTING_HLSL
#define VA_LIGHTING_HLSL


//TextureCube         g_EnvironmentMap            : register( T_CONCATENATER( SHADERGLOBAL_LIGHTING_ENVMAP_TEXTURESLOT_V ) );
TextureCubeArray    g_CubeShadowmapArray        : register( T_CONCATENATER( SHADERGLOBAL_LIGHTING_CUBE_SHADOW_TEXTURESLOT_V ) );

TextureCube         g_LocalIBLReflectionsMap    : register( T_CONCATENATER( LIGHTINGGLOBAL_LOCALIBL_REFROUGHMAP_TEXTURESLOT_V ) );
TextureCube         g_LocalIBLIrradianceMap     : register( T_CONCATENATER( LIGHTINGGLOBAL_LOCALIBL_IRRADIANCEMAP_TEXTURESLOT_V ) );

TextureCube         g_DistantIBLReflectionsMap  : register( T_CONCATENATER( LIGHTINGGLOBAL_DISTANTIBL_REFROUGHMAP_TEXTURESLOT_V ) );
TextureCube         g_DistantIBLIrradianceMap   : register( T_CONCATENATER( LIGHTINGGLOBAL_DISTANTIBL_IRRADIANCEMAP_TEXTURESLOT_V ) );

Texture2D           g_AOMap                     : register( T_CONCATENATER( SHADERGLOBAL_AOMAP_TEXTURESLOT_V ) );

cbuffer LightingConstantsBuffer                 : register( B_CONCATENATER( LIGHTINGGLOBAL_CONSTANTSBUFFERSLOT_V ) )
{
    LightingShaderConstants     g_Lighting;
}

// cbuffer DistantIBLConstantsBuffer               : register( B_CONCATENATER( LIGHTINGGLOBAL_DISTANTIBL_CONSTANTSBUFFERSLOT_V ) )
// {
//     IBLProbeConstants         g_DistantIBL;
// }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Per-shaded-pixel intermediate data structures
//

// Used to transfer any type of light (directional, point, spot, etc.) information to actual material lighting code.
struct LightParams
{
    float4  ColorIntensity;     // rgb, pre-exposed intensity
    float3  L;                  // light direction (worldspace)
    float   Dist;               // length of L vector before it was normalized
    float   Attenuation;
    float   NoL;
    bool    AddToEmissive;      // used to only exist under VA_RM_SPECIAL_EMISSIVE_LIGHT - removed #if for simplicity
};

#define VA_CUBE_SHADOW_TAP_COUNT (3)

// All cubemap shadows have same resolution & setup so they share a lot of the parameters
struct CubeShadowsParams
{
    float3      TapOffsets[VA_CUBE_SHADOW_TAP_COUNT];
//    float       ViewDepth;
//    float       Something;
};

struct IBLParams
{
    bool        UseLocal;               // local enabled
    bool        UseDistant;             // distant enabled
    
    float       LocalToDistantK;        // interpolation factor
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Stuff
//

float SampleAO( float2 svpos )
{
    if( g_Lighting.AOMapEnabled )
        return g_AOMap.SampleBias( g_samplerLinearClamp, svpos * g_Lighting.AOMapTexelSize, -0.75 ).x;  // MIP bias is only relevant when using VRS above 2x2 rate and g_AOMap having precomputed MIP
    else
        return 1.0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IBL
//

IBLParams ComputeIBLParams( const float noise, const float noiseAttenuation, const float3 worldspacePos, const float3 geometricNormal )
{
    // some global settings
    const float transitionArea              = 0.25;

    IBLParams ret;

    ret.UseLocal        = g_Lighting.LocalIBL.Enabled;
    ret.UseDistant      = g_Lighting.DistantIBL.Enabled;

    ret.LocalToDistantK = saturate( ret.UseDistant - ret.UseLocal );
    
    if( ret.UseLocal && g_Lighting.LocalIBL.UseProxy )
    {
        float3 dist             = mul( g_Lighting.LocalIBL.WorldToFadeoutProxy, float4( worldspacePos, 1 ) ).xyz;

        // this really is a bad hack
#ifdef VA_RM_LOCALIBL_NORMALBIAS
        dist += mul( (float3x3)g_Lighting.LocalIBL.WorldToFadeoutProxy, geometricNormal ) * VA_RM_LOCALIBL_NORMALBIAS;
#endif
        dist     = abs( dist );

        float md = max( dist.x, max( dist.y, dist.z ) );

        float materialBasedBias = 0.0;
#ifdef VA_RM_LOCALIBL_BIAS
        materialBasedBias = VA_RM_LOCALIBL_BIAS;
#endif

        ret.LocalToDistantK     = saturate( 1 - (1-md) / transitionArea - materialBasedBias );
    }
    if( ret.LocalToDistantK < 0.001 )
        ret.UseDistant = 0;
    if( ret.LocalToDistantK > 0.999 )
        ret.UseLocal = 0;

    return ret;
}

float3 ComputeIBLDirection( const float3 worldspacePos, const IBLProbeConstants probe, float3 direction )
{
    [branch]
    if( probe.UseProxy )
    {
        // https://developer.valvesoftware.com/wiki/Parallax_Corrected_Cubemaps

        float3 ReflDirectionWS = direction;

        // Intersection with OBB convert to unit box space
        // Transform in local unit parallax cube space (scaled and rotated)
        float3 RayLS        = mul( (float3x3)probe.WorldToGeometryProxy, ReflDirectionWS );
        float3 PositionLS   = mul( probe.WorldToGeometryProxy, float4( worldspacePos, 1 ) ).xyz;

        float3 Unitary = float3(1.0f, 1.0f, 1.0f);
        float3 FirstPlaneIntersect  = (Unitary - PositionLS) / RayLS;
        float3 SecondPlaneIntersect = (-Unitary - PositionLS) / RayLS;
        float3 FurthestPlane = max(FirstPlaneIntersect, SecondPlaneIntersect);
        float Distance = min(FurthestPlane.x, min(FurthestPlane.y, FurthestPlane.z));

        // Use Distance in WS directly to recover intersection
        float3 IntersectPositionWS = worldspacePos + ReflDirectionWS * Distance;
        return IntersectPositionWS - probe.Position;
    }
    else
    {
        return direction;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Shadows
//

CubeShadowsParams ComputeCubeShadowsParams( float noise, float noiseAttenuation, float3 worldspacePos )
{
    CubeShadowsParams ret;

    float angle = noise * (3.14159265 * 2.0);

    float ca = cos( angle );
    float sa = sin( angle );
    float scale = g_Lighting.ShadowCubeFilterKernelSize;

    // make sure the kernel size is (roughly) independent of the number of taps!
    float refit = 1.0 / clamp( g_poissonDisk[VA_CUBE_SHADOW_TAP_COUNT-1].z, 0.25, 1.0 );

    // think of a better way to use gradient to scale shadow kernel
    // refit *= 1 + max( 0, max( length( ddx(worldspacePos.xyz) ), length( ddy(worldspacePos.xyz ) ) ) * 50.0 - 0.01 );

    scale *= refit;

    float2x2 RotScaleMatrix = float2x2( scale * ca, scale * -sa, scale * sa, scale * ca );

    [unroll]
    for( uint tap = 0; tap < VA_CUBE_SHADOW_TAP_COUNT; tap++ )
    {
        float2 rotatedOffset = mul( RotScaleMatrix, g_poissonDisk[tap].xy );
        ret.TapOffsets[tap] = float3( rotatedOffset, 1.0 + g_poissonDisk[tap].z * g_Lighting.ShadowCubeFilterKernelSizeUnscaled * refit );
    }

    // ret.ViewDepth = dot( worldspacePos.xyz - g_Global.CameraWorldPosition.xyz - g_Global.GlobalWorldBase.xyz, g_Global.CameraDirection.xyz );
    // ret.Something = ;

    return ret;
}

float ComputeCubeShadow( CubeShadowsParams cubeShadowParams, float3 worldGeometryNormal, int cubeShadowIndex, float3 pixelToLightDir, float pixelToLightLength, float lightSize, float lightRange )
{
    // This is to get the slope between the shadow receiving geometry and the light (shadow) direction
    float NoL = dot(worldGeometryNormal,pixelToLightDir);
    //float slopeBias = tan(acos(NoL));
    float slopeBias = sqrt(1 - NoL*NoL) / NoL;  // same as above ^ but with no trigonometry functions
    slopeBias = min( 8.0, slopeBias );          // arbitrarily limit the slope bias, otherwise it can cause light bleeding artifact; 10, 12 are good values too

    // This is the base direction we're sampling the cubemap at
    float3 cubeDir = -pixelToLightDir;

    float3 absCubeDir = abs( cubeDir );
        
#if 0   // not needd for now
    uint longestComponentIndex = 2;
    if( (absCubeDir.z > absCubeDir.x) * (absCubeDir.z > absCubeDir.y) == 0 )
        longestComponentIndex = (absCubeDir.x > absCubeDir.y)?(0):(1);
    float longestComponent = absCubeDir[longestComponentIndex];
    float2 shorterComponents = float2( absCubeDir[(longestComponentIndex+1)%3], (longestComponentIndex+2)%3 );
#else
    float longestComponent = max( max( absCubeDir.x, absCubeDir.y), absCubeDir.z );
#endif

    float cubeDepth = longestComponent * pixelToLightLength;

    // Worldspace cube shadow pixel size: I think this is right? halfPixel / resolution * distance along face axis?
    // (use sqrt(2)/2==0.7071 instead because it is needed when the pixel 'cube' is projected diagonally or something like that)
    float worldspaceDepthBiasScale = g_Lighting.ShadowCubeDepthBiasScale * cubeDepth;

    // TODO: maybe precompute these during light setup?
    const float a = lightSize / ( lightSize - lightRange );
    const float b = ( lightSize * lightRange ) / ( lightRange - lightSize );

    const float fixedAndSlopeBias = 1+slopeBias;

    // const float screenSpaceScaling = 1.0 + max( 0, cubeShadowParams.Something ) * 10.0;

#if 0   // manual compare
    // Apply the fixed and slope-based biases
    cubeDepth   -= (worldspaceDepthBiasScale * fixedAndSlopeBias);
    // Convert to cubemap projected depth (NDC z)
    float projZ = (cubeDepth * a + b) / ( cubeDepth );    // see PerspectiveFovLH; camera is using 'm_useReversedZ' so near/far are swapped

    float shadowmapCompVal = g_CubeShadowmapArray.SampleLevel( g_samplerPointClamp, float4( cubeDir, cubeShadowIndex ), 0.0 ).x;
    return( shadowmapCompVal < projZ );
#elif 0 // use cmp sampler, 1-tap
    worldspaceDepthBiasScale *= 1.5; // increase bias to cover 2x2 block from the LinearCmpSampler
    // Apply the fixed and slope-based biases
    cubeDepth   -= (worldspaceDepthBiasScale * fixedAndSlopeBias);
    // Convert to cubemap projected depth (NDC z)
    float projZ = (cubeDepth * a + b) / ( cubeDepth );    // see PerspectiveFovLH; camera is using 'm_useReversedZ' so near/far are swapped
    return g_CubeShadowmapArray.SampleCmpLevelZero( g_samplerLinearCmpSampler, float4( cubeDir, cubeShadowIndex ), projZ ).x;
#else   // use multi-tap with cmp sampler

    float3 jitterX, jitterY;
    ComputeOrthonormalBasis( cubeDir, jitterX, jitterY );
        
    // precompute bias basis before the loop
    const float tapProjZBase = (cubeDepth * a + b) / ( cubeDepth );
    const float cubeDepthBiased1 = cubeDepth-fixedAndSlopeBias*worldspaceDepthBiasScale;
    const float tapProjZBaseB1 = (cubeDepthBiased1 * a + b) / ( cubeDepthBiased1 ) - tapProjZBase;

    float shadowSum = 0.0;
    [unroll]
    for( uint tap = 0; tap < VA_CUBE_SHADOW_TAP_COUNT; tap++ )
    {
        // A LOT of this can be precomputed
        float2 tapOffset        = /*screenSpaceScaling **/ cubeShadowParams.TapOffsets[tap].xy;   //mul( cubeShadowParams.RotScaleMatrix, g_poissonDisk[tap].xy );
        float tapDepthBiasSize  = /*screenSpaceScaling **/ cubeShadowParams.TapOffsets[tap].z;    //(1.0 + g_poissonDisk[tap].z * g_Lighting.ShadowCubeFilterKernelSizeUnscaled);

#if 0
        float tapDepth = cubeDepth - (tapDepthBiasSize * worldspaceDepthBiasScale * fixedAndSlopeBias);
        float tapProjZ = (tapDepth * a + b) / ( tapDepth );    // most of this could go outside of the loop as only the first part (tapDepth * a) changes enough
#else
        // using precomputed bias basis
        float tapProjZ = tapProjZBase + tapProjZBaseB1 * tapDepthBiasSize;
#endif

        float3 tapDir = normalize( cubeDir + jitterX * tapOffset.x + jitterY * tapOffset.y );
        float tapShadow = g_CubeShadowmapArray.SampleCmpLevelZero( g_samplerLinearCmpSampler, float4( tapDir, cubeShadowIndex ), tapProjZ ).x;
        //float tapShadow = g_CubeShadowmapArray.SampleLevel( g_samplerPointClamp, float4( tapDir, cubeShadowIndex ), 0.0 ).x < tapProjZ;

#define SHADOW_USE_NON_LINEAR_AVERAGE

#ifdef SHADOW_USE_NON_LINEAR_AVERAGE
        shadowSum += sqrt(tapShadow);
    }
    shadowSum /= (float)VA_CUBE_SHADOW_TAP_COUNT;
    return shadowSum * shadowSum;
#else
        shadowSum += tapShadow;
    }
    shadowSum /= (float)VA_CUBE_SHADOW_TAP_COUNT;
    return shadowSum;
#endif
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Fog/atmospherics
//

float LightingGetFogK( float3 surfaceWorldspacePos )
{
    if( g_Lighting.FogEnabled )
    {
        float dist = length( surfaceWorldspacePos - g_Lighting.FogCenter);
        return g_Lighting.FogBlendMultiplier * pow( saturate( (dist - g_Lighting.FogRadiusInner) / g_Lighting.FogRange), g_Lighting.FogBlendCurvePow );
    }
    else
    {
        return 0.0;
    }
}

float3 LightingApplyFog( float3 surfaceWorldspacePos, float3 inColor )
{
    return lerp( inColor, g_Lighting.FogColor, LightingGetFogK( surfaceWorldspacePos ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // #ifndef VA_LIGHTING_HLSL