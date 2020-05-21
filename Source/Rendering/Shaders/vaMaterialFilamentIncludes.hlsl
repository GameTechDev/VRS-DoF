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
// Based on:    https://github.com/google/filament, see vaMaterialFilament.hlsl for more info
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// GLSL <-> HLSL conversion helpers (this link was helpful: https://dench.flatlib.jp/opengl/glsl_hlsl)

// #if defined(TARGET_MOBILE)
// #define HIGHP highp
// #define MEDIUMP mediump
// #else
#define HIGHP
#define MEDIUMP
// #endif

#define highp
#define mediump

//#if !defined(TARGET_MOBILE) || defined(CODEGEN_TARGET_VULKAN_ENVIRONMENT)
//#define LAYOUT_LOCATION(x) layout(location = x)
//#else
#define LAYOUT_LOCATION(x)
//#endif

#define dFdx    ddx
#define dFdy    ddy

#define vec2    float2
#define vec3    float3
#define vec4    float4
#define mat3    float3x3
#define mat4    float4x4
#define mix     lerp

#define linear  _linear

#define inversesqrt rsqrt
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// includes of filament or ported filament headers!
//
#include "Filament\ambient_occlusion.va.fs"
#include "Filament\common_math.fs"
#include "Filament\brdf.va.fs"
#include "Filament\common_getters.va.fs"
#include "Filament\common_graphics.va.fs"
#include "Filament\common_lighting.va.fs"
#include "Filament\common_shading.va.fs"
#include "Filament\common_material.fs"
#include "Filament\conversion_functions.va.fs"
// "Filament\depth_main.fs"                     - handled in PS_DepthOnly
// "Filament\dithering.fs"                      - not needed ATM
// "Filament\fxaa.fs"                           - not needed ATM
// "Filament\inputs.fs"                         - using Vanilla one (RenderMaterialInterpolants)
#include "Filament\material_inputs.va.fs"

#if defined(SHADING_MODEL_CLOTH)
#include "Filament\shading_model_cloth.va.fs"
#elif defined(SHADING_MODEL_SUBSURFACE)
#include "Filament\shading_model_subsurface.va.fs"
#else
#include "Filament\shading_model_standard.va.fs"
#endif

#include "Filament\light_directional.va.fs"
#include "Filament\light_indirect.va.fs"        // stubbed out for now
#include "Filament\light_punctual.va.fs"
#include "Filament\shading_parameters.va.fs"
// #include "Filament\getters.va.fs"			// not meaningful to port this - too many differences
// "Filament\main.fs"                           - entry points are in vaMaterialFilament.hlsl
// "Filament\post_process.fs"                   - not used here
#include "Filament\shading_lit.va.fs"
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

