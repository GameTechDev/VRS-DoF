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

#include "Rendering/vaRenderDevice.h"
#include "Rendering/vaTextureHelpers.h"

#include "Rendering/vaRenderBuffers.h"

#include "vaAssetPack.h"

#include "Rendering/vaRenderMesh.h"
#include "Rendering/vaRenderMaterial.h"

#include "Rendering/vaDebugCanvas.h"

#include "Rendering/vaTexture.h"

#include "Core/System/vaFileTools.h"

#include "EmbeddedRenderingMedia.inc"

#include "IntegratedExternals/vaImguiIntegration.h"

#include "Rendering/vaRenderGlobals.h"

#include "Rendering/Effects/vaPostProcess.h"

#include "Core/Misc/vaProfiler.h"


using namespace Vanilla;

thread_local vaRenderDeviceThreadLocal vaRenderDevice::s_threadLocal;

vaRenderDevice::vaRenderDevice( )
{ 
    m_profilingEnabled = true; 

//    vaRenderingModuleRegistrar::CreateSingletonIfNotCreated( );

    for( int i = 0; i < BINARY_EMBEDDER_ITEM_COUNT; i++ )
    {
        wchar_t * name = BINARY_EMBEDDER_NAMES[i];
        unsigned char * data = BINARY_EMBEDDER_DATAS[i];
        int64 dataSize = BINARY_EMBEDDER_SIZES[i];
        int64 timeStamp = BINARY_EMBEDDER_TIMES[i];

        vaFileTools::EmbeddedFilesRegister( name, data, dataSize, timeStamp );
    }

    s_threadLocal.RenderThread = true;
}

