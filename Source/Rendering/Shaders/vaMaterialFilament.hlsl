///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2019, Intel Corporation
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
// Based on:    https://github.com/google/filament
//
// Filament is the best opensource PBR renderer (that I could find), including excellent documentation and 
// performance. The size and complexity of the Filament library itself is a bit out of scope of what Vanilla is 
// intended to provide so it's not used directly but its PBR shaders and material  definitions are integrated into 
// Vanilla (mostly) as they are. Original Filament shaders are included into the repository but only some are 
// used directly - the rest are in as a reference for enabling easier integration of any future Filament changes.
//
// There are subtle differences in the implementation though, so beware.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef VA_MATERIAL_BASIC_INCLUDE_AS_UTILITY
#include "vaRenderMesh.hlsl"
#endif

#include "vaRenderingShared.hlsl"
#include "vaLighting.hlsl"

#if defined( VA_INTEL_EXT_GRADIENT_FILTER_EXTENSION_ENABLED )
#include "Intel\IntelExtensions_InternalUseOnly12.hlsl"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Set up various Filament-specific macros
// 
#if defined( VA_FILAMENT_STANDARD )
#elif defined( VA_FILAMENT_SUBSURFACE )
#define SHADING_MODEL_SUBSURFACE    // filament definition
#elif defined(VA_FILAMENT_CLOTH)
#define SHADING_MODEL_CLOTH         // filament definition
#elif defined( VA_FILAMENT_SPECGLOSS )
#define SHADING_MODEL_SPECULAR_GLOSSINESS
#else
#error Correct version of filament material model has to be set using one of the VA_FILAMENT_* macros
#endif
// Note: Filament SHADING_MODEL_UNLIT is never defined
//
#if VA_RM_ADVANCED_SPECULAR_SHADER
#define GEOMETRIC_SPECULAR_AA
#define GEOMETRIC_SPECULAR_AA_USE_NORMALMAPS

#if 1 // this one is a lot more aggressive AA but it seems to be worth it
#define GEOMETRIC_SPECULAR_AA_VARIANCE      (0.03)
#define GEOMETRIC_SPECULAR_AA_THRESHOLD     (0.01)
#else
#define GEOMETRIC_SPECULAR_AA_VARIANCE      (0.02)
#define GEOMETRIC_SPECULAR_AA_THRESHOLD     (0.002)
#endif

#endif
//
// we just always enable these and hope they get compiled out when unused (seems to happen in testing so far)
#define MATERIAL_HAS_NORMAL                 1           // #if defined( VA_RM_HAS_INPUT_Normal )
#define MATERIAL_HAS_EMISSIVE               1           // #if defined( VA_RM_HAS_INPUT_EmissiveColor )
#if defined(MATERIAL_HAS_CLEAR_COAT)
#define MATERIAL_HAS_CLEAR_COAT_NORMAL      1
#endif
//
// 
// These are the defines that I haven't ported/connected yet!
//
// #define MATERIAL_HAS_ANISOTROPY         1
// #define SPECULAR_AMBIENT_OCCLUSION      1
// #define MULTI_BOUNCE_AMBIENT_OCCLUSION  1
// #define TARGET_MOBILE
//
// port Vanilla to Filament blend modes
#if VA_RM_TRANSPARENT
#define BLEND_MODE_TRANSPARENT
#elif VA_RM_ALPHATEST
#define BLEND_MODE_MASKED
#else
#define BLEND_MODE_OPAQUE
#endif
#if VA_RM_TRANSPARENT && VA_RM_ALPHATEST
#error this scenario has not been tested with filament (why use alpha testing and transparency at the same time? might be best to disable it in the UI if not needed)
#endif
//
// these are all Filament blend modes listed:
// #define BLEND_MODE_OPAQUE
// #define BLEND_MODE_MASKED
// #define BLEND_MODE_ADD
// #define BLEND_MODE_TRANSPARENT
// #define BLEND_MODE_MULTIPLY
// #define BLEND_MODE_SCREEN
// #define BLEND_MODE_FADE
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This is where the Filament core gets included
#include "vaMaterialFilamentIncludes.hlsl"
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// These are mostly Vanilla-specific functions
//

