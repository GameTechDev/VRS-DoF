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

#include "Core/vaCoreIncludes.h"
#include "Core/vaEvent.h"

#include "Rendering/vaRendering.h"

namespace Vanilla
{
    class vaDebugCanvas2D;
    class vaDebugCanvas3D;

    class vaTexture;
    class vaRenderDeviceContext;

    class vaConstantBuffer;

    struct vaGraphicsItem;
    struct vaSceneDrawContext;

    class vaTextureTools;
    class vaRenderMaterialManager;
    class vaRenderMeshManager;
    class vaAssetPackManager;
    class vaPostProcess;

    class vaShaderManager;

    class vaVertexShader;
    class vaPixelShader;
    class vaVertexBuffer;
    
    class vaRenderGlobals;
    class vaRenderDevice;
    class vaRenderDeviceContext;

    struct vaRenderDeviceCapabilities
    {
        struct _VariableShadingRate
        {
            bool        Tier1                                                   = false;
            bool        Tier2                                                   = false;
            bool        AdditionalShadingRatesSupported                         = false;        // Indicates whether 2x4, 4x2, and 4x4 coarse pixel sizes are supported for single-sampled rendering; and whether coarse pixel size 2x4 is supported for 2x MSAA.
            bool        PerPrimitiveShadingRateSupportedWithViewportIndexing    = false;        // Indicates whether the per-provoking-vertex (also known as per-primitive) rate can be used with more than one viewport. If so, then, in that case, that rate can be used when SV_ViewportIndex is written to.
            uint        ShadingRateImageTileSize                                = 32;
        }                           VariableShadingRate;
    };

    struct vaRenderDeviceThreadLocal
    {
        bool        RenderThread          = false;
        bool        RenderThreadSynced    = false;        // MainThread or guaranteed not to run in parallel with MainThread
    };

    class vaRenderDevice
    {
        vaThreadSpecificAsyncCallbackQueue< vaRenderDevice &, float >
                                                m_asyncBeginFrameCallbacks;

    public:
        vaEvent<void(vaRenderDevice &)>         e_DeviceFullyInitialized;
        vaEvent<void()>                         e_DeviceAboutToBeDestroyed;
        vaEvent<void(float deltaTime)>          e_BeginFrame;

    protected:
        struct SimpleVertex
        {
            float   Position[4];
            float   UV[2];

            SimpleVertex( ) {};
            SimpleVertex( float px, float py, float pz, float pw, float uvx, float uvy ) { Position[0] = px; Position[1] = py; Position[2] = pz; Position[3] = pw; UV[0] = uvx; UV[1] = uvy; }
        };
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Needed for a couple of general utility functions - they used to live in vaPostProcess but since they're used frequently, creating the whole 
        // vaPostProcess instance for them sounds just too troublesome, so placing them here
        shared_ptr< vaConstantBuffer >          m_PPConstants;
        shared_ptr<vaVertexShader>              m_fsVertexShader;  
        shared_ptr<vaVertexBuffer>              m_fsVertexBufferZ0;             // TODO: use trick from the link to avoid using vbuffers at all: https://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/
        shared_ptr<vaVertexBuffer>              m_fsVertexBufferZ1;             // TODO: use trick from the link to avoid using vbuffers at all: https://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/
        shared_ptr<vaPixelShader>               m_copyResourcePS;  
        shared_ptr<vaVertexShader>              m_vertexShaderStretchRect;
        shared_ptr<vaPixelShader>               m_pixelShaderStretchRectLinear;
        shared_ptr<vaPixelShader>               m_pixelShaderStretchRectPoint;

    protected:

        int64                                   m_currentFrameIndex            = 0;

        shared_ptr<vaDebugCanvas2D>             m_canvas2D;
        shared_ptr<vaDebugCanvas3D>             m_canvas3D;

        shared_ptr<vaRenderDeviceContext>       m_mainDeviceContext;

        bool                                    m_profilingEnabled;

        // maybe do https://en.cppreference.com/w/cpp/memory/shared_ptr/atomic2 for the future...
        shared_ptr<vaTextureTools>              m_textureTools;
        shared_ptr<vaRenderGlobals>             m_renderGlobals;
        shared_ptr<vaRenderMaterialManager>     m_renderMaterialManager;
        shared_ptr<vaRenderMeshManager>         m_renderMeshManager;
        shared_ptr<vaAssetPackManager>          m_assetPackManager;
        shared_ptr<vaShaderManager>             m_shaderManager;
        shared_ptr<vaPostProcess>               m_postProcess;