void vaRenderDevice::InitializeBase( )
{
    assert( IsRenderThread() );

    m_canvas2D  = shared_ptr< vaDebugCanvas2D >( new vaDebugCanvas2D( vaRenderingModuleParams( *this ) ) );
    m_canvas3D  = shared_ptr< vaDebugCanvas3D >( new vaDebugCanvas3D( vaRenderingModuleParams( *this ) ) );

    m_textureTools = std::make_shared<vaTextureTools>( *this );
    m_renderMaterialManager = std::make_shared<vaRenderMaterialManager>( *this ); // shared_ptr<vaRenderMaterialManager>( VA_RENDERING_MODULE_CREATE( vaRenderMaterialManager, *this ) );
    m_renderMeshManager = shared_ptr<vaRenderMeshManager>( VA_RENDERING_MODULE_CREATE( vaRenderMeshManager, *this ) );
    m_assetPackManager = std::make_shared<vaAssetPackManager>( *this );//shared_ptr<vaAssetPackManager>( VA_RENDERING_MODULE_CREATE( vaAssetPackManager, *this ) );
    // m_postProcess = VA_RENDERING_MODULE_CREATE_SHARED( vaPostProcess, *this );

    m_renderGlobals = VA_RENDERING_MODULE_CREATE_SHARED( vaRenderGlobals, *this );

    // fullscreen triangle stuff & related
    {
        m_PPConstants = VA_RENDERING_MODULE_CREATE_SHARED( vaConstantBuffer, *this );
        m_PPConstants->Create( sizeof( PostProcessConstants ), nullptr, true );

        m_fsVertexShader                = VA_RENDERING_MODULE_CREATE_SHARED( vaVertexShader, *this );
        m_fsVertexBufferZ0              = VA_RENDERING_MODULE_CREATE_SHARED( vaVertexBuffer, *this );
        m_fsVertexBufferZ1              = VA_RENDERING_MODULE_CREATE_SHARED( vaVertexBuffer, *this );
        m_copyResourcePS                = VA_RENDERING_MODULE_CREATE_SHARED( vaPixelShader, *this );
        m_vertexShaderStretchRect       = VA_RENDERING_MODULE_CREATE_SHARED( vaVertexShader, *this );
        m_pixelShaderStretchRectLinear  = VA_RENDERING_MODULE_CREATE_SHARED( vaPixelShader, *this );
        m_pixelShaderStretchRectPoint   = VA_RENDERING_MODULE_CREATE_SHARED( vaPixelShader, *this );

        std::vector<vaVertexInputElementDesc> inputElements;
        inputElements.push_back( { "SV_Position", 0,    vaResourceFormat::R32G32B32A32_FLOAT,    0,  0, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );
        inputElements.push_back( { "TEXCOORD", 0,       vaResourceFormat::R32G32_FLOAT,          0, 16, vaVertexInputElementDesc::InputClassification::PerVertexData, 0 } );

        // full screen pass vertex shader
        {
            const char * pVSString = "void main( inout const float4 xPos : SV_Position, inout float2 UV : TEXCOORD0 ) { }";
            m_fsVertexShader->CreateShaderAndILFromBuffer( pVSString, "vs_5_0", "main", inputElements, vaShaderMacroContaner(), false );
        }

        // copy resource shader
        {
            string shaderCode = 
                "Texture2D g_source           : register( t0 );                 \n"
                "float4 main( in const float4 xPos : SV_Position ) : SV_Target  \n"
                "{                                                              \n"
                "   return g_source.Load( int3( xPos.xy, 0 ) );                 \n"
                "}                                                              \n";

            m_copyResourcePS->CreateShaderFromBuffer( shaderCode, "ps_5_0", "main", vaShaderMacroContaner(), false );
        }

        m_vertexShaderStretchRect->CreateShaderAndILFromFile( L"vaPostProcess.hlsl", "vs_5_0", "VSStretchRect", inputElements, { }, false );
        m_pixelShaderStretchRectLinear->CreateShaderFromFile( L"vaPostProcess.hlsl", "ps_5_0", "PSStretchRectLinear", { }, false );
        m_pixelShaderStretchRectPoint->CreateShaderFromFile( L"vaPostProcess.hlsl", "ps_5_0", "PSStretchRectPoint", { }, false );

        {
            // using one big triangle
            SimpleVertex fsVertices[3];
            float z = 0.0f;
            fsVertices[0] = SimpleVertex( -1.0f,  1.0f, z, 1.0f, 0.0f, 0.0f );
            fsVertices[1] = SimpleVertex( 3.0f,   1.0f, z, 1.0f, 2.0f, 0.0f );
            fsVertices[2] = SimpleVertex( -1.0f, -3.0f, z, 1.0f, 0.0f, 2.0f );

            m_fsVertexBufferZ0->Create( countof( fsVertices ), sizeof( SimpleVertex ), fsVertices, false );
        }

        {
            // using one big triangle
            SimpleVertex fsVertices[3];
            float z = 1.0f;
            fsVertices[0] = SimpleVertex( -1.0f,  1.0f, z, 1.0f, 0.0f, 0.0f );
            fsVertices[1] = SimpleVertex( 3.0f,   1.0f, z, 1.0f, 2.0f, 0.0f );
            fsVertices[2] = SimpleVertex( -1.0f, -3.0f, z, 1.0f, 0.0f, 2.0f );

            m_fsVertexBufferZ1->Create( countof( fsVertices ), sizeof( SimpleVertex ), fsVertices, false );
        }

        // this still lets all of them compile in parallel, just ensures they're done before leaving the function
        m_fsVertexShader->WaitFinishIfBackgroundCreateActive( );
        m_copyResourcePS->WaitFinishIfBackgroundCreateActive( );
        m_vertexShaderStretchRect->WaitFinishIfBackgroundCreateActive( );
        m_pixelShaderStretchRectLinear->WaitFinishIfBackgroundCreateActive( );
        m_pixelShaderStretchRectPoint->WaitFinishIfBackgroundCreateActive( );
    }
}

void vaRenderDevice::DeinitializeBase( )
{
    m_asyncBeginFrameCallbacks.InvokeAndDeactivate( *this, std::numeric_limits<float>::lowest() );

    assert( IsRenderThread() );
    m_renderGlobals                 = nullptr;
    m_PPConstants                   = nullptr;
    m_fsVertexShader                = nullptr;
    m_fsVertexBufferZ0              = nullptr;
    m_fsVertexBufferZ1              = nullptr;
    m_copyResourcePS                = nullptr;
    m_vertexShaderStretchRect       = nullptr;
    m_pixelShaderStretchRectLinear  = nullptr;
    m_pixelShaderStretchRectPoint   = nullptr;
    m_canvas2D                      = nullptr;
    m_canvas3D                      = nullptr;
    m_assetPackManager              = nullptr;
    m_textureTools                  = nullptr;
    m_postProcess                   = nullptr;
    m_renderMaterialManager         = nullptr;
    m_renderMeshManager             = nullptr;
    m_shaderManager                 = nullptr;
    vaBackgroundTaskManager::GetInstancePtr()->ClearAndRestart();
}

vaRenderDevice::~vaRenderDevice( ) 
{ 
    assert( IsRenderThread() );
    assert( !m_frameStarted );
    assert( m_disabled );
    assert( m_renderGlobals == nullptr );   // forgot to call DeinitializeBase()?
}

void vaRenderDevice::ExecuteAsyncBeginFrameCallbacks( float deltaTime )
{
    m_asyncBeginFrameCallbacks.Invoke( *this, deltaTime );
}

vaTextureTools & vaRenderDevice::GetTextureTools( )
{
    assert( IsRenderThread() );
    assert( m_textureTools != nullptr );
    return *m_textureTools;
}

vaRenderMaterialManager & vaRenderDevice::GetMaterialManager( )
{
    assert( m_renderMaterialManager != nullptr );
    return *m_renderMaterialManager;
}

vaRenderMeshManager &     vaRenderDevice::GetMeshManager( )
{
    assert( m_renderMeshManager != nullptr );
    return *m_renderMeshManager;
}

vaAssetPackManager &     vaRenderDevice::GetAssetPackManager( )
{
    assert( IsRenderThread() );
    assert( m_assetPackManager != nullptr );
    return *m_assetPackManager;
}

vaRenderGlobals & vaRenderDevice::GetRenderGlobals( )
{
    assert( IsRenderThread() );
    assert( m_renderGlobals != nullptr );
    return *m_renderGlobals;
}

vaPostProcess & vaRenderDevice::GetPostProcess( )
{
    assert( IsRenderThread( ) );
    if( m_postProcess == nullptr )
        m_postProcess = VA_RENDERING_MODULE_CREATE_SHARED( vaPostProcess, *this );
    return *m_postProcess;
}

void vaRenderDevice::BeginFrame( float deltaTime )
{
    assert( !m_disabled );
    assert( IsRenderThread() );
    m_totalTime += deltaTime;
    m_lastDeltaTime = deltaTime;
    assert( !m_frameStarted );
    m_frameStarted = true;
    m_currentFrameIndex++;

    if( m_mainDeviceContext != nullptr )
        m_mainDeviceContext->SetRenderTarget( GetCurrentBackbuffer(), nullptr, true );

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    // for DX12 there's an empty frame done before creating the swapchain - ignore some of the checks during that
    if( m_swapChainTextureSize != vaVector2i(0, 0) )
    {
        assert( m_imguiFrameStarted );      // ImGui always has frame set so anyone can imgui anything at any time (if on the main thread)
    }
#endif

    // implementer's class responsibility
    // m_mainDeviceContext->BeginFrame();

    e_BeginFrame.Invoke( deltaTime );
}

void vaRenderDevice::EndAndPresentFrame( int vsyncInterval )
{
    assert( !m_disabled );
    assert( IsRenderThread() );
    vsyncInterval;

    // implementer's class responsibility
    // m_mainDeviceContext->EndFrame( );

    assert( m_frameStarted );
    m_frameStarted = false;

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    // for DX12 there's an empty frame done before creating the swapchain - ignore some of the checks during that
    if( m_swapChainTextureSize != vaVector2i(0, 0) )
    {
        assert( m_imguiFrameStarted );
        if( !m_imguiFrameRendered )
        {
            // if we haven't rendered anything, reset imgui to avoid any unnecessary accumulation
            ImGuiEndFrame( );
            ImGuiNewFrame( );
        }
        m_imguiFrameRendered = false;
    }
#endif
}

void vaRenderDevice::FillFullscreenPassRenderItem( vaGraphicsItem & renderItem, bool zIs0 ) const
{
    assert( !m_disabled );

    // this should be thread-safe as long as the lifetime of the device is guaranteed
    renderItem.Topology         = vaPrimitiveTopology::TriangleList;
    renderItem.VertexShader     = GetFSVertexShader();
    renderItem.VertexBuffer     = (zIs0)?(GetFSVertexBufferZ0()):(GetFSVertexBufferZ1());
    renderItem.DrawType         = vaGraphicsItem::DrawType::DrawSimple;
    renderItem.DrawSimpleParams.VertexCount = 3;
}


namespace Vanilla
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    void ImSetBigClearSansRegular( ImFont * font );
    void ImSetBigClearSansBold(    ImFont * font );
#endif`
}

