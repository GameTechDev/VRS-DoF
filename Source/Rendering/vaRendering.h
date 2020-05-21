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

#include "Scene/vaCameraBase.h"

#include "Core/vaUIDObject.h"

#include "Core/Misc/vaProfiler.h"

#include "Rendering/Shaders/vaSharedTypes.h"

namespace Vanilla
{
    class vaRenderDevice;
    class vaRenderingModule;
    class vaRenderingModuleAPIImplementation;
    //class vaRenderDevice;
    //class vaRenderDeviceContext;
    class vaRenderGlobals;
    class vaLighting;
    class vaAssetPack;
    class vaTexture;
    class vaXMLSerializer;
    class vaPixelShader;
    class vaConstantBuffer;
    class vaVertexBuffer;
    class vaIndexBuffer;
    class vaVertexShader;
    class vaGeometryShader;
    class vaHullShader;
    class vaDomainShader;
    class vaPixelShader;
    class vaComputeShader;
    class vaShaderResource;

    // Not sure this belongs here but whatever
    enum class vaFullscreenState : int32
    {
        Unknown                 = 0,
        Windowed                = 1,
        Fullscreen              = 2,
        FullscreenBorderless    = 3
    };

    // This should be renamed to something more sensible - render type? render path? 
    enum class vaDrawContextOutputType : int32
    {
        DepthOnly       = 0,            // Z-pre-pass or shadowmap; no pixels are shaded unless alpha tested
        Forward         = 1,            // regular rendering (used for post process passes as well)
        Deferred        = 2,            // usually multiple render targets, no blending, etc
        CustomShadow    = 3,            // all non-depth-only shadow variations
        //GenericShading                // maybe for future testing/prototyping, a generic function similar to CustomShadow
    };

    enum class vaDrawContextFlags : uint32
    {
        None                                        = 0,
        DebugWireframePass                          = ( 1 << 0 ),   // consider using vaDrawContextFlags::SetZOffsettedProjMatrix as well
        SetZOffsettedProjMatrix                     = ( 1 << 1 ),
        // CustomShadowmapShaderLinearDistanceToCenter = ( 1 << 2 ),   // only valid when OutputType is of vaDrawContextOutputType::CustomShadow (TODO: remove this)
    };

    BITFLAG_ENUM_CLASS_HELPER( vaDrawContextFlags );

    enum class vaBlendMode : int32
    {
        Opaque,
        Additive,
        AlphaBlend,
        PremultAlphaBlend,
        Mult,
        OffscreenAccumulate,    // for later compositing with PremultAlphaBlend - see https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch23.html (23.5 Alpha Blending) for more detail (although they have a mistake in their blend states...)
    };

    // This has some overlap in the meaning with vaBlendMode but is a higher-level abstraction; blend mode only defines color blending operation while LayerMode
    // defines blend mode, draw order (solid and alpha tested go first, then decal, then transparencies), depth buffer use and etc.
    enum class vaLayerMode : int32
    {
        Opaque              = 0,        // Classic opaque geometry      (writes into depth, overwrites color)
        AlphaTest           = 1,        // Opaque but uses alpha test   (writes into depth, overwrites color)
        Decal               = 2,        // has to be placed upon existing opaque geometry; always drawn before all other transparencies (drawing order/visibility guaranteed by depth buffer), alpha-blends into color, doesn't write into depth but has depth test enabled, doesn't ignore SSAO
        Transparent         = 3,        // transparent geometry; sorted by distance, alpha-blends into color, doesn't write into depth but has depth test enabled, ignores SSAO
        MaxValue
    };

    enum class vaPrimitiveTopology : int32
    {
        PointList,
        LineList,
        TriangleList,
        TriangleStrip,
    };

    // don't change these enum values - they are expected to be what they're set to.
    enum class vaComparisonFunc : int32
    {
        Never           = 1,
        Less            = 2,
        Equal           = 3,
        LessEqual       = 4,
        Greater         = 5,
        NotEqual        = 6,
        GreaterEqual    = 7,
        Always          = 8
    };

    enum class vaFillMode : int32
    {
        Wireframe       = 2,
        Solid           = 3
    };

