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

// References: 
//  * https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
//  * https://google.github.io/filament/Filament.html

#pragma once

#include "Core/vaCoreIncludes.h"

#include "vaRendering.h"

#include "Rendering/vaTexture.h"

#include "Rendering/Shaders/vaLightingShared.h"

#include "Core/vaUI.h"

#include "Rendering/vaRenderBuffers.h"

#include "Core/vaXMLSerialization.h"

#include "Rendering/Shaders/vaIBLShared.h"

namespace Vanilla
{
    struct vaSceneDrawContext;
    class vaRenderDeviceContext;
    class vaDebugCanvas2D;
    class vaDebugCanvas3D;


    //bool UIPanelTick( const string & uniqueID, vaIBLProbeData & probeData, vaIBLProbe::UIContext & probeUIContext, vaApplicationBase & application );

    class vaIrradianceSHCalculator : public vaRenderingModule
    {
        //uint8                                           m_maxMipLevel               = 0;
    public:
        static const int                                c_numSHBands = 3;       // note, 2 was never tested and probably not supported

    private:
        shared_ptr<vaTexture>                           m_SH;
        shared_ptr<vaTexture>                           m_SHCPUReadback;
        bool                                            m_SHComputed            = false;
        //shared_ptr<vaTexture>                           m_SHScalingK;

        shared_ptr<vaComputeShader>                     m_CSComputeSH;
        shared_ptr<vaComputeShader>                     m_CSPostProcessSH;      // windowing and precomputation for shader usage

    public:
        vaIrradianceSHCalculator( vaRenderDevice& renderDevice );
        virtual ~vaIrradianceSHCalculator( );

    public:
        void                                            ComputeSH( vaRenderDeviceContext & renderContext, shared_ptr<vaTexture> & sourceCube );
        std::array<vaVector3, 9>                        GetSH( vaRenderDeviceContext& renderContext );   // will block
        //void                                            ConvertToIrradianceCube( vaRenderDeviceContext & renderContext, vector<shared_ptr<vaTexture>> destCubeMIPLevels );

    public:
        void                                            Reset( );

    };

    class vaIBLCubemapPreFilter : public vaRenderingModule
    {
    public:
        // affect filtering performance
        static const uint32                             c_defaultSamplesPerTexel            = 1024;
        
        // affect runtime performance
        static const uint32                             c_defaultReflRoughCubeFirstMIPSize  = 256;      // quality levels: low - 128; medium 256; high 512
        static const uint32                             c_defaultReflRoughCubeLastMIPSize   = 4;        // quality levels: low - 2;   medium 4;   high 4
        static const uint32                             c_defaultIrradianceBaseCubeSize     = 32;       // 

        enum class FilterType : uint32
        {
            Unknown                 = 0,
            ReflectionsRoughness    = 1,
            Irradiance              = 2,
        };

    protected:
        struct SampleInfo   // originally called CacheEntry in '\filament\libs\ibl\src\CubemapIBL.cpp'
        {
            vaVector3   L;
            float       Weight; // "brdf_NoL"
            float       MIPLevel;

            static vaVector4 Pack( const SampleInfo& si )
            {
                return { si.L.x, si.L.y, si.Weight, (float)si.MIPLevel };
            }
            static SampleInfo Unpack( const vaVector4& psi )
            {
                SampleInfo ret;
                ret.L.x     = psi.x;
                ret.L.y     = psi.y;
                ret.L.z     = std::sqrtf( vaMath::Clamp( 1 - psi.x * psi.x - psi.y * psi.y, 0.0f, 1.0f ) );
                ret.Weight  = psi.z;
                ret.MIPLevel= psi.w;
                return ret;
            }
        };

        struct LevelInfo
        {
            std::vector<SampleInfo>                     Samples;

            shared_ptr<vaTexture>                       SamplesTexture;
            shared_ptr<vaComputeShader>                 CSCubePreFilter;

            uint32                                      CSThreadGroupSize;
            uint32                                      Size;
        };

        std::vector<LevelInfo>                          m_levels;

        uint32                                          m_outputBaseSize    = 0;
        uint32                                          m_numSamples        = 0;
        uint32                                          m_numMIPLevels      = 0;
        FilterType                                      m_filterType        = FilterType::Unknown;

    public:
        vaIBLCubemapPreFilter( vaRenderDevice& renderDevice )  : vaRenderingModule( renderDevice )             { }
        virtual ~vaIBLCubemapPreFilter( )                                                                      { }

    public:
        void                                            Init( uint32 outputBaseSize, uint32 outputMinSize, uint32 samplesPerTexel, FilterType filterType );
        void                                            Process( vaRenderDeviceContext& renderContext, vector<shared_ptr<vaTexture>> destCubeMIPLevels, shared_ptr<vaTexture> & sourceCube );
        void                                            Reset( );
    };

    typedef std::function< vaDrawResultFlags( vaRenderDeviceContext & renderContext, const vaCameraBase & faceCamera, const shared_ptr<vaTexture> & faceDepth, const shared_ptr<vaTexture> & faceColor ) >   CubeFaceCaptureCallback;

    struct vaIBLProbeData
    {
        // these define capture parameters
        vaVector3                   Position = vaVector3( 0, 0, 0 );
        //vaMatrix3x3                 Orientation     = vaMatrix3x3::Identity;
        float                       ClipNear = 0.1f;
        float                       ClipFar = 1000.0f;

        // If enabled, proxy OBB is used to define fadeout boundaries (weight) and control parallax
        vaOrientedBoundingBox       GeometryProxy = vaOrientedBoundingBox( vaVector3( 0, 0, 0 ), vaVector3( 1, 1, 1 ), vaMatrix3x3::Identity );
        bool                        UseGeometryProxy = false;