void vaRenderDevice::ImGuiCreate( )
{
    assert( IsRenderThread() );
#ifdef VA_IMGUI_INTEGRATION_ENABLED

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
        
    io.Fonts->AddFontDefault();

    // this would be a good place for DPI scaling.
    float bigSizePixels = 26.0f;
    float displayOffset = -1.0f;

    ImFontConfig fontConfig;

    vaFileTools::EmbeddedFileData fontFileData;

    fontFileData = vaFileTools::EmbeddedFilesFind( wstring( L"fonts:\\ClearSans-Regular.ttf" ) );
    if( fontFileData.HasContents( ) )
    {
        void * imguiBuffer = ImGui::MemAlloc( (int)fontFileData.MemStream->GetLength() );
        memcpy( imguiBuffer, fontFileData.MemStream->GetBuffer(), (int)fontFileData.MemStream->GetLength() );

        ImFont * font = io.Fonts->AddFontFromMemoryTTF( imguiBuffer, (int)fontFileData.MemStream->GetLength(), bigSizePixels, &fontConfig );
        font->DisplayOffset.y += displayOffset;   // Render 1 pixel down
        ImSetBigClearSansRegular( font );
    }

    fontFileData = vaFileTools::EmbeddedFilesFind( wstring( L"fonts:\\ClearSans-Bold.ttf" ) );
    if( fontFileData.HasContents( ) )
    {
        void * imguiBuffer = ImGui::MemAlloc( (int)fontFileData.MemStream->GetLength() );
        memcpy( imguiBuffer, fontFileData.MemStream->GetBuffer(), (int)fontFileData.MemStream->GetLength() );

        ImFont * font = io.Fonts->AddFontFromMemoryTTF( imguiBuffer, (int)fontFileData.MemStream->GetLength(), bigSizePixels, &fontConfig );
        font->DisplayOffset.y += displayOffset;   // Render 1 pixel down
        ImSetBigClearSansBold( font );
    }

    // enable docking
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
}

