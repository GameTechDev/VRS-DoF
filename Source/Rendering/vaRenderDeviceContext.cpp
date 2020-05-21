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

#pragma once

#include "vaRenderDeviceContext.h"

#include "Core/System/vaFileTools.h"

#include "Rendering/vaShader.h"
#include "Rendering/vaRenderBuffers.h"
#include "Rendering/vaRenderMaterial.h"
#include "Rendering/vaLighting.h"
#include "Rendering/vaRenderGlobals.h"

#include "Rendering/vaGPUTimer.h"


using namespace Vanilla;


vaRenderDeviceContext::vaRenderDeviceContext( const vaRenderingModuleParams & params ) 
    : vaRenderingModule( params ), m_outputsState()
{ 
    m_tracer = VA_RENDERING_MODULE_CREATE_SHARED( vaGPUContextTracer, vaGPUContextTracerParams( *this ) );
}

vaRenderDeviceContext::~vaRenderDeviceContext( )
{
    assert( m_itemsStarted == vaRenderTypeFlags::None );
#ifdef VA_SCOPE_TRACE_ENABLED
    assert( m_frameBeginEndTrace == nullptr );
#endif
}

void vaRenderDeviceContext::BeginFrame( ) 
{ 
    m_tracer->BeginFrame( ); 
#ifdef VA_SCOPE_TRACE_ENABLED
    m_frameBeginEndTrace = new(m_frameBeginEndTraceStorage) vaScopeTrace( "RenderFrame", this );
#endif
}
void vaRenderDeviceContext::EndFrame( ) 
{
    SetRenderTarget( nullptr, nullptr, 0 );
#ifdef VA_SCOPE_TRACE_ENABLED
    m_frameBeginEndTrace->~vaScopeTrace(); m_frameBeginEndTrace = nullptr;
#endif
    m_tracer->EndFrame( );
}

vaRenderDeviceContext::RenderOutputsState::RenderOutputsState( )
{
    Viewport            = vaViewport( 0, 0 );
    ScissorRect         = vaRecti( 0, 0, 0, 0 );
    ScissorRectEnabled  = false;
    
    for( int i = 0; i < _countof(UAVInitialCounts); i++ )
        UAVInitialCounts[i] = 0;
    
    RenderTargetCount   = 0;
    UAVsStartSlot       = 0;
    UAVCount            = 0;
}

void vaRenderDeviceContext::SetRenderTarget( const std::shared_ptr<vaTexture> & renderTarget, const std::shared_ptr<vaTexture> & depthStencil, bool updateViewport )
{
    assert( m_itemsStarted == vaRenderTypeFlags::None );

    for( int i = 0; i < c_maxRTs; i++ )     m_outputsState.RenderTargets[i]      = nullptr;
    for( int i = 0; i < c_maxUAVs; i++ )    m_outputsState.UAVs[i]               = nullptr;
    for( int i = 0; i < c_maxUAVs; i++ )    m_outputsState.UAVInitialCounts[i]   = (uint32)-1;
    m_outputsState.RenderTargets[0]     = renderTarget;
    m_outputsState.DepthStencil         = depthStencil;
    m_outputsState.RenderTargetCount    = renderTarget != nullptr;
    m_outputsState.UAVsStartSlot        = 0;
    m_outputsState.UAVCount             = 0;
    m_outputsState.ScissorRect          = vaRecti( 0, 0, 0, 0 );
    m_outputsState.ScissorRectEnabled   = false;

    const std::shared_ptr<vaTexture> & anyRT = ( renderTarget != NULL ) ? ( renderTarget ) : ( depthStencil );

    vaViewport vp = m_outputsState.Viewport;

    if( anyRT != NULL )
    {
        assert( anyRT->GetType( ) == vaTextureType::Texture2D );   // others not supported (yet?)
        vp.X = 0;
        vp.Y = 0;
        vp.Width = anyRT->GetSizeX( );
        vp.Height = anyRT->GetSizeY( );
    }

    if( renderTarget != NULL )
    {
        assert( ( renderTarget->GetBindSupportFlags( ) & vaResourceBindSupportFlags::RenderTarget ) != 0 );
    }
    if( depthStencil != NULL )
    {
        assert( ( depthStencil->GetBindSupportFlags( ) & vaResourceBindSupportFlags::DepthStencil ) != 0 );
    }

    UpdateRenderTargetsDepthStencilUAVs( );

    if( updateViewport )
        SetViewport( vp );
}