// Load from the actual material (textures, constants, etc.) and set some of the dependant filament defines!
MaterialInputs LoadMaterial( const RenderMaterialInterpolants vertex )
{
    const RenderMaterialInputs materialInputs = LoadRenderMaterialInputs( vertex );

    MaterialInputs material = InitMaterial();

#if defined( VA_RM_HAS_INPUT_BaseColor )
    material.BaseColor  = vertex.Color.rgba * materialInputs.BaseColor;
#endif

#if defined( VA_RM_HAS_INPUT_Normal )
    // load normalmap (could be texture or static)
    material.Normal = materialInputs.Normal.xyz;
#endif

#if defined( VA_RM_HAS_INPUT_EmissiveColor )
    material.EmissiveColor = materialInputs.EmissiveColor.xyz;
#endif

#if defined( VA_RM_HAS_INPUT_EmissiveIntensity )
    material.EmissiveIntensity = materialInputs.EmissiveIntensity.x;
#endif

#if defined( VA_RM_HAS_INPUT_Roughness ) && !defined(SHADING_MODEL_SPECULAR_GLOSSINESS)
    material.Roughness = materialInputs.Roughness.x;
#endif

#if defined( VA_RM_HAS_INPUT_Metallic )
    material.Metallic = materialInputs.Metallic.x;
#endif

#if defined( VA_RM_HAS_INPUT_Reflectance )
    material.Reflectance = materialInputs.Reflectance.x;
#endif

#if defined( VA_RM_HAS_INPUT_AmbientOcclusion )
    material.AmbientOcclusion = materialInputs.AmbientOcclusion.x;
#endif

    // this just to support a different way of defining/overriding alpha (instead of using BaseColor)
#if defined( VA_RM_HAS_INPUT_Opacity )
    material.BaseColor.a *= materialInputs.Opacity.x;
#endif

#ifdef VA_RM_HAS_INPUT_SpecularColor
    material.SpecularColor  = materialInputs.SpecularColor.rgb;
#endif

#ifdef VA_RM_HAS_INPUT_Glossiness
    material.Glossiness = materialInputs.Glossiness.x;
#endif

    // InvGlossiness is basically roughness and it's weird but it's basically what Amazon Lumberyard Bistro dataset has in its textures so.. just go with it I guess?
#ifdef VA_RM_HAS_INPUT_InvGlossiness
    material.Glossiness = 1.0 - materialInputs.InvGlossiness.x;
#endif


    // this is no longer needed with the new system!
#if defined( VA_RM_HAS_INPUT_ARMHack )
#error this goes out!
    float3 ARMHack = materialInputs.ARMHack.rgb;
    material.AmbientOcclusion   = ARMHack.r;
    material.Roughness          = ARMHack.g;
    material.Metallic           = ARMHack.b;
#endif

    return material;
}

void AlphaDiscard( const MaterialInputs material )
{
    if( (material.BaseColor.a+g_Global.WireframePass) < VA_RM_ALPHATEST_THRESHOLD )
        discard;
}

