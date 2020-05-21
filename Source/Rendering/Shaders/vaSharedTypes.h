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

#ifndef VA_SHADER_SHARED_TYPES_HLSL
#define VA_SHADER_SHARED_TYPES_HLSL

#include "vaShaderCore.h"

#include "vaSharedTypes_PostProcess.h"

#ifndef VA_COMPILED_AS_SHADER_CODE
namespace Vanilla
{
#endif

#define SHADERGLOBAL_DEBUG_FLOAT_OUTPUT_COUNT               4096

// for vaShaderItemGlobals (for stuff that is set less frequently than vaGraphicsItem/vaComputeItem - mostly used for vaSceneDrawContext-based subsystems)
#define SHADERGLOBAL_SRV_SLOT_BASE                          32
#define SHADERGLOBAL_SRV_SLOT_COUNT                         16
#define SHADERGLOBAL_CBV_SLOT_BASE                           8
#define SHADERGLOBAL_CBV_SLOT_COUNT                          6              // because D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT is 14... 
#define SHADERGLOBAL_UAV_SLOT_BASE                           8
#define SHADERGLOBAL_UAV_SLOT_COUNT                          4
// #define SHADERGLOBAL_SAMPLER_SLOT_BASE                       4          // not actually used yet!

//////////////////////////////////////////////////////////////////////////
// PREDEFINED GLOBAL SAMPLER SLOTS
#define SHADERGLOBAL_SHADOWCMP_SAMPLERSLOT                   9
#define SHADERGLOBAL_POINTCLAMP_SAMPLERSLOT                 10
#define SHADERGLOBAL_POINTWRAP_SAMPLERSLOT                  11
#define SHADERGLOBAL_LINEARCLAMP_SAMPLERSLOT                12
#define SHADERGLOBAL_LINEARWRAP_SAMPLERSLOT                 13
#define SHADERGLOBAL_ANISOTROPICCLAMP_SAMPLERSLOT           14
#define SHADERGLOBAL_ANISOTROPICWRAP_SAMPLERSLOT            15
//////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////
// PREDEFINED CONSTANTS BUFFER SLOTS
#define POSTPROCESS_CONSTANTSBUFFERSLOT                     1
#define RENDERMESHMATERIAL_CONSTANTSBUFFERSLOT              1
#define SHADERINSTANCE_CONSTANTSBUFFERSLOT                  2
#define SKYBOX_CONSTANTSBUFFERSLOT                          4
#define ZOOMTOOL_CONSTANTSBUFFERSLOT                        4
#define CDLOD2_CONSTANTS_BUFFERSLOT                         3
//
// global/system constant slots go from EXTENDED_SRV_CBV_UAV_SLOT_BASE to EXTENDED_SRV_CBV_UAV_SLOT_BASE+...16?
#define SHADERGLOBAL_CONSTANTSBUFFERSLOT                    (SHADERGLOBAL_CBV_SLOT_BASE + 0)
#define LIGHTINGGLOBAL_CONSTANTSBUFFERSLOT                  (SHADERGLOBAL_CBV_SLOT_BASE + 1)
// #define LIGHTINGGLOBAL_DISTANTIBL_CONSTANTSBUFFERSLOT      (SHADERGLOBAL_CBV_SLOT_BASE + 2)
//
// need to do this so X_CONCATENATER-s work
#define SHADERGLOBAL_CONSTANTSBUFFERSLOT_V                  8
#define LIGHTINGGLOBAL_CONSTANTSBUFFERSLOT_V                9
//#define LIGHTINGGLOBAL_DISTANTIBL_CONSTANTSBUFFERSLOT_V    10
#if (SHADERGLOBAL_CONSTANTSBUFFERSLOT_V != SHADERGLOBAL_CONSTANTSBUFFERSLOT)                                            \
    || (LIGHTINGGLOBAL_CONSTANTSBUFFERSLOT_V != LIGHTINGGLOBAL_CONSTANTSBUFFERSLOT)                                     \
//    || (LIGHTINGGLOBAL_DISTANTIBL_CONSTANTSBUFFERSLOT_V != LIGHTINGGLOBAL_DISTANTIBL_CONSTANTSBUFFERSLOT)                                                     
    #error _V values above not in sync, just fix them up please
#endif

// PREDEFINED CONSTANTS BUFFER SLOTS
//////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
// PREDEFINED SHADER RESOURCE VIEW SLOTS
//
// 10 should be enough for materials, right, right?
#define RENDERMESH_TEXTURE_SLOT_MIN                         0
#define RENDERMESH_TEXTURE_SLOT_MAX                         9
//
#define CDLOD2_TEXTURE_SLOT0                                10
#define CDLOD2_TEXTURE_SLOT1                                11
#define CDLOD2_TEXTURE_SLOT2                                12
#define CDLOD2_TEXTURE_OVERLAYMAP_0                         13
//
#define SIMPLE_PARTICLES_VIEWSPACE_DEPTH                    10
//
//////////////////////////////////////////////////////////////////////////

// global texture slots go from EXTENDED_SRV_CBV_UAV_SLOT_BASE to EXTENDED_SRV_CBV_UAV_SLOT_BASE+ ... 8?
#define SHADERGLOBAL_MATERIAL_DFG_LOOKUPTABLE               (SHADERGLOBAL_SRV_SLOT_BASE + 0)
//#define SHADERGLOBAL_LIGHTING_ENVMAP_TEXTURESLOT            (SHADERGLOBAL_SRV_SLOT_BASE + 2)        // <- this is legacy environment map, will go out in the future
#define SHADERGLOBAL_LIGHTING_CUBE_SHADOW_TEXTURESLOT       (SHADERGLOBAL_SRV_SLOT_BASE + 3)
#define LIGHTINGGLOBAL_LOCALIBL_REFROUGHMAP_TEXTURESLOT     (SHADERGLOBAL_SRV_SLOT_BASE + 4)        // <- this is 'modern' reflection map
#define LIGHTINGGLOBAL_LOCALIBL_IRRADIANCEMAP_TEXTURESLOT   (SHADERGLOBAL_SRV_SLOT_BASE + 5)        // <- this is 'modern' irradiance map (using either this or SH)
#define LIGHTINGGLOBAL_DISTANTIBL_REFROUGHMAP_TEXTURESLOT   (SHADERGLOBAL_SRV_SLOT_BASE + 6)        // <- this is 'modern' reflection map
#define LIGHTINGGLOBAL_DISTANTIBL_IRRADIANCEMAP_TEXTURESLOT (SHADERGLOBAL_SRV_SLOT_BASE + 7)        // <- this is 'modern' irradiance map (using either this or SH)
#define SHADERGLOBAL_AOMAP_TEXTURESLOT                      (SHADERGLOBAL_SRV_SLOT_BASE + 8)        // <- this is the SSAO (for now)

// this is so annoying but I don't know how to resolve it - in order to be used in 'T_CONCATENATER', it has to be a number string token or something like that so "(base+n)" doesn't work
#define SHADERGLOBAL_MATERIAL_DFG_LOOKUPTABLE_V                 32
//#define SHADERGLOBAL_LIGHTING_ENVMAP_TEXTURESLOT_V              34
#define SHADERGLOBAL_LIGHTING_CUBE_SHADOW_TEXTURESLOT_V         35
#define LIGHTINGGLOBAL_LOCALIBL_REFROUGHMAP_TEXTURESLOT_V       36
#define LIGHTINGGLOBAL_LOCALIBL_IRRADIANCEMAP_TEXTURESLOT_V     37
#define LIGHTINGGLOBAL_DISTANTIBL_REFROUGHMAP_TEXTURESLOT_V     38
#define LIGHTINGGLOBAL_DISTANTIBL_IRRADIANCEMAP_TEXTURESLOT_V   39
#define SHADERGLOBAL_AOMAP_TEXTURESLOT_V                        40
#if (   SHADERGLOBAL_MATERIAL_DFG_LOOKUPTABLE               != SHADERGLOBAL_MATERIAL_DFG_LOOKUPTABLE_V                  \
    ||  SHADERGLOBAL_LIGHTING_CUBE_SHADOW_TEXTURESLOT       != SHADERGLOBAL_LIGHTING_CUBE_SHADOW_TEXTURESLOT_V          \
    ||  LIGHTINGGLOBAL_LOCALIBL_REFROUGHMAP_TEXTURESLOT     != LIGHTINGGLOBAL_LOCALIBL_REFROUGHMAP_TEXTURESLOT_V        \
    ||  LIGHTINGGLOBAL_LOCALIBL_IRRADIANCEMAP_TEXTURESLOT   != LIGHTINGGLOBAL_LOCALIBL_IRRADIANCEMAP_TEXTURESLOT_V      \
    ||  LIGHTINGGLOBAL_DISTANTIBL_REFROUGHMAP_TEXTURESLOT   != LIGHTINGGLOBAL_DISTANTIBL_REFROUGHMAP_TEXTURESLOT_V      \
    ||  LIGHTINGGLOBAL_DISTANTIBL_IRRADIANCEMAP_TEXTURESLOT != LIGHTINGGLOBAL_DISTANTIBL_IRRADIANCEMAP_TEXTURESLOT_V    \
    ||  SHADERGLOBAL_AOMAP_TEXTURESLOT                      != SHADERGLOBAL_AOMAP_TEXTURESLOT_V    \
        )
    #error _V values above not in sync, just fix them up please
