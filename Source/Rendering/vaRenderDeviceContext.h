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
// Author(s):  Filip Strugar (filip.strugar@intel.com)
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Core/vaCoreIncludes.h"

#include "Rendering/vaRendering.h"
#include "Rendering/vaRenderDevice.h"

#include "Rendering/vaTexture.h"

#include "Rendering/vaRenderBuffers.h"
#include "Rendering/Shaders/vaSharedTypes_PostProcess.h"

namespace Vanilla
{
    class vaRenderDevice;
    class vaGPUContextTracer;
    
    enum class vaRenderTypeFlags : uint32
    {
        None                        = 0,
        Graphics                    = ( 1 << 0 ),
        Compute                     = ( 1 << 1 ),
        // Copy                     = ( 1 << 2 )
    };

    BITFLAG_ENUM_CLASS_HELPER( vaRenderTypeFlags );


    // vaRenderDeviceContext is used to get/set current render targets and access rendering API stuff like contexts, etc.
    class vaRenderDeviceContext : public vaRenderingModule
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    public:
        static const uint32                 c_maxRTs = 8;
        static const uint32                 c_maxUAVs = 8;

        struct RenderOutputsState
        {
            vaViewport                      Viewport;
            vaRecti                         ScissorRect;
            bool                            ScissorRectEnabled;

            shared_ptr<vaTexture>           RenderTargets[c_maxRTs];
            shared_ptr<vaTexture>           UAVs[c_maxUAVs];
            uint32                          UAVInitialCounts[c_maxUAVs];
            std::shared_ptr<vaTexture>      DepthStencil;

            uint32                          RenderTargetCount;
            uint32                          UAVsStartSlot;
            uint32                          UAVCount;

            RenderOutputsState( );
        };

    protected:
        RenderOutputsState                  m_outputsState;

        // (optionally) captured at BeginItems and released at EndItems
        // vaSceneDrawContext *                m_currentSceneDrawContext                           = nullptr;
        vaRenderTypeFlags                   m_itemsStarted                  = vaRenderTypeFlags::None;
        vaShaderItemGlobals                 m_currentShaderItemGlobals;

        shared_ptr<vaGPUContextTracer>      m_tracer;

#ifdef VA_SCOPE_TRACE_ENABLED
        char                                m_frameBeginEndTraceStorage[sizeof(vaScopeTrace)];
        vaScopeTrace *                      m_frameBeginEndTrace = nullptr;
#endif

    protected:

    protected:
        vaRenderDeviceContext( const vaRenderingModuleParams & params );
    
    public:
        virtual ~vaRenderDeviceContext( );
        //
    public:
        virtual void                        BeginFrame( );
        virtual void                        EndFrame( )  ;

        const vaViewport &                  GetViewport( ) const                                { return m_outputsState.Viewport;    }
        void                                SetViewport( const vaViewport & vp )                { m_outputsState.Viewport = vp; m_outputsState.ScissorRect = vaRecti( 0, 0, 0, 0 ); m_outputsState.ScissorRectEnabled = false; UpdateViewport(); }

        void                                GetScissorRect( vaRecti & outScissorRect, bool & outScissorRectEnabled ) { outScissorRect = m_outputsState.ScissorRect; outScissorRectEnabled = m_outputsState.ScissorRectEnabled; }
        void                                SetViewportAndScissorRect( const vaViewport & vp, const vaRecti & scissorRect )  
                                                                                                { m_outputsState.Viewport = vp; m_outputsState.ScissorRect = scissorRect; m_outputsState.ScissorRectEnabled = true; UpdateViewport(); }

        const std::shared_ptr<vaTexture> &  GetRenderTarget( ) const                            { return m_outputsState.RenderTargets[0]; }
        const std::shared_ptr<vaTexture> &  GetDepthStencil( ) const                            { return m_outputsState.DepthStencil; }
        const shared_ptr<vaTexture> *       GetRenderTargets( ) const                           { return m_outputsState.RenderTargets; }
        const shared_ptr<vaTexture> *       GetUAVs( ) const                                    { return m_outputsState.RenderTargets; }
        
        uint32                              GetRenderTargetCount( ) const                       { return m_outputsState.RenderTargetCount; }
        uint32                              GetUAVsStartSlot( ) const                           { return m_outputsState.UAVsStartSlot; }
        uint32                              GetUAVCount( ) const                                { return m_outputsState.UAVCount; }

        const RenderOutputsState &          GetOutputs( ) const                                 { assert( m_renderDevice.IsFrameStarted()); return m_outputsState; }
        void                                SetOutputs( const RenderOutputsState & state )      { assert( m_renderDevice.IsFrameStarted()); m_outputsState = state; UpdateRenderTargetsDepthStencilUAVs(); UpdateViewport(); }

        // All these SetRenderTargetXXX below are simply helpers for setting m_outputsState - equivalent to filling the RenderOutputsState and doing SetOutputs (although more optimal)
        void                                SetRenderTarget( const std::shared_ptr<vaTexture> & renderTarget, const std::shared_ptr<vaTexture> & depthStencil, bool updateViewport );