    // analogous to D3D12_SHADING_RATE
    enum class vaShadingRate : int32
    {
        ShadingRate1X1  = 0,
        ShadingRate1X2  = 0x1,
        ShadingRate2X1  = 0x4,
        ShadingRate2X2  = 0x5,
        ShadingRate2X4  = 0x6,
        ShadingRate4X2  = 0x9,
        ShadingRate4X4  = 0xa
    };

    // Some of the predefined sampler types - defined in vaStandardSamplers.hlsl shader include on the shader side
    enum class vaStandardSamplerType : int32
    {
        // TODO: maybe implement mirror but beware of messing up existing data files; also need to update global samplers...
        PointClamp,
        PointWrap,
        //PointMirror,
        LinearClamp,
        LinearWrap,
        //LinearMirror,
        AnisotropicClamp,
        AnisotropicWrap,
        //AnisotropicMirror,

        MaxValue
    };


    enum class vaDrawResultFlags : uint32
    {
        None                    = 0,            // means all ok
        UnspecifiedError        = ( 1 << 0 ),
        ShadersStillCompiling   = ( 1 << 1 ),
        AssetsStillLoading      = ( 1 << 2 )
    };
    BITFLAG_ENUM_CLASS_HELPER( vaDrawResultFlags );

    // Used for more complex rendering when there's camera, lighting, various other settings - not needed by many systems
    struct vaSceneDrawContext
    {
        struct GlobalSettings
        {
            vaVector3                   WorldBase               = vaVector3( 0.0f, 0.0f, 0.0f );    // global world position offset for shading; used to make all shading computation close(r) to (0,0,0) for precision purposes
            vaVector2                   PixelScale              = vaVector2( 1.0f, 1.0f );          // global pixel scaling for various effect that depend on pixel size - used to maintain consistency when supersampling using higher resolution (and effectively smaller pixel)
            float                       MIPOffset               = 0.0f;                             // global texture mip offset for those subsystems that support it
            float                       SpecialEmissiveScale    = 1.0f;                             // special emissive is used for materials that directly output point light's brightness if below 'radius'; this feature is not wanted in some cases to avoid duplicating the light emission (such as when drawing into environment maps)
            bool                        DisableGI               = false;              
        };

        vaSceneDrawContext( vaRenderDeviceContext & deviceContext, const vaCameraBase & camera, vaDrawContextOutputType outputType, vaDrawContextFlags renderFlags = vaDrawContextFlags::None, vaLighting * lighting = nullptr, const GlobalSettings & settings = GlobalSettings() )
            : Camera( camera ), RenderDeviceContext( deviceContext ), OutputType( outputType ), RenderFlags( renderFlags ), Lighting( lighting ), Settings( settings ) { }

        // Mandatory
        vaRenderDeviceContext &         RenderDeviceContext;    // vaRenderDeviceContext is used to get/set current render targets and access rendering API stuff like contexts, etc.
        const vaCameraBase &            Camera;                 // Currently selected camera

        // Optional/settings
        vaDrawContextOutputType         OutputType;
        vaDrawContextFlags              RenderFlags;
        vaLighting *                    Lighting;
        //vaVector4                       ViewspaceDepthOffsets   = vaVector4( 0.0f, 0.0f, 0.0f, 0.0f );      // used for depth bias for shadow maps and similar; .xy are flat, .zw are slope based; .xz are absolute (world space) and .zw are scaled by camera-to-vertex distance
        GlobalSettings                  Settings;
    };

    // This is currently only used internally when/if vaSceneDrawContext is used during BeginItems; this is where all constants, SRVs, UAVs, whatnot get
    // set from vaSceneDrawContext and vaRenderGlobals and etc; 
    // In the future, if there's need to manually fill these it should be trivial to add BeginItems variant only taking vaShaderItemGlobals arguments!
    // In the future, this might be where viewport and render targets get set as well. This would simplify resource transitions.
    struct vaShaderItemGlobals
    {
        static const uint32                 ShaderResourceViewsShaderSlotBase   = SHADERGLOBAL_SRV_SLOT_BASE;
        static const uint32                 ConstantBuffersShaderSlotBase       = SHADERGLOBAL_CBV_SLOT_BASE;
        static const uint32                 UnorderedAccessViewsShaderSlotBase  = SHADERGLOBAL_UAV_SLOT_BASE;