#endif


struct ShaderGlobalConstants
{
    vaMatrix4x4             View;
    vaMatrix4x4             ViewInv;
    vaMatrix4x4             Proj;
    vaMatrix4x4             ProjInv;
    vaMatrix4x4             ViewProj;
    vaMatrix4x4             ViewProjInv;

    vaVector4               GlobalWorldBase;        // global world position offset for shading; used to make all shading computation close(r) to (0,0,0) for precision purposes
    vaVector4               CameraDirection;
    vaVector4               CameraWorldPosition;    // drawContext.Camera.GetPosition() - drawContext.GlobalWorldBase
    vaVector4               CameraSubpixelOffset;   // .xy contains value of subpixel offset used for supersampling/TAA (otheriwse knows as jitter) or (0,0) if no jitter enabled; .zw are 0

    vaVector2               ViewportSize;           // ViewportSize.x, ViewportSize.y
    vaVector2               ViewportPixelSize;      // 1.0 / ViewportSize.x, 1.0 / ViewportSize.y
    vaVector2               ViewportHalfSize;       // ViewportSize.x * 0.5, ViewportSize.y * 0.5
    vaVector2               ViewportPixel2xSize;    // 2.0 / ViewportSize.x, 2.0 / ViewportSize.y

    vaVector2               DepthUnpackConsts;
    vaVector2               CameraTanHalfFOV;
    vaVector2               CameraNearFar;
    vaVector2               CursorScreenPosition;