        vaVector2i                              m_swapChainTextureSize = { 0, 0 };
        string                                  m_adapterNameShort;
        uint                                    m_adapterVendorID;
        string                                  m_adapterNameID;             // adapterNameID is a mix of .Description and [.SubSysId] that uniquely identifies the current graphics device on the system


        double                                  m_totalTime             = 0.0;
        float                                   m_lastDeltaTime         = 0.0f;
        bool                                    m_frameStarted = false;
        //std::mutex                              m_frameMutex;

        bool                                    m_imguiFrameStarted = false;
        bool                                    m_imguiFrameRendered = false;

        // a lot of the vaRenderDevice functionality is locked to the thread that created the object
        std::atomic<std::thread::id>            m_threadID              = std::this_thread::get_id();

        vaFullscreenState                       m_fullscreenState       = vaFullscreenState::Unknown;
        
        // set when window is destroyed, presents and additional rendering is no longer possible but device is still not destroyed
        bool                                    m_disabled             = false;

        vaRenderDeviceCapabilities              m_caps;

        static thread_local vaRenderDeviceThreadLocal  s_threadLocal;

    public:
        static const int                        c_BackbufferCount    = 2;

    public:
        vaRenderDevice( );
        virtual ~vaRenderDevice( );

    protected:
        void                                InitializeBase( );
        void                                DeinitializeBase( );
        void                                ExecuteAsyncBeginFrameCallbacks( float deltaTime );

    public:
        virtual void                        CreateSwapChain( int width, int height, HWND hwnd, vaFullscreenState fullscreenState )  = 0;
        virtual bool                        ResizeSwapChain( int width, int height, vaFullscreenState fullscreenState )             = 0;    // return true if actually resized (for further handling)

        void                                SetDisabled( )                                                              { m_disabled = true; }

        const vaRenderDeviceCapabilities &  GetCapabilities( ) const                                                    { return m_caps; }

        const vaVector2i &                  GetSwapChainTextureSize( ) const { return m_swapChainTextureSize; }

        virtual shared_ptr<vaTexture>       GetCurrentBackbuffer( ) const                                               = 0;

        virtual bool                        IsSwapChainCreated( ) const                                                 = 0;
        virtual void                        SetWindowed( )                                                              = 0;    // disable fullscreen
        virtual vaFullscreenState           GetFullscreenState( ) const                                                 { return m_fullscreenState; }

       
        virtual void                        BeginFrame( float deltaTime )              ;
        virtual void                        EndAndPresentFrame( int vsyncInterval = 0 );

        // this is a TODO for the future, for cases where a temporary per-frame resource (such as descriptors) gets exhausted due to huge per-frame
        // rendering requirements (for ex., doing supersampling or machine learning).
        // This should automatically get called by vaRenderDeviceContext::EndItems and in other places, and be capable of ending the frame (no UI!)
        // and re-starting it, with everything set up as it was. Should work in our out of BeginItems/EndItems scope.
        // virtual void                        SyncAndFlush( ) = 0;

        
        bool                                IsCreationThread( ) const                                                   { return m_threadID == std::this_thread::get_id(); }
        //bool                                IsRenderThread( ) const                                                     { return s_threadLocal.RenderThread || s_threadLocal.RenderThreadSynced; }
        bool                                IsFrameStarted( ) const                                                     { return m_frameStarted; }

        double                              GetTotalTime( ) const                                                       { return m_totalTime; }
        int64                               GetCurrentFrameIndex( ) const                                               { return m_currentFrameIndex; }

        vaRenderDeviceContext *             GetMainContext( ) const                                                     { assert( IsRenderThread() ); return m_mainDeviceContext.get(); }
                
        vaDebugCanvas2D &                   GetCanvas2D( )                                                              { return *m_canvas2D; }
        vaDebugCanvas3D &                   GetCanvas3D( )                                                              { return *m_canvas3D; }


        bool                                IsProfilingEnabled( )                                                       { return m_profilingEnabled; }

        const string &                      GetAdapterNameShort( ) const                                                { return m_adapterNameShort; }
        const string &                      GetAdapterNameID( ) const                                                   { return m_adapterNameID; }
        uint                                GetAdapterVendorID( ) const                                                 { return m_adapterVendorID; }
        virtual string                      GetAPIName( ) const                                                         = 0;

