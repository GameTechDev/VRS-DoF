///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017, Intel Corporation
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



#ifndef VA_MATERIALSHARED_SH_INCLUDED
#define VA_MATERIALSHARED_SH_INCLUDED

#include "vaShared.hlsl"

struct RenderMaterialInterpolants
{
    float4 Position            : SV_Position;
    float4 Color               : COLOR;
    float4 WorldspacePos       : TEXCOORD0;         // this is offset with -g_Global.GlobalWorldBase
    float4 WorldspaceNormal    : NORMAL0;
    float4 Texcoord01          : TEXCOORD1;
//  float4 Texcoord23          : TEXCOORD2;         // currently commented out - texcoords 2 and 3 are probably never going to be needed but just in case (maybe lightmaps + object space shading or something?)
};


//cbuffer B2 : register(b2)
//{
//   MaterialConstants    g_material;
//};

Texture2D         g_DFGLookupTable        : register( T_CONCATENATER( SHADERGLOBAL_MATERIAL_DFG_LOOKUPTABLE_V ) );

// this will declare textures for the current material
#ifdef VA_RM_TEXTURE_DECLARATIONS
VA_RM_TEXTURE_DECLARATIONS;
#else
#error Expected VA_RM_TEXTURE_DECLARATIONS to be defined
#endif

// This is what all the vaRenderMaterial::m_new_inputSlots get converted into...
struct RenderMaterialInputs
{
#ifdef VA_RM_INPUTS_DECLARATIONS
    VA_RM_INPUTS_DECLARATIONS
#else
#error Expected VA_RM_INPUTS_DECLARATIONS to be defined
#endif
};

RenderMaterialInputs LoadRenderMaterialInputs( in RenderMaterialInterpolants vertex )
{
    RenderMaterialInputs inputs;

#ifdef VA_RM_NODES_DECLARATIONS
    VA_RM_NODES_DECLARATIONS;
#else
#error Expected VA_RM_NODES_DECLARATIONS to be defined
#endif

#ifdef VA_RM_INPUTS_LOADING
    VA_RM_INPUTS_LOADING;
#else
#error Expected VA_RM_INPUTS_LOADING to be defined
#endif

    return inputs;
}

// predefined input macros:
//
// VA_RM_HAS_INPUT_#name                            : defined and 1 if material input slot with the 'name' exists



#endif // VA_MATERIALSHARED_SH_INCLUDED