        void                                SetRenderTargets( uint32 numRTs, const std::shared_ptr<vaTexture> * renderTargets, const std::shared_ptr<vaTexture> & depthStencil, bool updateViewport );

        void                                SetRenderTargetsAndUnorderedAccessViews( uint32 numRTs, const std::shared_ptr<vaTexture> * renderTargets, const std::shared_ptr<vaTexture> & depthStencil, 
                                                                                        uint32 UAVStartSlot, uint32 numUAVs, const std::shared_ptr<vaTexture> * UAVs, bool updateViewport, const uint32 * UAVInitialCounts = nullptr );
        // platform-independent way of drawing items; it is immediate and begin/end items is there mostly for state caching; 
        // when present, 'sceneDrawContext' parameter represents global states like lighting; these are same for all items between BeginItems/EndItems
        void                                BeginItems( vaRenderTypeFlags typeFlags, vaSceneDrawContext* sceneDrawContext );
        virtual vaDrawResultFlags           ExecuteItem( const vaGraphicsItem & renderItem )                                                = 0;
        virtual vaDrawResultFlags           ExecuteItem( const vaComputeItem & computeItem )                                                = 0;
        virtual void                        EndItems( )                                                                                     { assert( m_itemsStarted != vaRenderTypeFlags::None ); m_itemsStarted = vaRenderTypeFlags::None; }
        vaDrawResultFlags                   ExecuteSingleItem( const vaGraphicsItem & renderItem, vaSceneDrawContext* sceneDrawContext )    { assert( m_itemsStarted == vaRenderTypeFlags::None ); BeginItems( vaRenderTypeFlags::Graphics, sceneDrawContext ); vaDrawResultFlags drawResults = ExecuteItem( renderItem ); EndItems(); assert( m_itemsStarted == vaRenderTypeFlags::None ); return drawResults; }
        vaDrawResultFlags                   ExecuteSingleItem( const vaComputeItem & computeItem, vaSceneDrawContext* sceneDrawContext )    { assert( m_itemsStarted == vaRenderTypeFlags::None ); BeginItems( vaRenderTypeFlags::Compute, sceneDrawContext ); vaDrawResultFlags drawResults = ExecuteItem( computeItem ); EndItems(); assert( m_itemsStarted == vaRenderTypeFlags::None ); return drawResults; }

        virtual vaRenderTypeFlags           GetSupportFlags( ) const                                                                        { return vaRenderTypeFlags::Graphics | vaRenderTypeFlags::Compute; }
        virtual vaRenderTypeFlags           GetStartedFlags( ) const                                                                        { return m_itemsStarted; }

        // Helper to fill in vaGraphicsItem with most common elements needed to render a fullscreen quad (or triangle, whatever) - vertex shader, vertex buffer, etc
        void                                FillFullscreenPassRenderItem( vaGraphicsItem & renderItem, bool zIs0 = true ) const             { GetRenderDevice().FillFullscreenPassRenderItem( renderItem, zIs0 ); }

        inline const shared_ptr<vaGPUContextTracer>& GetTracer( ) const { return m_tracer; }

    protected:
        virtual void                        UpdateViewport( ) = 0;
        virtual void                        UpdateRenderTargetsDepthStencilUAVs( ) = 0;

        virtual void                        BeginItems( vaRenderTypeFlags typeFlags, const vaShaderItemGlobals & shaderGlobals )            { assert( m_itemsStarted == vaRenderTypeFlags::None ); assert( typeFlags != vaRenderTypeFlags::None ); m_itemsStarted = typeFlags; m_currentShaderItemGlobals = shaderGlobals; };

    public:
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // useful for copying individual MIPs, in which case use Views created with vaTexture::CreateView
        virtual vaDrawResultFlags           CopySRVToRTV( shared_ptr<vaTexture> destination, shared_ptr<vaTexture> source );
        virtual vaDrawResultFlags           CopySRVToCurrentOutput( shared_ptr<vaTexture> source );
        //
        // Copies srcTexture into current render target with stretching using requested filter and blend modes.
        virtual vaDrawResultFlags           StretchRect( const shared_ptr<vaTexture> & srcTexture, const vaVector4 & dstRect = {0,0,0,0}, const vaVector4 & srcRect = {0,0,0,0}, bool linearFilter = true, vaBlendMode blendMode = vaBlendMode::Opaque, const vaVector4 & colorMul = vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ), const vaVector4 & colorAdd = vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) );
        // Copies srcTexture into dstTexture with stretching using requested filter and blend modes. Backups current render target and restores it after.
        virtual vaDrawResultFlags           StretchRect( const shared_ptr<vaTexture> & dstTexture, const shared_ptr<vaTexture> & srcTexture, const vaVector4 & dstRect = {0,0,0,0}, const vaVector4 & srcRect = {0,0,0,0}, bool linearFilter = true, vaBlendMode blendMode = vaBlendMode::Opaque, const vaVector4 & colorMul = vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ), const vaVector4 & colorAdd = vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) );
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    };


}