void vaRenderDeviceContext::SetRenderTargetsAndUnorderedAccessViews( uint32 numRTs, const std::shared_ptr<vaTexture> * renderTargets, const std::shared_ptr<vaTexture> & depthStencil,
    uint32 UAVStartSlot, uint32 numUAVs, const std::shared_ptr<vaTexture> * UAVs, bool updateViewport, const uint32 * UAVInitialCounts )
{
    assert( m_itemsStarted == vaRenderTypeFlags::None );

    assert( numRTs <= c_maxRTs );
    assert( numUAVs <= c_maxUAVs );
    m_outputsState.RenderTargetCount = numRTs  = vaMath::Min( numRTs , c_maxRTs  );
    m_outputsState.UAVCount          = numUAVs = vaMath::Min( numUAVs, c_maxUAVs );

    for( size_t i = 0; i < c_maxRTs; i++ )     
        m_outputsState.RenderTargets[i]      = (i < m_outputsState.RenderTargetCount)?(renderTargets[i]):(nullptr);
    for( size_t i = 0; i < c_maxUAVs; i++ )    
    {
        m_outputsState.UAVs[i]               = (i < m_outputsState.UAVCount)?(UAVs[i]):(nullptr);
        m_outputsState.UAVInitialCounts[i]   = ( (i < m_outputsState.UAVCount) && (UAVInitialCounts != nullptr) )?( UAVInitialCounts[i] ):( -1 );
    }
    m_outputsState.DepthStencil = depthStencil;
    m_outputsState.UAVsStartSlot = UAVStartSlot;

    const std::shared_ptr<vaTexture> & anyRT = ( m_outputsState.RenderTargets[0] != NULL ) ? ( m_outputsState.RenderTargets[0] ) : ( depthStencil );

    vaViewport vp = m_outputsState.Viewport;

    if( anyRT != NULL )
    {
        assert( anyRT->GetType( ) == vaTextureType::Texture2D );   // others not supported (yet?)
        vp.X = 0;
        vp.Y = 0;
        vp.Width = anyRT->GetSizeX( );
        vp.Height = anyRT->GetSizeY( );
    }

    for( size_t i = 0; i < m_outputsState.RenderTargetCount; i++ )
    {
        if( m_outputsState.RenderTargets[i] != nullptr )
        {
            assert( ( m_outputsState.RenderTargets[i]->GetBindSupportFlags( ) & vaResourceBindSupportFlags::RenderTarget ) != 0 );
        }
    }
    for( size_t i = 0; i < m_outputsState.UAVCount; i++ )
    {
        if( m_outputsState.UAVs[i] != nullptr )
        {
            assert( ( m_outputsState.UAVs[i]->GetBindSupportFlags( ) & vaResourceBindSupportFlags::UnorderedAccess ) != 0 );
        }
    }
    if( depthStencil != NULL )
    {
        assert( ( depthStencil->GetBindSupportFlags( ) & vaResourceBindSupportFlags::DepthStencil ) != 0 );
    }

    UpdateRenderTargetsDepthStencilUAVs( );

    if( updateViewport )
    {
        // can't update viewport if no RTs
        assert(anyRT != NULL);
        if( anyRT != NULL )
            SetViewport( vp );
    }
}

void vaRenderDeviceContext::SetRenderTargets( uint32 numRTs, const std::shared_ptr<vaTexture> * renderTargets, const std::shared_ptr<vaTexture> & depthStencil, bool updateViewport )
{
    SetRenderTargetsAndUnorderedAccessViews( numRTs, renderTargets, depthStencil, 0, 0, nullptr, updateViewport );
}

void vaRenderDeviceContext::BeginItems( vaRenderTypeFlags typeFlags, vaSceneDrawContext* sceneDrawContext )
{
    vaShaderItemGlobals shaderGlobals;

    if( sceneDrawContext != nullptr )
    {
        assert( &sceneDrawContext->RenderDeviceContext == this );

        if( sceneDrawContext->Lighting != nullptr )
            sceneDrawContext->Lighting->UpdateAndSetToGlobals( *sceneDrawContext, shaderGlobals );

        sceneDrawContext->RenderDeviceContext.GetRenderDevice( ).GetRenderGlobals( ).UpdateAndSetToGlobals( *sceneDrawContext, shaderGlobals );

        GetRenderDevice().GetMaterialManager().UpdateAndSetToGlobals( *sceneDrawContext, shaderGlobals );
    }
    BeginItems( typeFlags, shaderGlobals );
}

vaDrawResultFlags vaRenderDeviceContext::CopySRVToCurrentOutput( shared_ptr<vaTexture> source )
{
    return GetRenderDevice().CopySRVToCurrentOutput( *this, source );
}

// useful for copying individual MIPs, in which case use Views created with vaTexture::CreateView
vaDrawResultFlags vaRenderDeviceContext::CopySRVToRTV( shared_ptr<vaTexture> destination, shared_ptr<vaTexture> source )
{
    return GetRenderDevice( ).CopySRVToRTV( *this, destination, source );
}

vaDrawResultFlags vaRenderDeviceContext::StretchRect( const shared_ptr<vaTexture> & srcTexture, const vaVector4 & dstRect, const vaVector4 & _srcRect, bool linearFilter, vaBlendMode blendMode, const vaVector4 & colorMul, const vaVector4 & colorAdd )
{
    return GetRenderDevice( ).StretchRect( *this, srcTexture, dstRect, _srcRect, linearFilter, blendMode, colorMul, colorAdd );
}

vaDrawResultFlags vaRenderDeviceContext::StretchRect( const shared_ptr<vaTexture> & dstTexture, const shared_ptr<vaTexture> & srcTexture, const vaVector4 & _dstRect, const vaVector4 & srcRect, bool linearFilter, vaBlendMode blendMode, const vaVector4 & colorMul, const vaVector4 & colorAdd )
{
    return GetRenderDevice( ).StretchRect( *this, dstTexture, srcTexture, _dstRect, srcRect, linearFilter, blendMode, colorMul, colorAdd );

}