void vaRenderDevice::ImGuiDestroy( )
{
    assert( IsRenderThread() );
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::DestroyContext();
#endif
}

void vaRenderDevice::ImGuiEndFrame( )
{
    assert( !m_disabled );

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    assert( m_imguiFrameStarted );
    ImGui::EndFrame();
    m_imguiFrameStarted = false;
#endif
}

void vaRenderDevice::ImGuiRender( vaRenderDeviceContext & renderContext )
{
    assert( !m_disabled );

    renderContext;
    assert( m_frameStarted );
    assert( !m_imguiFrameRendered );    // can't render twice per frame due to internal imgui constraints!
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGuiEndFrameAndRender( renderContext );
    m_imguiFrameRendered = true;
    ImGuiNewFrame();
#endif
}

vaDrawResultFlags vaRenderDevice::CopySRVToCurrentOutput( vaRenderDeviceContext & renderContext, shared_ptr<vaTexture> source )
{
    vaRenderDeviceContext::RenderOutputsState currentOutputs = renderContext.GetOutputs( );
    if( currentOutputs.RenderTargets[0] == nullptr
        || currentOutputs.RenderTargets[0]->GetType( ) != source->GetType( )
        || currentOutputs.RenderTargets[0]->GetSizeX( ) != source->GetSizeX( )
        || currentOutputs.RenderTargets[0]->GetSizeY( ) != source->GetSizeY( )
        || currentOutputs.RenderTargets[0]->GetSizeZ( ) != source->GetSizeZ( )
        || currentOutputs.RenderTargets[0]->GetSampleCount( ) != source->GetSampleCount( )
        )
    {
        assert( false );
        VA_ERROR( "vaRenderDevice::CopySRVToRTV - not supported or incorrect parameters" );
        return vaDrawResultFlags::UnspecifiedError;
    }

    vaGraphicsItem renderItem;
    FillFullscreenPassRenderItem( renderItem );
    renderItem.ShaderResourceViews[0] = source;
    renderItem.PixelShader = renderContext.GetRenderDevice( ).GetFSCopyResourcePS( );
    return renderContext.ExecuteSingleItem( renderItem, nullptr );

}

// useful for copying individual MIPs, in which case use Views created with vaTexture::CreateView
vaDrawResultFlags vaRenderDevice::CopySRVToRTV( vaRenderDeviceContext & renderContext, shared_ptr<vaTexture> destination, shared_ptr<vaTexture> source )
{
    vaRenderDeviceContext::RenderOutputsState backupOutputs = renderContext.GetOutputs( );

    renderContext.SetRenderTarget( destination, nullptr, true );
    auto retVal = CopySRVToCurrentOutput( renderContext, source );

    renderContext.SetOutputs( backupOutputs );

    return retVal;
}

