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

// !!! SCHEDULED FOR DELETION !!!

#include "Core/vaCoreIncludes.h"

#include "Rendering/Effects/vaSimpleShadowMap.h"

#include "Rendering/DirectX/vaDirectXIncludes.h"
#include "Rendering/DirectX/vaShaderDX11.h"
#include "Rendering/DirectX/vaDirectXTools.h"

#include "Rendering/DirectX/vaTextureDX11.h"

namespace Vanilla
{
/*
    class vaSimpleShadowMapDX11 : public vaSimpleShadowMap, vaDirectXNotifyTarget
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    private:
        //vaVertexShaderDX11               m_vertexShader;
        //vaPixelShaderDX11                m_pixelShader;

        //vaDirectXVertexBuffer < vaVector3 >
        //    m_screenTriangleVertexBuffer;
        //ID3D11DepthStencilState *           m_depthStencilState;

        ID3D11SamplerState *            m_shadowCmpSamplerState;

        ID3D11RasterizerState *         m_rasterizerStateCullNone;
        ID3D11RasterizerState *         m_rasterizerStateCullCW;
        ID3D11RasterizerState *         m_rasterizerStateCullCCW;

        vaTypedConstantBufferWrapper < ShaderSimpleShadowsGlobalConstants >
                                        m_constantsBuffer;

        // this should maybe go into platform independent vaSimpleShadowMap
        vaViewport                      m_prevCanvasVP;
        std::shared_ptr<vaTexture>      m_prevCanvasRT;
        std::shared_ptr<vaTexture>      m_prevCanvasDS;

        bool                            m_depthSlopeDirty;

    protected:
        vaSimpleShadowMapDX11( const vaRenderingModuleParams & params );
        ~vaSimpleShadowMapDX11( );

    public:
        ID3D11RasterizerState *         GetRasterizerStateCullNone( )   { return m_rasterizerStateCullNone; }
        ID3D11RasterizerState *         GetRasterizerStateCullCW( )     { return m_rasterizerStateCullCW;   }
        ID3D11RasterizerState *         GetRasterizerStateCullCCW( )    { return m_rasterizerStateCullCCW;  }

    private:
        // vaDirectXNotifyTarget
        virtual void                    OnDeviceCreated( ID3D11Device* device, IDXGISwapChain* swapChain );
        virtual void                    OnDeviceDestroyed( );

    private:
        // vaSimpleShadowMapAPIInternalCallbacks
        virtual void                    InternalResolutionOrTexelWorldSizeChanged( );
        virtual void                    InternalStartGenerating( const vaSceneDrawContext & context );
        virtual void                    InternalStopGenerating( const vaSceneDrawContext & context );
        virtual void                    InternalStartUsing( const vaSceneDrawContext & context );
        virtual void                    InternalStopUsing( const vaSceneDrawContext & context );
    };
    */
}

