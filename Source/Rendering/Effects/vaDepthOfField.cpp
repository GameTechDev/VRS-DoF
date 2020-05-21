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
// Author(s):  Trapper Mcferron (trapper.mcferron@intel.com)
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "vaDepthOfField.h"

#include "Rendering/vaRenderDeviceContext.h"

#include "IntegratedExternals/vaImguiIntegration.h"

using namespace Vanilla;

vaDepthOfField::vaDepthOfField( const vaRenderingModuleParams & params ) : 
    vaRenderingModule( params ), 
    m_constantsBuffer( params ),
    m_CSResolve( params ),
    m_CSSplitPlanes( params ),
    m_CSFarBlur {params,params,params},
    m_CSNearBlur {params,params,params},
    vaUIPanel( "DepthOfField", -1, true, vaUIPanel::DockLocation::DockedLeftBottom )
{ 
//    assert( vaRenderingCore::IsInitialized() );

    m_CSSplitPlanes->CreateShaderFromFile( L"vaDepthOfField.hlsl", "cs_5_0", "CSSplitPlanes", { {"DOF_SPLIT_PLANES","1"} }, false );
    m_CSResolve->CreateShaderFromFile( L"vaDepthOfField.hlsl", "cs_5_0", "CSResolve", { {"DOF_RESOLVE","1"} } , false );

    for( int i = 0; i < 3; i++ )
    {
        std::pair<string, string> blurTypeMacro = { "DOF_BLUR_TYPE", std::to_string(i) };
        m_CSFarBlur[i]->CreateShaderFromFile( L"vaDepthOfField.hlsl", "cs_5_0", "CSFarBlur", { blurTypeMacro, {"DOF_FAR_BLUR", "1"} }, false );
        m_CSNearBlur[i]->CreateShaderFromFile( L"vaDepthOfField.hlsl", "cs_5_0", "CSNearBlur", { blurTypeMacro, {"DOF_NEAR_BLUR", "1"} }, false );
    }
}

vaDepthOfField::~vaDepthOfField( )
{
}