        // (ignored for global IBLs for now)
        vaOrientedBoundingBox       FadeOutProxy = vaOrientedBoundingBox( vaVector3( 0, 0, 0 ), vaVector3( 1, 1, 1 ), vaMatrix3x3::Identity );

        // gets baked into IBL
        vaVector3                   AmbientColor = vaVector3( 0, 0, 0 );
        float                       AmbientColorIntensity = 0.0f;

        // If set, probe is imported from file instead of captured from scene
        string                      ImportFilePath = "";

        // Whether it's enabled / required
        bool                        Enabled = false;

        bool                        operator == ( const vaIBLProbeData & eq ) const
        {
            return this->Position == eq.Position && this->ClipNear == eq.ClipNear && this->ClipFar == eq.ClipFar
                && this->GeometryProxy == eq.GeometryProxy && this->UseGeometryProxy == eq.UseGeometryProxy
                && this->FadeOutProxy == eq.FadeOutProxy
                && this->AmbientColor == eq.AmbientColor && this->AmbientColorIntensity == eq.AmbientColorIntensity
                && this->ImportFilePath == eq.ImportFilePath && this->Enabled == eq.Enabled;
        }
        bool                        operator != ( const vaIBLProbeData & eq ) const { return !( *this == eq ); }

        // helper stuff
        void                        SetImportFilePath( const string& importFilePath, bool updateEnabled = true );

        // static stuff
        static bool                 StaticSerialize( vaXMLSerializer& serializer, vaIBLProbeData & value );

        //static void                 UI( vaApplicationBase& application, vaIBLProbeData & value, const shared_ptr<void>& drawContext );
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // To quote Filament docs: 
    // "Distant light probes, used to capture lighting information at "infinity", where parallax can be ignored. Distant probes typically contain the sky, 
    // distant landscape features or buildings, etc. They are either captured by the engine or acquired from a camera as high dynamic range images (HDRI)."
    class vaIBLProbe : public vaRenderingModule, public vaUIPanel //, public std::enable_shared_from_this<vaIBLProbe>
    {
    public:
        struct UIContext
        {
            const weak_ptr<void>        AliveToken;

            bool                        PanelOpen = false;
            int                         GuizmoOperationType = 0;                    // translate = 0, rotate = 1, scale = 2

            UIContext( const weak_ptr<void>& aliveToken ) : AliveToken( aliveToken ) { }
        };

    protected:
        vaIBLProbeData                                  m_capturedData;
        string                                          m_fullImportedPath;

        bool                                            m_hasContents;

        //vaMatrix3x3                                     m_rotation;
        
        // The difference between reflections map and skybox texture is that reflection map has roughness-based prefiltering in mip-levels while skybox
        // has just regular box filtered mips.
        shared_ptr<vaTexture>                           m_reflectionsMap;
        int                                             m_maxReflMIPLevel;
        shared_ptr<vaTexture>                           m_irradianceMap;
        shared_ptr<vaTexture>                           m_skyboxTexture;


        std::array<vaVector3, 9>                        m_irradianceCoefs;

        float                                           m_intensity;

        // vaTypedConstantBufferWrapper< IBLProbeConstants >
        //                                                 m_constantsBuffer;

        // for importing!
        shared_ptr<vaComputeShader>                     m_CSEquirectangularToCubemap;

        shared_ptr<vaIrradianceSHCalculator>            m_irradianceSHCalculator;
        shared_ptr<vaIBLCubemapPreFilter>               m_reflectionsPreFilter;
        shared_ptr<vaIBLCubemapPreFilter>               m_irradiancePreFilter;

        vaResourceFormat                                m_skyboxFormat              = vaResourceFormat::R11G11B10_FLOAT;    // vaResourceFormat::R16G16B16A16_FLOAT
        vaResourceFormat                                m_reflectionsMapFormat      = vaResourceFormat::R11G11B10_FLOAT;    // vaResourceFormat::R16G16B16A16_FLOAT
        vaResourceFormat                                m_irradianceMapFormat       = vaResourceFormat::R11G11B10_FLOAT;    // vaResourceFormat::R16G16B16A16_FLOAT

        shared_ptr<vaTexture>                           m_cubeCaptureScratchDepth;

    public:
        vaIBLProbe( vaRenderDevice & renderDevice );
        virtual ~vaIBLProbe( );

    protected:
        friend class vaLighting;
        void                                            UpdateShaderConstants( vaSceneDrawContext & drawContext, IBLProbeConstants & outShaderConstants );
        void                                            SetToGlobals( vaShaderItemGlobals & shaderItemGlobals, bool distantIBLSlots );

    public:
        void                                            Reset( );
        bool                                            Import( vaRenderDeviceContext & renderContext, const vaIBLProbeData & captureData );
        vaDrawResultFlags                               Capture( vaRenderDeviceContext & renderContext, const vaIBLProbeData & captureData, const CubeFaceCaptureCallback & faceCaptureCallback, int cubeFaceResolution = vaIBLCubemapPreFilter::c_defaultReflRoughCubeFirstMIPSize );

        bool                                            HasContents( ) const                            { return m_hasContents; }
        const vaIBLProbeData &                          GetContentsData( ) const                        { return m_capturedData; }

        bool                                            HasSkybox( ) const                              { return m_hasContents && m_skyboxTexture != nullptr; }
        void                                            SetToSkybox( class vaSkybox & skybox );

    private:
        virtual void                                    UIPanelTick( vaApplicationBase & application ) override;

        shared_ptr<vaTexture>                           ImportCubemap( vaRenderDeviceContext & renderContext, const string & path, uint32 outputBaseSize );

        bool                                            Process( vaRenderDeviceContext & renderContext, const shared_ptr<vaTexture> & srcCube );
    };

}