float4 EvaluateMaterialAndLighting( const RenderMaterialInterpolants vertex, const ShadingParams shading, const PixelParams pixel, const MaterialInputs material )
{
    float3 diffuseColor     = 0.0.xxx;
    float3 specularColor    = 0.0.xxx;

    [branch]
    if( shading.IBL.UseLocal || shading.IBL.UseDistant )
        evaluateIBL( shading, material, pixel, diffuseColor, specularColor );

// #if defined(HAS_DIRECTIONAL_LIGHTING)
     evaluateDirectionalLights( shading, material, pixel, diffuseColor, specularColor );
// #endif

// #if defined(HAS_DYNAMIC_LIGHTING)
     evaluatePunctualLights( vertex, shading, pixel, diffuseColor, specularColor );
// #endif

#if defined(MATERIAL_HAS_EMISSIVE) && (VA_RM_SPECIAL_EMISSIVE_LIGHT == 0)
    diffuseColor.xyz += shading.PrecomputedEmissive;
#endif

    float transparency = computeDiffuseAlpha(material.BaseColor.a);

    // we don't want alpha blending to fade out speculars (at least not completely)
    specularColor /= max( 0.01, transparency );

    // specular dampening for VRS - this is useful on some materials but I haven't added the switches yet
#if 0 
    float2 shadingRate  = float2( ddx( vertex.Position.x ), ddy( vertex.Position.y ) );
    float VRSScale      = sqrt( ( shadingRate.x + shadingRate.y ) * 0.5 );  // is 1 for VRS 1x1
    specularColor       /= (VRSScale * 0.2 + 0.8);            // no change for VRS 1x1
#endif


    float4 finalColor = float4( diffuseColor + specularColor, transparency );

//
//#if defined(VA_FILAMENT_STANDARD)
//    float4 finalColor = float4( 1, 0, 0, 1 );
//#elif defined(VA_FILAMENT_SUBSURFACE)
//    float4 finalColor = float4( 0, 1, 0, 1 );
//#elif defined(VA_FILAMENT_CLOTH)
//    float4 finalColor = float4( 0, 0, 1, 1 );
//#else
//#error Correct version of filament material model has to be set using one of the VA_FILAMENT_* macros
//#endif
//
    return finalColor;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SHADER ENTRY POINTS
//

// depth test or a simple shadow map
void PS_DepthOnly( const in RenderMaterialInterpolants vertex, const bool isFrontFace : SV_IsFrontFace )// : SV_Target
{
#if VA_RM_ALPHATEST
    MaterialInputs material = LoadMaterial( vertex );
    AlphaDiscard( material );
#endif    
    // // if transparent, depth test
    // return float4( 0.0, 1.0, 0.0, 1.0 );
}

// pixel shader outputs custom shadow value(s)
float4 PS_CustomShadow( const in RenderMaterialInterpolants vertex, const bool isFrontFace : SV_IsFrontFace ) : SV_Target
{
#if VA_RM_ALPHATEST
    MaterialInputs material = LoadMaterial( vertex );
    AlphaDiscard( material );
#endif    

    //if( g_Global.CustomShadowmapShaderType == 1 )
    //{
    //    const float3 viewDir        = normalize( g_Global.CameraWorldPosition.xyz - vertex.WorldspacePos.xyz ); // normalized vector from the pixel to the eye 
    //
    //    // offsets added in the vertex shader - look for ViewspaceDepthOffsetFlatAdd etc
    //    float viewspaceLength = length( viewDir );
    //
    //    // use sqrt of length for more nearby precision in R16_UNORM (not ideal but works better than linear)
    //    // note, g_Global.CameraNearFar.y is same as light->Range
    //    return float4( sqrt(viewspaceLength / g_Global.CameraNearFar.y), 0.0, 0.0, 0.0 );
    //}
    //else
    //{
        return float4( 0.0, 0.0, 0.0, 0.0 );
    //}
    // // if transparent, depth test
    // return float4( 0.0, 1.0, 0.0, 1.0 );
}

float4 PS_Forward( const in RenderMaterialInterpolants vertex, const bool isFrontFace : SV_IsFrontFace ) : SV_Target
{
#if defined( VA_INTEL_EXT_GRADIENT_FILTER_EXTENSION_ENABLED )
    IntelExt_Init( );
#endif

    MaterialInputs material = LoadMaterial( vertex );

#if VA_RM_ALPHATEST && !defined(VA_NO_ALPHA_TEST_IN_MAIN_DRAW)
    AlphaDiscard( material );
#endif
    // material.Normal = float3( 0, 0, 1 );
    ShadingParams shading = ComputeShadingParams( vertex, material, isFrontFace );

    // return float4( DisplayNormalSRGB( material.Normal ), 1.0 ); // looks ok
    // return float4( DisplayNormalSRGB( shading.TangentToWorld[2] ), 1.0 ); // looks ok
    // return float4( DisplayNormalSRGB( shading.Normal ), 1.0 ); // looks ok
    // return float4( DisplayNormalSRGB( shading.Reflected ), 1.0 ); // I suppose it could be ok
    // return float4( pow( abs( shading.NoV ), 2.2 ).xxx * 0.1, 1.0 ); // I suppose it could be ok as well
    // return float4( shading.Noise, 1.0 ); // looks ok
    // return float4( material.AmbientOcclusion.xxx, 1.0 ); // looks ok
    // return float4( shading.CombinedAmbientOcclusion.xxx, 1.0 ); // looks ok
    // return float4( shading.IBL.LocalToDistantK.xxx, 1 );

    PixelParams pixel = ComputePixelParams( shading, material );

    // return float4( pixel.DiffuseColor, 1.0 ); // looks ok
    // return float4( pixel.PerceptualRoughness.xxx, 1.0 ); // no idea if it's ok yet
    // return float4( pixel.F0, 1.0 ); // I guess it's cool?
    // return float4( pixel.Roughness.xxx, 1.0 ); // no idea if it's ok yet
    // return float4( pixel.DFG, 1.0 ); // no idea if it's ok yet
    // return float4( pixel.EnergyCompensation, 1.0 ); // no idea if it's ok yet

    float4 finalColor = EvaluateMaterialAndLighting( vertex, shading, pixel, material );

    // finalColor.xyz = shading.IBL.LocalToDistantK.xxx;

    finalColor.rgb = LightingApplyFog( vertex.WorldspacePos.xyz, finalColor.rgb );

    // this adds UI highlights and other similar stuff
    finalColor.rgb = lerp( finalColor.rgb, g_Instance.CustomColor.rgb, g_Instance.CustomColor.a );

    // gradient filter emulation: everything is here except the final output    
#if 0
    {
//#define USE_HALF_FLOAT_PRECISION
#if defined(USE_HALF_FLOAT_PRECISION)
        typedef min16float      lpfloat;
        typedef min16float2     lpfloat2;
        typedef min16float3     lpfloat3;
        typedef min16float4     lpfloat4;
#else
        typedef float           lpfloat;
        typedef float2          lpfloat2;
        typedef float3          lpfloat3;
        typedef float4          lpfloat4;
#endif
        const lpfloat2 shadingRate  = lpfloat2( ddx_fine( interpolants.Position.x ), ddy_fine( interpolants.Position.y ) );

        // Keep color in full precision float - everything else can go half with almost no difference.
        // It is possible to do color in fp16 as well but precision losses can potentially be noticable in extreme HDR range scenarios.
        float4 coarseColor  = finalColor;

        lpfloat4 ddxColor = (lpfloat4)ddx_fine( coarseColor );
        lpfloat4 ddyColor = (lpfloat4)ddy_fine( coarseColor );

        // Global filter 'aggressiveness' setting - values between 0 and 1 are acceptable; 1 means almost full extrapolation
        // with little clamping, 0 means no change allowed at all; 0.67 is a good balance!
        const lpfloat filterAggressiveness = 0.67; // 0..1

        // Color clamping - reduces erroneous influence from non-covered non-visible 'helper' pixels that went haywire
    #if 1
        lpfloat4 clampValueX = filterAggressiveness * ( lpfloat4(coarseColor) + sqrt(abs(ddxColor)) * lpfloat(0.1) );
        lpfloat4 clampValueY = filterAggressiveness * ( lpfloat4(coarseColor) + sqrt(abs(ddyColor)) * lpfloat(0.1) );
        ddxColor = clamp( ddxColor, -clampValueX, clampValueX );
        ddyColor = clamp( ddyColor, -clampValueY, clampValueY );
    #endif

        // differentiating between interpolation/extrapolation (precompute part)
        const lpfloat extrapolateRate     = filterAggressiveness;     // (set to 1.0 to always extrapolate at full rate - useful for debugging)
        const lpfloat extrapolateRateM1   = extrapolateRate - 1.0;

#if 1
        // Determine where in the 2x2 quad used for ddx/ddy we are, so we can decide on whether to apply extrapolation or interpolation rates later.
        // (Must be a nicer way to figure this out? in any case we need -8 for top/left, +8 for bottom/right, for the code in the loops below to work.)
        lpfloat2 quadID = (lpfloat2)fmod( interpolants.Position.xy, shadingRate.xy * lpfloat(2.0) ); // this gives {1, 3} or {2, 6}
        quadID = (quadID.xy - shadingRate.xy) * 8; // this gives -8, 8 for 2x2 and -16, 16 for 4x4 (doesn't matter as long as it's 8 or above)
#else
        // This is the proper way to do the above, however there's an erroneous warning that makes it not work:
        // "Gradient operations are not affected by wave-sensitive data or control flow."
        lpfloat2 quadID = ((uint2)interpolants.Position.xy-QuadReadLaneAt( (uint2)interpolants.Position.xy, 0 )) * 16.0.xx - 8.0.xx;
#endif

        // constants used for interpolation
        const lpfloat fineStepX     = lpfloat(1.0) / shadingRate.x;
        const lpfloat fineStepY     = lpfloat(1.0) / shadingRate.y;
        const lpfloat fineOffsetX   = lpfloat(0.5) - fineStepX / lpfloat(2.0);
        const lpfloat fineOffsetY   = lpfloat(0.5) - fineStepY / lpfloat(2.0);

        float4 sumColor = 0;
        
        const uint dbg2 	= 100*length(coarseColor);

        const lpfloat4 Ys           = lpfloat4( 0, 1, 2, 3 ) * fineStepY.xxxx - fineOffsetY.xxxx;
        const lpfloat4 interpsY     = Ys * (saturate(Ys*quadID.yyyy) * extrapolateRateM1.xxxx + lpfloat(1.0).xxxx);
        const lpfloat4 Xs           = lpfloat4( 0, 1, 2, 3 ) * fineStepX.xxxx - fineOffsetX.xxxx;
        const lpfloat4 interpsX     = Xs * (saturate(Xs*quadID.xxxx) * extrapolateRateM1.xxxx + lpfloat(1.0).xxxx);
        //lpfloat gradRatesX[4];

        for( uint ix = 0; ix < (float)shadingRate.x; ix ++ )
            for( uint iy = 0; iy < (float)shadingRate.y; iy ++ )
            {
                float4  fineColor = coarseColor + ddxColor * interpsX[ix] + ddyColor * interpsY[iy];
                fineColor = max( float4(0,0,0,0), fineColor );  // negative color & alpha values make no sense so prevent them!

                //////////////////////////////////////////////////////////////////////////////////////////////////////////////////|
                // all of this below is emulation: since we can't output fine pixels for now, just average all samples out
                sumColor += fineColor;
                uint2 finePixelOutputCoords = uint2( ix + interpolants.Position.x - shadingRate.x/2, iy + interpolants.Position.y - shadingRate.y/2 );
                // this code is here just for testing performance - it makes sure all the values are used and cannot be compiled out
                if( any(finePixelOutputCoords == uint2(999,dbg2)) )
                    sumColor.g += 10;
                //////////////////////////////////////////////////////////////////////////////////////////////////////////////////|
            }
		// since we can't output fine pixels for now, just average all samples out
        finalColor = sumColor / (shadingRate.x*shadingRate.y);
    }
#endif

    finalColor = HDRFramebufferClamp( finalColor );
    finalColor = min( finalColor, 50000.0.xxxx );

#if defined( VA_INTEL_EXT_GRADIENT_FILTER_EXTENSION_ENABLED )
    finalColor = min( finalColor, 32767.0.xxxx );       // avoids the rare inf output from GradFilter!
    return IntelExt_GradientAndBroadcastVec4( finalColor );
#endif

    return finalColor;
}