        // SRVs
        shared_ptr<vaShaderResource>        ShaderResourceViews[SHADERGLOBAL_SRV_SLOT_COUNT];

        // CONSTANT BUFFERS
        shared_ptr<vaConstantBuffer>        ConstantBuffers[SHADERGLOBAL_CBV_SLOT_COUNT];

        // UAVs (this doesn't actually work yet due to mismatch with API support - will work once the viewport and render targets are set through this too)
        shared_ptr<vaShaderResource>        UnorderedAccessViews[SHADERGLOBAL_UAV_SLOT_COUNT];       // I did not add provision for hidden counter resets - if needed better use a different approach (InterlockedX on a separate untyped UAV)
    };

    // This is a platform-independent layer for -immediate- rendering of a single draw call - it's not fully featured or designed
    // for high performance; for additional features there's a provision for API-dependent custom callbacks; for more 
    // performance/reducing overhead the alternative is to provide API-specific rendering module implementations.
    struct vaGraphicsItem   // todo: maybe rename to vaShaderGraphicsItem?
    {
        enum class DrawType : uint8
        {
            DrawSimple,                   // Draw non-indexed, non-instanced primitives.
            // DrawAuto,                       // Draw geometry of an unknown size.
            DrawIndexed,                    // Draw indexed, non-instanced primitives.
            // DrawIndexedInstanced,           // Draw indexed, instanced primitives.
            // DrawIndexedInstancedIndirect,   // Draw indexed, instanced, GPU-generated primitives.
            // DrawInstanced,                  // Draw non-indexed, instanced primitives.
            // DrawInstancedIndirect,          //  Draw instanced, GPU-generated primitives.
        };

        DrawType                            DrawType                = DrawType::DrawSimple;

        // TOPOLOGY
        vaPrimitiveTopology                 Topology                = vaPrimitiveTopology::TriangleList;

        // BLENDING 
        vaBlendMode                         BlendMode               = vaBlendMode::Opaque;
        //vaVector4                           BlendFactor;
        //uint32                              BlendMask;

        // DEPTH
        bool                                DepthEnable             = false;
        bool                                DepthWriteEnable        = false;
        vaComparisonFunc                    DepthFunc               = vaComparisonFunc::Always;

        // STENCIL
        // <to add>

        // FILLMODE
        vaFillMode                          FillMode                = vaFillMode::Solid;

        // CULLMODE
        vaFaceCull                          CullMode                = vaFaceCull::Back;

        // MISC RASTERIZER
        bool                                FrontCounterClockwise   = false;
        // bool                                MultisampleEnable       = true;
        // bool                                ScissorEnable           = true;

        // Check vaRenderDevice::GetCapabilities().VariableShadingRateTier1 for support; if not supported this parameter is just ignored.
        vaShadingRate                       ShadingRate             = vaShadingRate::ShadingRate1X1;    // VRS 1x1 mean just normal shading

        // SHADER(s)
        shared_ptr<vaVertexShader>          VertexShader;
        shared_ptr<vaGeometryShader>        GeometryShader;
        shared_ptr<vaHullShader>            HullShader;
        shared_ptr<vaDomainShader>          DomainShader;
        shared_ptr<vaPixelShader>           PixelShader;

        // SAMPLERS
        // not handled by API independent layer yet but default ones set with SetStandardSamplers - see SHADERGLOBAL_SHADOWCMP_SAMPLERSLOT and SHADERGLOBAL_POINTCLAMP_SAMPLERSLOT and others

        // CONSTANT BUFFERS
        shared_ptr<vaConstantBuffer>        ConstantBuffers[8];

        // SRVs
        shared_ptr<vaShaderResource>        ShaderResourceViews[20];

        // VERTICES/INDICES
        shared_ptr<vaVertexBuffer>          VertexBuffer;
        uint                                VertexBufferByteStride  = 0;    // 0 means pick up stride from vaVertexBuffer itself
        uint                                VertexBufferByteOffset  = 0;
        shared_ptr<vaIndexBuffer>           IndexBuffer;

