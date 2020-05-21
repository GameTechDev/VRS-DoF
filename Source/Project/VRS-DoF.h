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

#include "vaApplicationWin.h"

#include "Scene/vaCameraControllers.h"
#include "Scene/vaScene.h"

#include "Rendering/vaRenderingIncludes.h"

#include "Core/vaUI.h"

#include "Core/Misc/vaMiniScript.h"
#include "Core/System/vaMemoryStream.h"

#include "Rendering/vaRenderDevice.h"
#include "Rendering/vaRenderCamera.h"
#include "Rendering/vaGBuffer.h"
#include "Rendering/vaDebugCanvas.h"
#include "Rendering/Effects/vaSkybox.h"
#include "Rendering/Effects/vaASSAOLite.h"
#include "Rendering/Effects/vaCMAA2.h"
#include "Rendering/Effects/vaDepthOfField.h"
#include "Rendering/Effects/vaPostProcessTonemap.h"
#include "Rendering/Misc/vaZoomTool.h"
#include "Rendering/Misc/vaImageCompareTool.h"

#include <optional>

namespace Vanilla
{
    class AutoBenchTool;

    class VanillaSample : public vaRenderingModule, public std::enable_shared_from_this<VanillaSample>, public vaUIPanel
    {
    public:

    public:

        enum class SceneSelectionType : int32
        {
            LumberyardBistro,
            //MinecraftLostEmpire,
            
            MaxValue
        };

        enum class VariableRateShadingType : int32
        {
            None,
            Tier1_Static_1x2_2x1,
            Tier1_Static_2x2,
            Tier1_Static_2x4_4x2,
            Tier1_Static_4x4,
            Tier1_DoF_Driven,
            //VRS_Tier2_DoF_Driven,

            MaxValue
        };

        enum class SuperSamplingType : int32
        {
            None,
            SS2x,
            SS4x
        };

        struct VanillaSampleSettings
        {
            bool                                    ShowWireframe                   = false;
            float                                   CameraYFov                      = 50.0f / 180.0f * VA_PIf;
            VariableRateShadingType                 VariableRateShadingOption       = VariableRateShadingType::Tier1_DoF_Driven;
            SuperSamplingType                       SuperSamplingOption             = SuperSamplingType::None;
            int                                     ShadingComplexity               = 2;                        // 0 == low, 1 == medium, 2 == high
            bool                                    DisableTransparencies           = false;
            
            bool                                    SortByVRS                       = true;
            bool                                    VisualizeVRS                    = false;
            float                                   DoFDrivenVRSTransitionOffset    = -0.1f;
            int                                     DoFDrivenVRSMaxRate             = 3;                        // 0 - no VRS; 1 - max is 2x1; 2 - max is 2x2; 3 - max is 4x2; 4 - max is 4x4;

            // just a quick hacky way to set up DoF - not physically correct; 
            // (maybe switch to http://www.dofmaster.com/equations.html / https://www.photopills.com/calculators/dof in the future)
            bool                                    EnableDOF                       = true;
            float                                   DoFFocalLength                  = 3.50f;
            float                                   DoFRange                        = 0.25f;

            bool                                    EnableGradientFilterExtension   = false;

            float                                   AutoVRSRateOffsetThreshold      = 0.2f;

            void Serialize( vaXMLSerializer & serializer )
            {
                serializer.Serialize( "ShowWireframe"                   , ShowWireframe                     );
                serializer.Serialize( "CameraYFov"                      , CameraYFov                        );
                serializer.Serialize( "VariableRateShadingOption"       , (int&)VariableRateShadingOption   );
                serializer.Serialize( "SuperSamplingOption"             , (int&)SuperSamplingOption         );
                serializer.Serialize( "ShadingComplexity"               , (int&)ShadingComplexity           );
                serializer.Serialize( "DisableTransparencies"           , DisableTransparencies             );
                serializer.Serialize( "EnableDOF"                       , EnableDOF                         );
                serializer.Serialize( "SortByVRS"                       , SortByVRS                         );
                serializer.Serialize( "VisualizeVRS"                    , VisualizeVRS                      );
                serializer.Serialize( "DoFDrivenVRSTransitionOffset"    , DoFDrivenVRSTransitionOffset      );
                serializer.Serialize( "DoFDrivenVRSMaxRate"             , DoFDrivenVRSMaxRate               );
                serializer.Serialize( "DoFFocalLength"                  , DoFFocalLength                    );
                serializer.Serialize( "DoFRange"                        , DoFRange                          );
                serializer.Serialize( "EnableGradientFilterExtension"   , EnableGradientFilterExtension     );
                serializer.Serialize( "AutoVRSRateOffsetThreshold"      , AutoVRSRateOffsetThreshold        );

                // this here is just to remind you to update serialization when changing the struct
                size_t dbgSizeOfThis = sizeof(*this); dbgSizeOfThis;
                assert( dbgSizeOfThis == 52 );
            }