// sceneContext needed for NDCToViewDepth to work - could be split out and made part of the constant buffer here
vaDrawResultFlags vaDepthOfField::Draw( vaSceneDrawContext & sceneContext, const shared_ptr<vaTexture> & inDepth, const shared_ptr<vaTexture> & inOutColor, const shared_ptr<vaTexture> & outColorNoSRGB )
{
    vaDrawResultFlags drawResults = vaDrawResultFlags::None;
    vaRenderDeviceContext & renderContext = sceneContext.RenderDeviceContext;

    VA_TRACE_CPUGPU_SCOPE( DepthOfField, renderContext );

    assert( inDepth->GetSize() == inOutColor->GetSize() );

    vaVector3i offscreenSize = { (inOutColor->GetSize().x + 1) / 2, (inOutColor->GetSize().y + 1) / 2, (inOutColor->GetSize().z + 1) / 2 };
    if( m_offscreenColorNearA == nullptr || m_offscreenColorNearA->GetSize() != offscreenSize )
    {
        // near hopefully needs less precision so i'm attempting to hold everything in 8 bit per channel color
        m_offscreenColorNearA   = vaTexture::Create2D( GetRenderDevice(), vaResourceFormat::R16G16B16A16_FLOAT, offscreenSize.x, offscreenSize.y, 1, 1, 1, vaResourceBindSupportFlags::ShaderResource | vaResourceBindSupportFlags::UnorderedAccess );
        m_offscreenColorNearB   = vaTexture::Create2D( GetRenderDevice(), vaResourceFormat::R16G16B16A16_FLOAT, offscreenSize.x, offscreenSize.y, 1, 1, 1, vaResourceBindSupportFlags::ShaderResource | vaResourceBindSupportFlags::UnorderedAccess );

        // offscreen color far A needs at least 8 bits of alpha to hold the coc blend
        // offscreen color far b just needs a single bit to maintain blurred/no-blurred
        m_offscreenColorFarA    = vaTexture::Create2D( GetRenderDevice(), vaResourceFormat::R16G16B16A16_FLOAT, offscreenSize.x, offscreenSize.y, 1, 1, 1, vaResourceBindSupportFlags::ShaderResource | vaResourceBindSupportFlags::UnorderedAccess );
        m_offscreenColorFarB    = vaTexture::Create2D( GetRenderDevice(), vaResourceFormat::R16G16B16A16_FLOAT, offscreenSize.x, offscreenSize.y, 1, 1, 1, vaResourceBindSupportFlags::ShaderResource | vaResourceBindSupportFlags::UnorderedAccess );

        m_offscreenCoc          = vaTexture::Create2D( GetRenderDevice(), vaResourceFormat::R8_UNORM, inOutColor->GetSize().x, inOutColor->GetSize().y, 1, 1, 1, vaResourceBindSupportFlags::ShaderResource | vaResourceBindSupportFlags::UnorderedAccess );
    }

    float kernelScale = (float)( ( sceneContext.Camera.GetYFOVMain( ) ) ? ( inDepth->GetSizeY( ) / 1080.0f ) : ( inDepth->GetSizeX( ) / 1920.0f ) );

    UpdateConstants( renderContext, kernelScale );

    // backup & remove outputs just in case colorInOut is in them
    vaRenderDeviceContext::RenderOutputsState rtState = renderContext.GetOutputs( );
    renderContext.SetRenderTarget( nullptr, nullptr, false );

    renderContext.BeginItems( vaRenderTypeFlags::Compute, &sceneContext );

    // split near / far planes and calculate coc
    {
        VA_TRACE_CPUGPU_SCOPE( DoF_SplitPlanes, renderContext );

        vaComputeItem computeItem;

        computeItem.ConstantBuffers[DOF_CB] = m_constantsBuffer;
        computeItem.ShaderResourceViews[DOF_SPLIT_PLANES_SRV_DEPTH] = inDepth;
        computeItem.ShaderResourceViews[DOF_SPLIT_PLANES_SRV_COLOR] = inOutColor;
        computeItem.UnorderedAccessViews[DOF_SPLIT_PLANES_UAV_NEAR] = m_offscreenColorNearA;
        computeItem.UnorderedAccessViews[DOF_SPLIT_PLANES_UAV_FAR] = m_offscreenColorFarA;
        computeItem.UnorderedAccessViews[DOF_SPLIT_PLANES_UAV_COC] = m_offscreenCoc;
        computeItem.ComputeShader = m_CSSplitPlanes;
        int threadGroupCountX = ( m_offscreenColorNearA->GetSizeX( ) + 16 - 1 ) / 16;
        int threadGroupCountY = ( m_offscreenColorNearA->GetSizeY( ) + 16 - 1 ) / 16;
        computeItem.SetDispatch( threadGroupCountX, threadGroupCountY, 1 );
        drawResults |= renderContext.ExecuteItem( computeItem );
    }

    {
        vaComputeItem computeItem;
        computeItem.ConstantBuffers[DOF_CB] = m_constantsBuffer;
        int threadGroupCountX = ( m_offscreenColorFarA->GetSizeX( ) + 8 - 1 ) / 8;
        int threadGroupCountY = ( m_offscreenColorFarA->GetSizeY( ) + 8 - 1 ) / 8;
        computeItem.SetDispatch( threadGroupCountX, threadGroupCountY, 1 );

        // blur far plane, near plane
        {
            VA_TRACE_CPUGPU_SCOPE( DoF_FarBlur, renderContext );

            for( int i = 0; i < 3; i++ )
            {
                computeItem.ShaderResourceViews[DOF_FAR_BLUR_SRV_COLOR] = (i & 0x1) == 0 ? m_offscreenColorFarA : m_offscreenColorFarB;
                computeItem.ShaderResourceViews[DOF_FAR_BLUR_SRV_COC] = m_offscreenCoc;
                computeItem.UnorderedAccessViews[DOF_FAR_BLUR_UAV_COLOR] = (i & 0x1) == 0 ? m_offscreenColorFarB : m_offscreenColorFarA;
                computeItem.ComputeShader = m_CSFarBlur[i];
                drawResults |= renderContext.ExecuteItem( computeItem );
            }

#if 1 // one more pass to make it smoother
            for( int i = 1; i < 3; i++ )
            {
                computeItem.ShaderResourceViews[DOF_FAR_BLUR_SRV_COLOR] = ( i & 0x1 ) == 0 ? m_offscreenColorFarA : m_offscreenColorFarB;
                computeItem.ShaderResourceViews[DOF_FAR_BLUR_SRV_COC] = m_offscreenCoc;
                computeItem.UnorderedAccessViews[DOF_FAR_BLUR_UAV_COLOR] = ( i & 0x1 ) == 0 ? m_offscreenColorFarB : m_offscreenColorFarA;
                computeItem.ComputeShader = m_CSFarBlur[i];
                drawResults |= renderContext.ExecuteItem( computeItem );
            }
#endif
        }

        {
            VA_TRACE_CPUGPU_SCOPE( DoF_NearBlur, renderContext );
            for( int i = 0; i < 3; i++ )
            {
                computeItem.ConstantBuffers[DOF_CB] = m_constantsBuffer;
                computeItem.ShaderResourceViews[DOF_NEAR_BLUR_SRV_COLOR] = (i & 0x1) == 0 ? m_offscreenColorNearA : m_offscreenColorNearB;
                computeItem.UnorderedAccessViews[DOF_NEAR_BLUR_UAV_COLOR] = (i & 0x1) == 0 ? m_offscreenColorNearB : m_offscreenColorNearA;
                computeItem.ComputeShader = m_CSNearBlur[i];
                drawResults |= renderContext.ExecuteItem( computeItem );
            }
        }
    }

    // resolve to final output
    {
        VA_TRACE_CPUGPU_SCOPE( DoF_Resolve, renderContext );

        vaComputeItem computeItem;

        computeItem.ConstantBuffers[DOF_CB] = m_constantsBuffer;
        computeItem.ShaderResourceViews[DOF_RESOLVE_SRV_COC] = m_offscreenCoc;
        computeItem.ShaderResourceViews[DOF_RESOLVE_SRV_FAR] = m_offscreenColorFarB;
        computeItem.ShaderResourceViews[DOF_RESOLVE_SRV_NEAR] = m_offscreenColorNearB;
        computeItem.UnorderedAccessViews[DOF_RESOLVE_UAV_OUT] = outColorNoSRGB;
        computeItem.ComputeShader = m_CSResolve;
        int threadGroupCountX = ( inOutColor->GetSizeX( ) + 16 - 1 ) / 16;
        int threadGroupCountY = ( inOutColor->GetSizeY( ) + 16 - 1 ) / 16;
        computeItem.SetDispatch( threadGroupCountX, threadGroupCountY, 1 );
        drawResults |= renderContext.ExecuteItem( computeItem );
    }

    renderContext.EndItems();

    // restore previous RTs
    renderContext.SetOutputs( rtState );

    return drawResults;
}