vaDrawResultFlags vaRenderDevice::StretchRect( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & srcTexture, const vaVector4 & _dstRect, const vaVector4 & _srcRect, bool linearFilter, vaBlendMode blendMode, const vaVector4 & colorMul, const vaVector4 & colorAdd )
{
    VA_TRACE_CPUGPU_SCOPE( PP_StretchRect, renderContext );

    vaVector4 dstRect = _dstRect;
    vaVector4 srcRect = _srcRect;

    if( dstRect == vaVector4::Zero )
    {
        if( renderContext.GetRenderTarget() != nullptr )
            dstRect = { 0, 0, (float)renderContext.GetRenderTarget()->GetSizeX( ), (float)renderContext.GetRenderTarget()->GetSizeY( ) };
        else
        {
            assert( false );
        }
    }

    if( srcRect == vaVector4::Zero )
        srcRect = { 0, 0, (float)srcTexture->GetSizeX( ), (float)srcTexture->GetSizeY( ) };
    assert( dstRect != vaVector4::Zero );

    // not yet supported / tested
    assert( dstRect.x == 0 );
    assert( dstRect.y == 0 );

    vaVector2 dstPixSize = vaVector2( 1.0f / ( dstRect.z - dstRect.x ), 1.0f / ( dstRect.w - dstRect.y ) );

    vaVector2 srcPixSize = vaVector2( 1.0f / (float)srcTexture->GetSizeX( ), 1.0f / (float)srcTexture->GetSizeY( ) );

    PostProcessConstants consts;
    consts.Param1.x = dstPixSize.x * dstRect.x * 2.0f - 1.0f;
    consts.Param1.y = 1.0f - dstPixSize.y * dstRect.y * 2.0f;
    consts.Param1.z = dstPixSize.x * dstRect.z * 2.0f - 1.0f;
    consts.Param1.w = 1.0f - dstPixSize.y * dstRect.w * 2.0f;

    consts.Param2.x = srcPixSize.x * srcRect.x;
    consts.Param2.y = srcPixSize.y * srcRect.y;
    consts.Param2.z = srcPixSize.x * srcRect.z;
    consts.Param2.w = srcPixSize.y * srcRect.w;

    consts.Param3 = colorMul;
    consts.Param4 = colorAdd;

    //consts.Param2.x = (float)viewport.Width
    //consts.Param2 = dstRect;

    m_PPConstants->Update( renderContext, &consts, sizeof(consts) );

    vaGraphicsItem renderItem;
    FillFullscreenPassRenderItem( renderItem );

    renderItem.ConstantBuffers[POSTPROCESS_CONSTANTSBUFFERSLOT] = m_PPConstants;
    renderItem.ShaderResourceViews[POSTPROCESS_TEXTURE_SLOT0] = srcTexture;

    renderItem.VertexShader = m_vertexShaderStretchRect;
    //renderItem.VertexShader->WaitFinishIfBackgroundCreateActive();
    renderItem.PixelShader = ( linearFilter ) ? ( m_pixelShaderStretchRectLinear ) : ( m_pixelShaderStretchRectPoint );
    //renderItem.PixelShader->WaitFinishIfBackgroundCreateActive();
    renderItem.BlendMode = blendMode;

    return renderContext.ExecuteSingleItem( renderItem, nullptr );
}

vaDrawResultFlags vaRenderDevice::StretchRect( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & dstTexture, const shared_ptr<vaTexture> & srcTexture, const vaVector4 & _dstRect, const vaVector4 & srcRect, bool linearFilter, vaBlendMode blendMode, const vaVector4 & colorMul, const vaVector4 & colorAdd )
{
    vaVector4 dstRect = _dstRect;
    if( dstRect == vaVector4::Zero )
        dstRect = { 0, 0, (float)dstTexture->GetSizeX( ), (float)dstTexture->GetSizeY( ) };

    auto outputs = renderContext.GetOutputs( );
    renderContext.SetRenderTarget( dstTexture, nullptr, true );
    auto ret = renderContext.StretchRect( srcTexture, dstRect, srcRect, linearFilter, blendMode, colorMul, colorAdd );
    renderContext.SetOutputs( outputs );
    return ret;

}