            void Validate( )
            {
                CameraYFov                      = vaMath::Clamp( CameraYFov, 15.0f / 180.0f * VA_PIf, 130.0f / 180.0f * VA_PIf );
                VariableRateShadingOption           = (VariableRateShadingType)vaMath::Clamp( (int)VariableRateShadingOption, (int)VariableRateShadingType::None, (int)VariableRateShadingType::MaxValue-1 );
                ShadingComplexity               = vaMath::Clamp( ShadingComplexity, 0, 2 );
                DoFDrivenVRSTransitionOffset    = vaMath::Clamp( DoFDrivenVRSTransitionOffset, -1.0f, 1.0f );
                DoFDrivenVRSMaxRate             = vaMath::Clamp( DoFDrivenVRSMaxRate, 0, 4 );
                DoFFocalLength                  = vaMath::Clamp( DoFFocalLength,    0.0f, 100.0f );
                DoFRange                        = vaMath::Clamp( DoFRange,          0.0f, 1.0f );
            }
        };

    protected:
        shared_ptr<vaRenderCamera>              m_camera;
        shared_ptr<vaCameraControllerFreeFlight>
                                                m_cameraFreeFlightController;
        shared_ptr<vaCameraControllerFlythrough>
                                                m_cameraFlythroughController;
        bool                                    m_cameraFlythroughPlay = false;

        vaApplicationBase &                     m_application;

        shared_ptr<vaSkybox>                    m_skybox;

        vaGBuffer::BufferFormats                m_GBufferFormats;
        shared_ptr<vaGBuffer>                   m_GBuffer;

        shared_ptr<vaGBuffer>                   m_SSGBuffer;
        shared_ptr<vaTexture>                   m_SSScratchColor;
        int                                     m_SSBuffersLastUsed                     = 0;        // delete SS buffers when not in use for more than couple of frames
        
        // 0 is SuperSampleReference, 1 is SuperSampleReferenceFast
        const int                               m_SSResScale[2]                         = { 4    ,  2       };      // draw at ResScale times higher resolution (only 1 and 2 and 4 supported due to filtering support)
        int                                     m_SSGridRes[2]                          = { 4    ,  2       };      // multi-tap using GridRes x GridRes samples for each pixel
        float                                   m_SSMIPBias[2]                          = { 1.60f,  0.60f   };      // make SS sample textures from a bit lower MIPs to avoid significantly over-sharpening textures vs non-SS (for textures that have high res mip levels or at distance)
        float                                   m_SSSharpen[2]                          = { 0.0f,   0.0f    };      // used to make texture-only view (for ex. looking at the painting) closer to non-SS (as SS adds a dose of blur due to tex sampling especially when no higher-res mip available for textures, which is the case here in most cases)
        float                                   m_SSDDXDDYBias[2]                       = { 0.20f,  0.22f   };      // SS messes up with pixel size which messes up with specular as it is based on ddx/ddy so compensate a bit here (0.20 gave closest specular by PSNR diff from no-AA)
        shared_ptr<vaTexture>                   m_SSAccumulationColor;

        shared_ptr<vaTexture>                   m_exportedLuma;

        shared_ptr<vaTexture>                   m_scratchPostProcessColor;      // in some variations a postprocess step needs to make a copy during processing (SMAA for ex.) - in that case, this will contain the final output

        shared_ptr<vaLighting>                  m_lighting;
        shared_ptr<vaPostProcessTonemap>        m_postProcessTonemap;

        shared_ptr<vaScene>                     m_scenes[ (int32)SceneSelectionType::MaxValue ];

        shared_ptr<vaScene>                     m_currentScene;
        
        vaRenderSelection                       m_selectedOpaque;
        vaRenderSelection                       m_selectedTransparent;
        vaRenderSelection::SortSettings         m_sortDepthPrepass;
        vaRenderSelection::SortSettings         m_sortOpaque;
        vaRenderSelection::SortSettings         m_sortTransparent;
        
        vaDrawResultFlags                       m_currentDrawResults = vaDrawResultFlags::None;
        bool                                    m_currentSceneChanged = false;

        shared_ptr<vaShadowmap>                 m_queuedShadowmap;
        vaRenderSelection                       m_queuedShadowmapRenderSelection;
        bool                                    m_shadowsStable = false;

        bool                                    m_IBLsStable = false;

        shared_ptr<vaCMAA2>                     m_CMAA2;

        shared_ptr<vaZoomTool>                  m_zoomTool;
        shared_ptr<vaImageCompareTool>          m_imageCompareTool;

        float                                   m_lastDeltaTime;

        vaVector3                               m_mouseCursor3DWorldPosition;

        shared_ptr<vaASSAOLite>                 m_SSAO;
        shared_ptr<vaTexture>                   m_SSAOScratchBuffer;            // gets written and read by 'DrawSceneOpaque' - not really meant for use beyond that
        shared_ptr<vaTexture>                   m_SSAOScratchBufferMIP0;        // just a view view into MIP 0
        shared_ptr<vaTexture>                   m_SSAOScratchBufferMIP1;        // just a view view into MIP 1
        shared_ptr<vaDepthOfField>              m_DepthOfField;