        struct DrawSimpleParams
        {
            uint32                              VertexCount         = 0;    // (DrawSimple only) Number of vertices to draw.
            uint32                              StartVertexLocation = 0;    // (DrawSimple only) Index of the first vertex, which is usually an offset in a vertex buffer.
        }                                   DrawSimpleParams;
        struct DrawIndexedParams
        {
            uint32                              IndexCount          = 0;    // (DrawIndexed only) Number of indices to draw.
            uint32                              StartIndexLocation  = 0;    // (DrawIndexed only) The location of the first index read by the GPU from the index buffer.
            int32                               BaseVertexLocation  = 0;    // (DrawIndexed only) A value added to each index before reading a vertex from the vertex buffer.
        }                                   DrawIndexedParams;
        
        // Callback to insert any API-specific overrides or additional tweaks
        std::function<bool( const vaGraphicsItem &, vaRenderDeviceContext & )> PreDrawHook;
        std::function<void( const vaGraphicsItem &, vaRenderDeviceContext & )> PostDrawHook;

        // Helpers
        void                                SetDrawSimple( int vertexCount, int startVertexLocation )                           { this->DrawType = DrawType::DrawSimple; DrawSimpleParams.VertexCount = vertexCount; DrawSimpleParams.StartVertexLocation = startVertexLocation; }
        void                                SetDrawIndexed( uint indexCount, uint startIndexLocation, int baseVertexLocation )  { this->DrawType = DrawType::DrawIndexed; DrawIndexedParams.IndexCount = indexCount; DrawIndexedParams.StartIndexLocation = startIndexLocation; DrawIndexedParams.BaseVertexLocation = baseVertexLocation; }
    };

    struct vaComputeItem   // todo: maybe rename to vaShaderGraphicsItem?
    {
        enum ComputeType
        {
            Dispatch,
            DispatchIndirect,
        };

        ComputeType                         ComputeType             = Dispatch;

        shared_ptr<vaComputeShader>         ComputeShader;

        // CONSTANT BUFFERS
        shared_ptr<vaConstantBuffer>        ConstantBuffers[_countof(vaGraphicsItem::ConstantBuffers)];           // keep the same count as in for render items for convenience and debugging safety

        // SRVs
        shared_ptr<vaShaderResource>        ShaderResourceViews[_countof(vaGraphicsItem::ShaderResourceViews)];   // keep the same count as in for render items for convenience and debugging safety

        // UAVs
        shared_ptr<vaShaderResource>        UnorderedAccessViews[8];            // I did not add provision for hidden counter resets - if needed better use a different approach (InterlockedX on a separate untyped UAV)

        // these do nothing in DX11 (it's implied but can be avoided with custom IHV extensions I think) but in DX12 they
        // add a "commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV( nullptr ) )"
        bool                                GlobalUAVBarrierBefore  = true;     // using defaults (true) can obviously be highly inefficient but it's a safe
        bool                                GlobalUAVBarrierAfter   = true;     // using defaults (true) can obviously be highly inefficient but it's a safe

        struct DispatchParams
        {
            uint32                              ThreadGroupCountX = 0;
            uint32                              ThreadGroupCountY = 0;
            uint32                              ThreadGroupCountZ = 0;
        }                                   DispatchParams;

        struct DispatchIndirectParams
        {
            shared_ptr<vaShaderResource>        BufferForArgs;
            uint32                              AlignedOffsetForArgs = 0;
        }                                   DispatchIndirectParams;

        // Callback to insert any API-specific overrides or additional tweaks
        std::function<bool( const vaComputeItem &, vaRenderDeviceContext & )> PreComputeHook;

        // Helpers
        void                                SetDispatch( uint32 threadGroupCountX, uint32 threadGroupCountY, uint32 threadGroupCountZ )     { this->ComputeType = Dispatch; DispatchParams.ThreadGroupCountX = threadGroupCountX; DispatchParams.ThreadGroupCountY = threadGroupCountY; DispatchParams.ThreadGroupCountZ = threadGroupCountZ; }
        void                                SetDispatchIndirect( shared_ptr<vaShaderResource> bufferForArgs, uint32 alignedOffsetForArgs )  { this->ComputeType = DispatchIndirect; DispatchIndirectParams.BufferForArgs = bufferForArgs; DispatchIndirectParams.AlignedOffsetForArgs = alignedOffsetForArgs; }
    };
    

    class vaShadowmap;
    class vaRenderMeshDrawList;
    struct vaIBLProbeData;

