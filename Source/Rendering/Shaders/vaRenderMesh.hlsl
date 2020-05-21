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

#include "vaMaterialShared.hlsl"

struct RenderMeshStandardVertexInput
{
    float4 Position             : SV_Position;
    float4 Color                : COLOR;
    float4 Normal               : NORMAL;
    float2 Texcoord0            : TEXCOORD0;
    float2 Texcoord1            : TEXCOORD1;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

RenderMaterialInterpolants VS_Standard( const in RenderMeshStandardVertexInput input )
{
    RenderMaterialInterpolants ret;

    //ret.Color                   = input.Color;
    ret.Texcoord01          = float4( input.Texcoord0, input.Texcoord1 );
    // ret.Texcoord23          = float4( 0, 0, 0, 0 );

    ret.WorldspacePos        = mul( g_Instance.World, float4( input.Position.xyz, 1) );
    ret.WorldspaceNormal.xyz = normalize( mul( (float3x3)g_Instance.NormalWorld, input.Normal.xyz ).xyz );

#if 0 // TODO: maybe upgrade this, see Real-Time Rendering (Fourth Edition), pg 237/238
    {
        float3 cameraToVertexDir = ret.WorldspacePos.xyz - g_Global.CameraWorldPosition.xyz - g_Global.GlobalWorldBase.xyz;
        float viewspaceLength = length( cameraToVertexDir );
        cameraToVertexDir /= viewspaceLength;

        float slope = saturate( 1 - abs( dot( cameraToVertexDir, ret.WorldspaceNormal.xyz ) ) );

        float amount = lerp( viewspaceLength * g_Global.ViewspaceDepthOffsetFlatRel + g_Global.ViewspaceDepthOffsetFlatAbs,
                                viewspaceLength * g_Global.ViewspaceDepthOffsetSlopeRel + g_Global.ViewspaceDepthOffsetSlopeAbs, 
                                slope );

        ret.WorldspacePos.xyz   += cameraToVertexDir * amount;

        // float viewspaceLength = length( ret.ViewspacePos.xyz );
        // float slope = saturate( 1 - abs( dot( ret.ViewspacePos.xyz / viewspaceLength, ret.ViewspaceNormal.xyz ) ) );
        // float scale = lerp( viewspaceLength * g_Global.ViewspaceDepthOffsetFlatMul + g_Global.ViewspaceDepthOffsetFlatAdd,
        //                         viewspaceLength * g_Global.ViewspaceDepthOffsetSlopeMul + g_Global.ViewspaceDepthOffsetSlopeAdd, 
        //                         slope ) / viewspaceLength;
        // ret.ViewspacePos.xyz *= scale;
    }
#endif 

    ret.Position            = mul( g_Global.ViewProj, float4( ret.WorldspacePos.xyz, 1.0 ) );

    // do all the subsequent shading math with the GlobalWorldBase for precision purposes
    ret.WorldspacePos.xyz -= g_Global.GlobalWorldBase.xyz;

//    // this is unused but hey, copy it anyway
//    ret.ViewspaceNormal.w   = input.Normal.w;

    // hijack this for highlighting and similar stuff
    ret.Color               = input.Color;

    // // For normals that are facing back (or close to facing back, slightly skew them back towards the eye to 
    // // reduce (mostly specular) aliasing artifacts on edges; do it separately for potential forward or back
    // // looking vectors because facing is determinted later, and the correct one is picked in the shader.
    // float3 viewDir          = normalize( ret.ViewspacePos.xyz );
    // float normalAwayFront   = saturate( ( dot( viewDir,  ret.ViewspaceNormal.xyz ) + 0.08 ) * 1.0 );
    // float normalAwayBack    = saturate( ( dot( viewDir, -ret.ViewspaceNormal.xyz ) + 0.08 ) * 1.0 );

    ret.WorldspaceNormal.w   = 0.0; // normalAwayFront;
    ret.WorldspacePos.w      = 0.0; // normalAwayBack;

    return ret;
}



//#ifdef ENABLE_PASSTHROUGH_GS

// // Per-vertex data passed to the rasterizer.
// struct GeometryShaderOutput
// {
//     min16float4 pos     : SV_POSITION;
//     min16float3 color   : COLOR0;
//     uint        rtvId   : SV_RenderTargetArrayIndex;
// };


// This geometry shader is a pass-through that leaves the geometry unmodified 
// and sets the render target array index.
[maxvertexcount(3)]
void GS_Standard(triangle RenderMaterialInterpolants input[3], inout TriangleStream<RenderMaterialInterpolants> outStream)
{
    RenderMaterialInterpolants output;
    [unroll(3)]
    for (int i = 0; i < 3; ++i)
    {
        output.Position         = input[i].Position;
        output.Color            = input[i].Color;
        output.WorldspacePos    = input[i].WorldspacePos;
        output.WorldspaceNormal = input[i].WorldspaceNormal;
        output.Texcoord01       = input[i].Texcoord01;
        //output.Texcoord23       = input[i].Texcoord23;

        // output.Texcoord23       = 0;
        // output.Texcoord23[i]    = 1.0;
        
        // 
        // output.WorldspacePos += output.WorldspaceNormal * 0.1;
        // output.Position         = mul( g_Global.Proj, float4( output.WorldspacePos.xyz, 1.0 ) );

        outStream.Append(output);
    }
}

//#endif