        bool                                    m_requireDeterminism            = false;
        bool                                    m_allLoadedPrecomputedAndStable = false;    // updated at the end of each frame; will be true if all assets are loaded, shaders compiler and static shadow maps created 

        //bool                                    m_debugTexturingDisabled        = false;

        // this is hacky but better not required for now
        shared_ptr<vaIBLProbe>                  m_IBLProbeLocal;
        shared_ptr<vaIBLProbe>                  m_IBLProbeDistant;

        vaMiniScript                            m_miniScript;
        shared_ptr<vaTexture>                   m_currentFrameTexture;  // for scripting

    protected:
        VanillaSampleSettings                   m_settings;
        VanillaSampleSettings                   m_lastSettings;
        bool                                    m_globalShaderMacrosDirty = true;

    public:
        VanillaSample( vaRenderDevice & renderDevice, vaApplicationBase & applicationBase );
        virtual ~VanillaSample( );

        const vaApplicationBase &               GetApplication( ) const             { return m_application; }

    public:
        shared_ptr<vaRenderCamera> &            Camera( )                           { return m_camera; }
        VanillaSampleSettings &                 Settings( )                         { return m_settings; }
        vaDepthOfField::DoFSettings &           DoFSettings( )                      { return m_DepthOfField->Settings(); }
        shared_ptr<vaPostProcessTonemap>  &     PostProcessTonemap( )               { return m_postProcessTonemap; }
        const shared_ptr<vaImageCompareTool> &  ImageCompareTool( )                 { return m_imageCompareTool; }
        const shared_ptr<vaTexture> &           CurrentFrameTexture( )              { return m_currentFrameTexture; }

        bool                                    SetVRSOption( VariableRateShadingType type );

        const char *                            GetVRSOptionName( VariableRateShadingType type );
        bool                                    GetVRSOptionSupported( VariableRateShadingType type );

        void                                    SetRequireDeterminism( bool enable ){ m_requireDeterminism = enable; }
        bool                                    IsAllLoadedPrecomputedAndStable( )  { return m_allLoadedPrecomputedAndStable; }

        const shared_ptr<vaCameraControllerFlythrough> & 
                                                GetFlythroughCameraController()     { return m_cameraFlythroughController; }
        bool                                    GetFlythroughCameraEnabled() const  { return m_cameraFlythroughPlay; }
        void                                    SetFlythroughCameraEnabled( bool enabled ) { m_cameraFlythroughPlay = enabled; }

    public:
        // events/callbacks:
        void                                    OnBeforeStopped( );
        void                                    OnTick( float deltaTime );
        void                                    OnSerializeSettings( vaXMLSerializer & serializer );

    protected:
        vaDrawResultFlags                       RenderTick( float deltaTime );
        void                                    LoadAssetsAndScenes( );


        bool                                    LoadCamera( int index = -1 );
        void                                    SaveCamera( int index = -1 );

        vaDrawResultFlags                       DrawScene( vaRenderCamera & camera, vaGBuffer & gbufferOutput, const shared_ptr<vaTexture> & colorScratch, bool & colorScratchContainsFinal, const vaViewport & mainViewport, const vaSceneDrawContext::GlobalSettings & globalSettings = vaSceneDrawContext::GlobalSettings(), bool skipTonemapTick = false );

        vaDrawResultFlags                       DrawSceneDepthPrePass( vaRenderDeviceContext & renderContext, const vaRenderSelection & renderSelection, const vaCameraBase & camera, const shared_ptr<vaTexture> & depth, const vaSceneDrawContext::GlobalSettings & globalSettings, const vaRenderSelection::SortSettings & sortSettings );
        vaDrawResultFlags                       DrawSceneOpaque( vaRenderDeviceContext& renderContext, const vaRenderSelection & renderSelection, const vaCameraBase & camera, const shared_ptr<vaTexture> & depth, const shared_ptr<vaTexture> & color, bool sky, bool applySSAO, const vaSceneDrawContext::GlobalSettings & globalSettings, const vaRenderSelection::SortSettings & sortSettings );
        vaDrawResultFlags                       DrawScenePostOpaque( vaRenderDeviceContext& renderContext, const vaRenderSelection & renderSelection, const vaCameraBase & camera, const shared_ptr<vaTexture> & depth, const shared_ptr<vaTexture> & color, bool transparencies, const vaSceneDrawContext::GlobalSettings & globalSettings, const vaRenderSelection::SortSettings & sortSettings );

        virtual void                            UIPanelTick( vaApplicationBase & application ) override;
//        virtual string                          UIPanelGetDisplayName() const override                      { return "VRS Depth of Field demo" };

        void                                    ScriptedTests( vaApplicationBase & application ) ;

    private:
        //void                                    RandomizeCurrentPoissonDisk( int count = SSAO_MAX_SAMPLES );
    };

}