    // Ccontains various things that can get collected for rendering; it should be able (but is not guaranteed yet heh) to accept contents
    // from multiple scenes or from outside of scenes.
    // It only supports vaRenderMeshDrawList as the container for now, so only meshes. But other things could be added in theory if it's
    // not suitable to support them through the render mesh interface itself.
    struct vaRenderSelection
    {
        // contains culling and sorting information that should be honored when filling up the vaRenderSelection
        struct FilterSettings
        {
            vaBoundingSphere                    BoundingSphereFrom      = vaBoundingSphere::Degenerate; //( { 0, 0, 0 }, 0.0f );
            vaBoundingSphere                    BoundingSphereTo        = vaBoundingSphere::Degenerate; //( { 0, 0, 0 }, 0.0f );
            vector<vaPlane>                     FrustumPlanes;

            FilterSettings( ) { }

            // settings for frustum culling for a regular draw based on a given camera
            static FilterSettings               FrustumCull( const vaCameraBase & camera ) { FilterSettings ret; ret.FrustumPlanes.resize( 6 ); camera.CalcFrustumPlanes( &ret.FrustumPlanes[0] ); return ret; }
            static FilterSettings               ShadowmapCull( const vaShadowmap & shadowmap );
            static FilterSettings               EnvironmentProbeCull( const vaIBLProbeData & probeData );
        };

        struct SortSettings
        {
            // All materials of type vaLayerMode::Decal are a special case and always sorted before any others, and sorted by their 'decal order', ignoring any other sort references.

            vaVector3                           ReferencePoint          = vaVector3( std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() );
            bool                                SortByDistanceToPoint   = false;        // useful for cubemaps and etc; probably not fully correct for transparencies? (would need sorting by distance to the plane?)
            bool                                FrontToBack             = true;         // front to back for opaque/depth pre-pass, back to front for transparencies is usual

            bool                                SortByVRSType           = false;        // this will force sorting by shading rate first

            inline bool operator == ( const SortSettings & other ) const    { return 0 == memcmp( this, &other, sizeof( SortSettings ) ); }
            inline bool operator != ( const SortSettings & other ) const    { return !(*this==other); }

            static SortSettings                 Standard( bool sortByVRSType ) { SortSettings ret; ret.SortByVRSType = sortByVRSType; return ret; }
            static SortSettings                 Standard( const vaCameraBase & camera, bool frontToBack, bool sortByVRSType ) { SortSettings ret; ret.SortByDistanceToPoint = true; ret.ReferencePoint = camera.GetPosition(); ret.FrontToBack = frontToBack; ret.SortByVRSType = sortByVRSType; return ret; }
        };
        
        const shared_ptr<vaRenderMeshDrawList>  MeshList                = std::make_shared<vaRenderMeshDrawList>();


        void                                    Reset( );
    };

    // base type for forwarding vaRenderingModule constructor parameters
    struct vaRenderingModuleParams
    {
        vaRenderDevice & RenderDevice;
        vaRenderingModuleParams( vaRenderDevice & device ) : RenderDevice( device ) { }
        virtual ~vaRenderingModuleParams( ) { }
    };

    class vaRenderingModuleRegistrar : public vaSingletonBase < vaRenderingModuleRegistrar >
    {
        friend class vaRenderDevice;
        friend class vaCore;
    private:
        vaRenderingModuleRegistrar( )   { }
        ~vaRenderingModuleRegistrar( )  { }

        struct ModuleInfo
        {
            std::function< vaRenderingModule * ( const vaRenderingModuleParams & )> 
                                                ModuleCreateFunction;

            explicit ModuleInfo( const std::function< vaRenderingModule * ( const vaRenderingModuleParams & )> & moduleCreateFunction )
                : ModuleCreateFunction( moduleCreateFunction ) { }
        };

        std::map< std::string, ModuleInfo >     m_modules;
        std::recursive_mutex                    m_mutex;

    public:
        static void                             RegisterModule( const std::string & deviceTypeName, const std::string & name, std::function< vaRenderingModule * ( const vaRenderingModuleParams & )> moduleCreateFunction );
        static vaRenderingModule *              CreateModule( const std::string & deviceTypeName, const std::string & name, const vaRenderingModuleParams & params );
        //void                                  CreateModuleArray( int inCount, vaRenderingModule * outArray[] );