void vaDepthOfField::UIPanelTick( vaApplicationBase & /*application*/ )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::PushItemWidth( 120.0f );

    ImGui::InputFloat( "InFocusFrom",           &m_settings.InFocusFrom, 1 );
    ImGui::InputFloat( "InFocusTo",             &m_settings.InFocusTo, 1 );
    ImGui::InputFloat( "Near Transition Range", &m_settings.NearTransitionRange, 0.25f );
    ImGui::InputFloat( "Far Transition Range",  &m_settings.FarTransitionRange, .25f );
    ImGui::InputFloat( "Near Blur Size",        &m_settings.NearBlurSize, 1.0f );
    ImGui::InputFloat( "Far Blur Size",         &m_settings.FarBlurSize, 1.0f );
    // ImGui::InputFloat( "VRS 2X2 Distance", &m_settings.Vrs2x2Distance, 0.25f );
    // ImGui::InputFloat( "VRS 4X4 Distance", &m_settings.Vrs4x4Distance, 0.25f );

    ImGui::PopItemWidth();
#endif
}

void vaDepthOfField::UpdateConstants( vaRenderDeviceContext & renderContext, float kernelScale )
{
    DepthOfFieldShaderConstants consts;

    consts.focalStart   = m_settings.InFocusFrom;
    consts.focalEnd     = m_settings.InFocusTo;
    consts.nearKernel   = m_settings.NearBlurSize * kernelScale;
    consts.farKernel    = m_settings.FarBlurSize * kernelScale;
    consts.nearBlend    = m_settings.NearTransitionRange;
    consts.cocRamp      = m_settings.FarTransitionRange;

    m_constantsBuffer.Update( renderContext, consts );
}

float vaDepthOfField::ComputeConservativeBlurFactor( const vaCameraBase & camera, const vaOrientedBoundingBox & obbWorldSpace )
{
    vaPlane cameraPlane = vaPlane::FromPointNormal( camera.GetPosition(), camera.GetDirection() );
    float distanceMin = std::max( 0.0f, obbWorldSpace.NearestDistanceToPlane( cameraPlane ) );
    float distanceMax = obbWorldSpace.FarthestDistanceToPlane( cameraPlane );

    // near DoF / far DoF transition ranges
    float nearDoFTransition = std::max( 0.0f, (m_settings.InFocusFrom - distanceMax) / m_settings.NearTransitionRange );
    float farDoFTransition  = std::max( 0.0f, (distanceMin - m_settings.InFocusTo) / m_settings.FarTransitionRange );
    
    return std::max( nearDoFTransition, farDoFTransition );
}