        // Fullscreen 
        const shared_ptr<vaVertexShader> &  GetFSVertexShader( ) const                                                  { return m_fsVertexShader;   }
        const shared_ptr<vaVertexBuffer> &  GetFSVertexBufferZ0( ) const                                                { return m_fsVertexBufferZ0; }
        const shared_ptr<vaVertexBuffer> &  GetFSVertexBufferZ1( ) const                                                { return m_fsVertexBufferZ1; }
        const shared_ptr<vaPixelShader>  &  GetFSCopyResourcePS( ) const                                                { return m_copyResourcePS;   }
        void                                FillFullscreenPassRenderItem( vaGraphicsItem & renderItem, bool zIs0 = true ) const;

    private:
        friend vaApplicationBase;
        // ImGui gets drawn into the main device context ( GetMainContext() ) - this is fixed for now but could be a parameter
        void                                ImGuiRender( vaRenderDeviceContext & renderContext );

    public:
        // Few notes The rules for async callback are as follows:
        //  1.) callbacks can be added into the queue from any thread
        //  2.) if it is added from the render thread and you call .wait() before it executed, it will deadlock
        //  3.) otherwise feel free to .get()/.wait() on the future
        //  4.) if device gets destroyed with some callbacks enqueued, they will get called during destruction but with deltaTime set to std::numeric_limits<float>::lowest() and no more callbacks will be allowed to get added
        std::future<bool>                   AsyncInvokeAtBeginFrame( std::function<bool( vaRenderDevice &, float deltaTime )> && callback )  { return m_asyncBeginFrameCallbacks.Enqueue( std::forward<decltype(callback)>(callback) ); }


    protected:
        virtual void                        ImGuiCreate( );
        virtual void                        ImGuiDestroy( );
        virtual void                        ImGuiNewFrame( ) = 0;
        virtual void                        ImGuiEndFrame( );
        // ImGui gets drawn into the main device context ( GetMainContext() ) - this is fixed for now but could be a parameter
        virtual void                        ImGuiEndFrameAndRender( vaRenderDeviceContext & renderContext ) = 0;



    public:
        // These are essentially API dependencies - they require graphics API to be initialized so there's no point setting them up separately
        vaTextureTools &                    GetTextureTools( );
        vaRenderMaterialManager &           GetMaterialManager( );
        vaRenderMeshManager &               GetMeshManager( );
        vaAssetPackManager &                GetAssetPackManager( );
        virtual vaShaderManager &           GetShaderManager( )                                                         = 0;
        vaRenderGlobals &                   GetRenderGlobals( );
        vaPostProcess &                     GetPostProcess( );

    public:
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // useful for copying individual MIPs, in which case use Views created with vaTexture::CreateView
        virtual vaDrawResultFlags           CopySRVToRTV( vaRenderDeviceContext & renderContext, shared_ptr<vaTexture> destination, shared_ptr<vaTexture> source );
        virtual vaDrawResultFlags           CopySRVToCurrentOutput( vaRenderDeviceContext & renderContext, shared_ptr<vaTexture> source );
        //
        // Copies srcTexture into current render target with stretching using requested filter and blend modes.
        virtual vaDrawResultFlags           StretchRect( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & srcTexture, const vaVector4 & dstRect = {0,0,0,0}, const vaVector4 & srcRect = {0,0,0,0}, bool linearFilter = true, vaBlendMode blendMode = vaBlendMode::Opaque, const vaVector4 & colorMul = vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ), const vaVector4 & colorAdd = vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) );
        // Copies srcTexture into dstTexture with stretching using requested filter and blend modes. Backups current render target and restores it after.
        virtual vaDrawResultFlags           StretchRect( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & dstTexture, const shared_ptr<vaTexture> & srcTexture, const vaVector4 & dstRect = {0,0,0,0}, const vaVector4 & srcRect = {0,0,0,0}, bool linearFilter = true, vaBlendMode blendMode = vaBlendMode::Opaque, const vaVector4 & colorMul = vaVector4( 1.0f, 1.0f, 1.0f, 1.0f ), const vaVector4 & colorAdd = vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) );
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    public:
        // changing
        static const vaRenderDeviceThreadLocal & ThreadLocal( )                                                         { return s_threadLocal; }
        static void                         SetSyncedWithRenderThread( )                                                { s_threadLocal.RenderThreadSynced = true; }
        static bool                         IsRenderThread( )                                                           { return s_threadLocal.RenderThread || s_threadLocal.RenderThreadSynced; }


    public:
        template< typename CastToType >
        CastToType                          SafeCast( )                                                                 
        {
#ifdef _DEBUG
            CastToType ret = dynamic_cast< CastToType >( this );
            assert( ret != NULL );
            return ret;
#else
            return static_cast< CastToType >( this );
#endif
        }

        // ID3D11Device *             GetPlatformDevice( ) const                                  { m_device; }
    };

}