        template< typename ModuleType >
        static ModuleType *                     CreateModuleTyped( const std::string & name, const vaRenderingModuleParams & params );

        template< typename ModuleType >
        static ModuleType *                     CreateModuleTyped( const std::string & name, vaRenderDevice & deviceObject );


//    private:
//        static void                             CreateSingletonIfNotCreated( );
//        static void                             DeleteSingleton( );
    };

    class vaRenderingModule
    {
        string                              m_renderingModuleTypeName;

    protected:
        friend class vaRenderingModuleRegistrar;
        friend class vaRenderingModuleAPIImplementation;

        vaRenderDevice &                    m_renderDevice;

    protected:
        vaRenderingModule( const vaRenderingModuleParams & params ) : m_renderDevice( params.RenderDevice )   {  }
                                            
//                                            vaRenderingModule( const char * renderingCounterpartID );
    public:
        virtual                             ~vaRenderingModule( )                                                       { }

    private:
        // called only by vaRenderingModuleRegistrar::CreateModule
        void                                InternalRenderingModuleSetTypeName( const string & name )                   { m_renderingModuleTypeName = name; }

    public:
        const char *                        GetRenderingModuleTypeName( )                                               { return m_renderingModuleTypeName.c_str(); }
        vaRenderDevice &                    GetRenderDevice( )                                                          { return m_renderDevice; }
        vaRenderDevice &                    GetRenderDevice( ) const                                                    { return m_renderDevice; }

    public:
        template< typename CastToType >
        CastToType                          SafeCast( )                                                                 
        {
//            CastToType ret = dynamic_cast< CastToType >( this );
//            assert( ret != NULL );
//            return ret;
            return static_cast< CastToType >( this );
        }

    };

    template< typename DeviceType, typename ModuleType, typename ModuleAPIImplementationType >
    class vaRenderingModuleAutoRegister
    {
    public:
        explicit vaRenderingModuleAutoRegister( )
        {
            vaRenderingModuleRegistrar::RegisterModule( typeid(DeviceType).name(), typeid(ModuleType).name(), &CreateNew );
        }
        ~vaRenderingModuleAutoRegister( ) { }

    private:
        static inline vaRenderingModule *                   CreateNew( const vaRenderingModuleParams & params )
        { 
            return static_cast<vaRenderingModule*>( new ModuleAPIImplementationType( params ) ); 
        }
    };

    // to be placed at the end of the API counterpart module C++ file
    // example: 
    //      VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, Tree, TreeDX11 );
#define VA_RENDERING_MODULE_REGISTER( DeviceType, ModuleType, ModuleAPIImplementationType )                                                    \
        vaRenderingModuleAutoRegister< DeviceType, ModuleType, ModuleAPIImplementationType > autoReg##ModuleType_##ModuleAPIImplementationType;     \

// A helper for implementing vaRenderingModule and vaRenderingModuleAPIImplementation 
#define     VA_RENDERING_MODULE_MAKE_FRIENDS( )                                                                               \
    template< typename DeviceType, typename ModuleType, typename ModuleAPIImplementationType > friend class vaRenderingModuleAutoRegister;       \
    friend class vaRenderingModuleRegistrar;                                                                                            

    // A helper used to create rendering module and its counterpart at runtime
    // use example: 
    //      Tree * newTree = VA_RENDERING_MODULE_CREATE( Tree );
    // in addition, parameters can be passed through a pointer to vaRenderingModuleParams object; for example:
    //      vaTextureConstructorParams params( vaCore::GUIDCreate( ) );
    //      vaTexture * texture = VA_RENDERING_MODULE_CREATE( vaTexture, &params );
#define VA_RENDERING_MODULE_CREATE( ModuleType, Param )         vaRenderingModuleRegistrar::CreateModuleTyped<ModuleType>( typeid(ModuleType).name(), Param )
#define VA_RENDERING_MODULE_CREATE_SHARED( ModuleType, Param )  std::shared_ptr< ModuleType >( vaRenderingModuleRegistrar::CreateModuleTyped<ModuleType>( typeid(ModuleType).name(), Param ) )