    vaVector2               GlobalPixelScale;
    float                   GlobalMIPOffset;
    float                   GlobalSpecialEmissiveScale;

    float                   TransparencyPass;               // TODO: remove
    float                   WireframePass;                  // 1 if vaDrawContextFlags::DebugWireframePass set, otherwise 0
    float                   EV100;                          // see https://google.github.io/filament/Filament.html#lighting/directlighting/pre-exposedlights
    float                   PreExposureMultiplier;          // most of the lighting is already already pre-exposed on the CPU side, but not all


    float                   TimeFract;              // fractional part of total time from starting the app
    float                   TimeFmod3600;           // remainder of total time from starting the app divided by an hour (3600 seconds) - leaves you with at least 2 fractional part decimals of precision
    float                   SinTime2Pi;             // sine of total time from starting the app times 2*PI
    float                   SinTime1Pi;             // sine of total time from starting the app times PI

    // // push (or pull) vertices by this amount away (towards) camera; to avoid z fighting for debug wireframe or for the shadow drawing offset
    // // 'absolute' is in world space; 'relative' means 'scale by distance to camera'
    // float                   ViewspaceDepthOffsetFlatAbs;
    // float                   ViewspaceDepthOffsetFlatRel;
    // float                   ViewspaceDepthOffsetSlopeAbs;
    // float                   ViewspaceDepthOffsetSlopeRel;
};

struct SimpleSkyConstants
{
    vaMatrix4x4          ProjToWorld;

    vaVector4            SunDir;

    vaVector4            SkyColorLow;
    vaVector4            SkyColorHigh;

    vaVector4            SunColorPrimary;
    vaVector4            SunColorSecondary;

    float                SkyColorLowPow;
    float                SkyColorLowMul;

    float                SunColorPrimaryPow;
    float                SunColorPrimaryMul;
    float                SunColorSecondaryPow;
    float                SunColorSecondaryMul;

    float               Dummy0;
    float               Dummy1;
};

struct ShaderInstanceConstants
{
    vaMatrix4x4         World;
    //vaMatrix4x4         ShadowWorldViewProj;
    
    // since we now support non-uniform scale, we need the 'normal matrix' to keep normals correct 
    // (for more info see : https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/geometry/transforming-normals or http://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/ )
    vaMatrix4x4         NormalWorld;

    vaVector4           CustomColor;          // used for highlights, wireframe, etc - finalColor.rgb = lerp( finalColor.rgb, g_Instance.CustomColor.rgb, g_Instance.CustomColor.a )
};

// struct GBufferConstants
// {
//     float               Dummy0;
//     float               Dummy1;
//     float               Dummy2;
//     float               Dummy3;
// };

struct ZoomToolShaderConstants
{
    vaVector4           SourceRectangle;

    int                 ZoomFactor;
    float               Dummy1;
    float               Dummy2;
    float               Dummy3;
};


#ifndef VA_COMPILED_AS_SHADER_CODE
} // namespace Vanilla
#endif

#ifdef VA_COMPILED_AS_SHADER_CODE


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Global buffers
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

cbuffer GlobalConstantsBuffer                       : register( B_CONCATENATER( SHADERGLOBAL_CONSTANTSBUFFERSLOT_V ) )
{
    ShaderGlobalConstants                   g_Global;
}


//cbuffer ShaderSimpleShadowsGlobalConstantsBuffer    : register( B_CONCATENATER( SHADERSIMPLESHADOWSGLOBAL_CONSTANTSBUFFERSLOT ) )
//{
//    ShaderSimpleShadowsGlobalConstants      g_SimpleShadowsGlobal;
//}

// cbuffer RenderMeshMaterialConstantsBuffer : register( B_CONCATENATER( RENDERMESHMATERIAL_CONSTANTSBUFFERSLOT ) )
// {
//     RenderMeshMaterialConstants             g_RenderMeshMaterialGlobal;
// }

cbuffer ShaderInstanceConstantsBuffer                   : register( B_CONCATENATER( SHADERINSTANCE_CONSTANTSBUFFERSLOT ) )
{
    ShaderInstanceConstants                 g_Instance;
}

// cbuffer GBufferConstantsBuffer                      : register( B_CONCATENATER( GBUFFER_CONSTANTSBUFFERSLOT ) )
// {
//     GBufferConstants                        g_GBufferConstants;
// }



#endif

#endif // VA_SHADER_SHARED_TYPES_HLSL