    template< typename ModuleType >
    inline ModuleType * vaRenderingModuleRegistrar::CreateModuleTyped( const std::string & name, const vaRenderingModuleParams & params )
    {
        ModuleType * ret = NULL;
        vaRenderingModule * createdModule;
        createdModule = CreateModule( typeid(params.RenderDevice).name(), name, params );

        ret = dynamic_cast<ModuleType*>( createdModule );
        if( ret == NULL )
        {
            wstring wname = vaStringTools::SimpleWiden( name );
            wstring wtypename = vaStringTools::SimpleWiden( typeid(params.RenderDevice).name() );
            VA_WARN( L"vaRenderingModuleRegistrar::CreateModuleTyped failed for '%s'; have you done VA_RENDERING_MODULE_REGISTER( %s, %s, your_type )? ", wname.c_str(), wtypename.c_str(), wname.c_str() );
        }

        return ret;
    }

    template< typename ModuleType >
    inline ModuleType * vaRenderingModuleRegistrar::CreateModuleTyped( const std::string & name, vaRenderDevice & deviceObject )
    {
        ModuleType * ret = NULL;
        vaRenderingModule * createdModule;
        vaRenderingModuleParams tempParams( deviceObject );
        createdModule = CreateModule( typeid(tempParams.RenderDevice).name(), name, tempParams );

        ret = dynamic_cast<ModuleType*>( createdModule );
        if( ret == NULL )
        {
            wstring wname = vaStringTools::SimpleWiden( name );
            VA_ERROR( L"vaRenderingModuleRegistrar::CreateModuleTyped failed for '%s'; have you done VA_RENDERING_MODULE_REGISTER( vaRenderDeviceDX11, vaSomeClass, vaSomeClassDX11 )?; is vaSomeClass inheriting vaRenderingModule with 'public'? ", wname.c_str() );
        }

        return ret;
    }


    // TODO: upgrade to variadic template when you figure out how :P
    template< typename T >
    class vaAutoRenderingModuleInstance
    {
        shared_ptr<T> const   m_instance;

    public:
        vaAutoRenderingModuleInstance( const vaRenderingModuleParams & params ) : m_instance( shared_ptr<T>( vaRenderingModuleRegistrar::CreateModuleTyped<T>( typeid(T).name(), params ) ) ) { }
        vaAutoRenderingModuleInstance( vaRenderDevice & device )                : m_instance( shared_ptr<T>( vaRenderingModuleRegistrar::CreateModuleTyped<T>( typeid(T).name(), device ) ) ) { }
        ~vaAutoRenderingModuleInstance()    { }

        T & operator*( ) const
        {	// return reference to object
            assert( m_instance != nullptr ); return ( *m_instance );
        }

        const shared_ptr<T> & operator->( ) const
        {	// return pointer to class object
            assert( m_instance != nullptr ); return m_instance;
        }

        const shared_ptr<T> & get( ) const
        {	// return pointer to class object
            assert( m_instance != nullptr ); return m_instance;
        }

        operator const shared_ptr<T> & ( ) const
        {	// return pointer to class object
            assert( m_instance != nullptr ); return m_instance;
        }

        void destroy( )
        {
            m_instance = nullptr;
        }
    };

    template< class _Ty >
    using vaAutoRMI = vaAutoRenderingModuleInstance< _Ty >;

    // these should go into a separate header (vaAsset.h?)
    struct vaAsset;
    enum class vaAssetType : int32
    {
        Texture,
        RenderMesh,
        RenderMaterial,

        MaxVal
    };
    class vaAssetResource : public std::enable_shared_from_this<vaAssetResource>, public vaUIDObject
    {
    private:
        vaAsset *           m_parentAsset;

        int64               m_UI_ShowSelectedFrameIndex   = -1;
    
    protected:
        vaAssetResource( const vaGUID & uid ) : vaUIDObject( uid )                  { m_parentAsset = nullptr; }
        virtual ~vaAssetResource( )                                                 { assert( m_parentAsset == nullptr ); }
    
    public:
        vaAsset *           GetParentAsset( )                                       { return m_parentAsset; }
        template< typename AssetResourceType >
        AssetResourceType*  GetParentAsset( )                                       { return dynamic_cast<AssetResourceType*>( m_parentAsset ); }

        virtual vaAssetType GetAssetType( ) const                                   = 0;

        virtual bool        LoadAPACK( vaStream & inStream )                               = 0;
        virtual bool        SaveAPACK( vaStream & outStream )                                                       = 0;
        virtual bool        SerializeUnpacked( vaXMLSerializer & serializer, const wstring & assetFolder )          = 0;

        // just for UI display
        int64               GetUIShowSelectedFrameIndex( ) const                    { return m_UI_ShowSelectedFrameIndex; }
        void                SetUIShowSelectedFrameIndex( int64 showSelectedFrame )  { m_UI_ShowSelectedFrameIndex = showSelectedFrame; }

        virtual bool        UIPropertiesDraw( vaApplicationBase & application )     = 0;

    private:
        friend struct vaAsset;
        void                SetParentAsset( vaAsset * asset )         
        { 
            // there can be only one asset resource linked to one asset; this is one (of the) way to verify it:
            if( asset == nullptr )
            {
                assert( m_parentAsset != nullptr );
            }
            else
            {
                assert( m_parentAsset == nullptr );
            }
            m_parentAsset = asset; 
        }

    public:
        virtual void        RegisterUsedAssetPacks( std::function<void( const vaAssetPack & )> registerFunction );
    };

    inline vaVector2 vaShadingRateToVector2( vaShadingRate shadingRate )
    {
        vaVector2 ShadingRate = { 0,0 };
        switch( shadingRate )
        {
        case vaShadingRate::ShadingRate1X1: ShadingRate.x = 1; ShadingRate.y = 1;   break;
        case vaShadingRate::ShadingRate1X2: ShadingRate.x = 1; ShadingRate.y = 2;   break;
        case vaShadingRate::ShadingRate2X1: ShadingRate.x = 2; ShadingRate.y = 1;   break;
        case vaShadingRate::ShadingRate2X2: ShadingRate.x = 2; ShadingRate.y = 2;   break;
        case vaShadingRate::ShadingRate2X4: ShadingRate.x = 2; ShadingRate.y = 4;   break;
        case vaShadingRate::ShadingRate4X2: ShadingRate.x = 4; ShadingRate.y = 2;   break;
        case vaShadingRate::ShadingRate4X4: ShadingRate.x = 4; ShadingRate.y = 4;   break;
        default: assert( false ); break;
        }
        return ShadingRate;
    }

    inline string vaStandardSamplerTypeToShaderName( vaStandardSamplerType samplerType )
    {
        switch( samplerType )
        {
        case vaStandardSamplerType::PointClamp:         return "g_samplerPointClamp";        break;
        case vaStandardSamplerType::PointWrap:          return "g_samplerPointWrap";         break;
        case vaStandardSamplerType::LinearClamp:        return "g_samplerLinearClamp";       break;
        case vaStandardSamplerType::LinearWrap:         return "g_samplerLinearWrap";        break;
        case vaStandardSamplerType::AnisotropicClamp:   return "g_samplerAnisotropicClamp";  break;
        case vaStandardSamplerType::AnisotropicWrap:    return "g_samplerAnisotropicWrap";   break;
        default: assert( false ); return "g_samplerPointClamp"; break;
        }
    }

    inline string vaStandardSamplerTypeToUIName( vaStandardSamplerType samplerType )
    {
        switch( samplerType )
        {
        case vaStandardSamplerType::PointClamp:         return "PointClamp";        break;
        case vaStandardSamplerType::PointWrap:          return "PointWrap";         break;
        case vaStandardSamplerType::LinearClamp:        return "LinearClamp";       break;
        case vaStandardSamplerType::LinearWrap:         return "LinearWrap";        break;
        case vaStandardSamplerType::AnisotropicClamp:   return "AnisotropicClamp";  break;
        case vaStandardSamplerType::AnisotropicWrap:    return "AnisotropicWrap";   break;
        default: assert( false ); return "error"; break;
        }
    }

    inline string vaLayerModeToUIName( vaLayerMode value )
    {
        switch( value )
        {
        case vaLayerMode::Opaque:           return "Opaque";        break;
        case vaLayerMode::AlphaTest:        return "AlphaTest";     break;
        case vaLayerMode::Decal:            return "Decal";         break;
        case vaLayerMode::Transparent:      return "Transparent";   break;
        default: assert( false ); return "error"; break;
        }
    }

    
}
