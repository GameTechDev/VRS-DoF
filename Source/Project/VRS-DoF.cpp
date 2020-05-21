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

#include "VRS-DOF.h"

#include "Core/System/vaFileTools.h"
#include "Core/Misc/vaProfiler.h"

#include "Rendering/vaGPUTimer.h"

#include "Rendering/vaRenderMesh.h"
#include "Rendering/vaRenderMaterial.h"
#include "Rendering/vaAssetPack.h"
#include "Rendering/vaRenderGlobals.h"

#include "IntegratedExternals/vaImguiIntegration.h"
#include "Scene/vaAssetImporter.h"

#include <iomanip>
#include <sstream> // stringstream
#include <fstream>

using namespace Vanilla;

static void VanillaMain( vaRenderDevice & renderDevice, vaApplicationBase & application, float deltaTime, vaApplicationState applicationState )
{
    renderDevice; deltaTime;
    static shared_ptr<VanillaSample> vanillaSample;
    if( applicationState == vaApplicationState::Initializing )
    {
        vanillaSample = std::make_shared<VanillaSample>( application.GetRenderDevice( ), application );
        application.Event_Tick.Add( vanillaSample, &VanillaSample::OnTick );    // <- this 'takes over' the tick!
        application.Event_BeforeStopped.Add( vanillaSample, &VanillaSample::OnBeforeStopped );
        application.Event_SerializeSettings.Add( vanillaSample, &VanillaSample::OnSerializeSettings );
        return;
    }
    else if( applicationState == vaApplicationState::ShuttingDown )
    {
        vanillaSample = nullptr;
        return;
    }
    assert( applicationState == vaApplicationState::Running );
}

int APIENTRY _tWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow )
{
    hInstance; hPrevInstance; // unreferenced

    {
        vaCoreInitDeinit core;
        vaApplicationWin::Settings settings( L"Variable Rate Shading and Depth of Field", lpCmdLine, nCmdShow );
#ifdef _DEBUG
        settings.Vsync = false;
#else
        settings.Vsync = false;
#endif

        vaApplicationWin::Run( settings, VanillaMain );

    }
    return 0;
}


static wstring CameraFileName( int index )
{
    wstring fileName = vaCore::GetExecutableDirectory( ) + L"last";
    if( index != -1 ) 
        fileName += vaStringTools::Format( L"_%d", index );
    fileName += L".camerastate";
    return fileName;
}

VanillaSample::VanillaSample( vaRenderDevice & renderDevice, vaApplicationBase & applicationBase ) 
    : vaRenderingModule( renderDevice ), m_application( applicationBase ),
    vaUIPanel( "Sample settings", 0, true, vaUIPanel::DockLocation::DockedLeft, "", vaVector2( 500, 750 ) )
{
    m_camera = std::make_shared<vaRenderCamera>( GetRenderDevice(), true );

    m_camera->SetPosition( vaVector3( 4.3f, 29.2f, 14.2f ) );
    m_camera->SetOrientationLookAt( vaVector3( 6.5f, 0.0f, 8.7f ) );

    m_cameraFreeFlightController    = std::shared_ptr<vaCameraControllerFreeFlight>( new vaCameraControllerFreeFlight() );
    m_cameraFreeFlightController->SetMoveWhileNotCaptured( false );

    m_cameraFlythroughController    = std::make_shared<vaCameraControllerFlythrough>();
    const float keyTimeStep = 8.0f;
    float keyTime = 0.0f;
    // search for HACKY_FLYTHROUGH_RECORDER on how to 'record' these if need be 
    float defaultDoFRange = 0.25f;
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( -15.027f, -3.197f, 2.179f ),   vaQuaternion( 0.480f, 0.519f, 0.519f, 0.480f ),     keyTime,    13.5f, defaultDoFRange ) ); keyTime+=keyTimeStep;   // 0
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( -8.101f, 2.689f, 1.289f ),     vaQuaternion( 0.564f, 0.427f, 0.427f, 0.564f ),     keyTime,     3.0f, defaultDoFRange ) ); keyTime+=keyTimeStep;   // 8
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( -4.239f, 4.076f, 1.621f ),     vaQuaternion( 0.626f, 0.329f, 0.329f, 0.626f ),     keyTime,     6.0f, defaultDoFRange ) ); keyTime+=keyTimeStep;   // 16
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( 2.922f, 5.273f, 1.520f ),      vaQuaternion( 0.660f, 0.255f, 0.255f, 0.660f ),     keyTime,     3.0f, defaultDoFRange ) ); keyTime+=keyTimeStep;   // 24
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( 6.134f, 5.170f, 1.328f ),      vaQuaternion( 0.680f, 0.195f, 0.195f, 0.680f ),     keyTime,     7.0f, defaultDoFRange ) ); keyTime+=keyTimeStep;   // 32
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( 7.658f, 4.902f, 1.616f ),      vaQuaternion( 0.703f, 0.078f, 0.078f, 0.703f ),     keyTime,     6.5f, defaultDoFRange ) ); keyTime+=keyTimeStep;   // 40
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( 8.318f, 3.589f, 2.072f ),      vaQuaternion( 0.886f, -0.331f, -0.114f, 0.304f ),   keyTime,    14.0f, defaultDoFRange ) ); keyTime+=keyTimeStep;   // 48
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( 8.396f, 3.647f, 2.072f ),      vaQuaternion( 0.615f, 0.262f, 0.291f, 0.684f ),     keyTime,     3.0f, defaultDoFRange ) ); keyTime+=keyTimeStep;   // 56
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( 9.750f, 0.866f, 2.131f ),      vaQuaternion( 0.747f, -0.131f, -0.113f, 0.642f ),   keyTime,     3.0f, defaultDoFRange ) ); keyTime+=keyTimeStep;   // 64
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( 11.496f, -0.826f, 2.429f ),    vaQuaternion( 0.602f, -0.510f, -0.397f, 0.468f ),   keyTime,    10.0f, defaultDoFRange ) ); keyTime+=keyTimeStep;   // 72
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( 10.943f, -1.467f, 2.883f ),    vaQuaternion( 0.704f, 0.183f, 0.173f, 0.664f ),     keyTime,     1.2f, 1.8f*defaultDoFRange ) ); keyTime+=keyTimeStep;   // 80
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( 7.312f, -3.135f, 2.869f ),     vaQuaternion( 0.692f, 0.159f, 0.158f, 0.686f ),     keyTime,     1.5f, 2.0f*defaultDoFRange ) ); keyTime+=keyTimeStep;   // 88
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( 7.559f, -3.795f, 2.027f ),     vaQuaternion( 0.695f, 0.116f, 0.117f, 0.700f ),     keyTime,     1.0f, 1.8f*defaultDoFRange ) ); keyTime+=keyTimeStep;   // 96
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( 6.359f, -4.580f, 1.856f ),     vaQuaternion( 0.749f, -0.320f, -0.228f, 0.533f ),   keyTime,     4.0f, 1.2f*defaultDoFRange ) ); keyTime+=keyTimeStep;   // 104
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( 5.105f, -6.682f, 0.937f ),     vaQuaternion( 0.559f, -0.421f, -0.429f, 0.570f ),   keyTime,     2.0f, 1.2f*defaultDoFRange ) ); keyTime+=keyTimeStep;   // 112
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( 3.612f, -5.566f, 1.724f ),     vaQuaternion( 0.771f, -0.024f, -0.020f, 0.636f ),   keyTime,     2.0f, 1.2f*defaultDoFRange ) ); keyTime+=keyTimeStep;   // 120
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( 2.977f, -5.532f, 1.757f ),     vaQuaternion( 0.698f, -0.313f, -0.263f, 0.587f ),   keyTime,    12.0f, defaultDoFRange ) ); keyTime+=keyTimeStep;   // 128
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( 1.206f, -1.865f, 1.757f ),     vaQuaternion( 0.701f, -0.204f, -0.191f, 0.657f ),   keyTime,     2.0f, defaultDoFRange ) ); keyTime+=keyTimeStep;   // 136
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( 0.105f, -1.202f, 1.969f ),     vaQuaternion( 0.539f, 0.558f, 0.453f, 0.439f ),     keyTime,     9.0f, defaultDoFRange ) ); keyTime+=keyTimeStep;   // 144
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( -6.314f, -1.144f, 1.417f ),    vaQuaternion( 0.385f, 0.672f, 0.549f, 0.314f ),     keyTime,    13.0f, defaultDoFRange ) ); keyTime+=keyTimeStep;   // 152
    m_cameraFlythroughController->AddKey( vaCameraControllerFlythrough::Keyframe( vaVector3( -15.027f, -3.197f, 2.179f ),   vaQuaternion( 0.480f, 0.519f, 0.519f, 0.480f ), keyTime+0.01f,  13.0f, defaultDoFRange ) ); keyTime+=keyTimeStep;   // 160
    m_cameraFlythroughController->SetFixedUp( true );

    m_skybox                = VA_RENDERING_MODULE_CREATE_SHARED( vaSkybox, GetRenderDevice() );
    m_GBuffer               = VA_RENDERING_MODULE_CREATE_SHARED( vaGBuffer, GetRenderDevice() );
    m_lighting              = VA_RENDERING_MODULE_CREATE_SHARED( vaLighting, GetRenderDevice() );
    m_postProcessTonemap    = VA_RENDERING_MODULE_CREATE_SHARED( vaPostProcessTonemap, GetRenderDevice() );

    m_SSAO                  = std::make_shared<vaASSAOLite>( GetRenderDevice() );
    m_DepthOfField          = std::make_shared<vaDepthOfField>( GetRenderDevice() );

    // this is used for all frame buffer needs - color, depth, linear depth, gbuffer material stuff if used, etc.
    m_GBufferFormats = m_GBuffer->GetFormats();

    // disable, unused
    m_GBufferFormats.DepthBufferViewspaceLinear = vaResourceFormat::Unknown;
        
    // use lower precision for perf reasons - default is R16G16B16A16_FLOAT
    m_GBufferFormats.Radiance  = vaResourceFormat::R11G11B10_FLOAT;
    
#if 0 // for testing the path for HDR displays (not all codepaths will support it - for ex, SMAA requires unconverted sRGB as the input)
    m_GBufferFormats.OutputColorTypeless           = vaResourceFormat::R11G11B10_FLOAT;
    m_GBufferFormats.OutputColorView               = vaResourceFormat::R11G11B10_FLOAT;
    m_GBufferFormats.OutputColorIgnoreSRGBConvView = vaResourceFormat::R11G11B10_FLOAT;
    m_GBufferFormats.OutputColorR32UINT_UAV        = vaResourceFormat::Unknown;
#endif
#if 0 // for testing the path for HDR displays (not all codepaths will support it - for ex, SMAA requires unconverted sRGB as the input)
    m_GBufferFormats.OutputColorTypeless           = vaResourceFormat::R16G16B16A16_FLOAT;
    m_GBufferFormats.OutputColorView               = vaResourceFormat::R16G16B16A16_FLOAT;
    m_GBufferFormats.OutputColorIgnoreSRGBConvView = vaResourceFormat::R16G16B16A16_FLOAT;
    m_GBufferFormats.OutputColorR32UINT_UAV        = vaResourceFormat::Unknown;
#endif

    // camera settings
    {
        auto & generalSettings  = m_camera->GeneralSettings();
        auto & tonemapSettings  = m_camera->TonemapSettings();
        auto & bloomSettings    = m_camera->BloomSettings();
        generalSettings.ExposureCompensation        = 0.4f;     // just make everything a bit brighter for no reason
        generalSettings.UseAutoExposure             = true;
        generalSettings.AutoExposureKeyValue        = 0.5f;
        generalSettings.ExposureMax                 = 6.0f;
        generalSettings.ExposureMin                 = -10.0f;
        //generalSettings.AutoExposureAdaptationSpeed = std::numeric_limits<float>::infinity();   // for testing purposes we're setting this to infinity
        
        tonemapSettings.UseTonemapping              = true;        // for debugging using values it's easier if it's disabled
        
        bloomSettings.UseBloom                      = true;
        bloomSettings.BloomSize                     = 0.35f;
        bloomSettings.BloomMultiplier               = 0.01f;
        bloomSettings.BloomMinThreshold             = 0.001f;
        bloomSettings.BloomMaxClamp                 = 4.0f;
    }

    if( m_SSAO.get() != nullptr )
    {
        auto & ssaoSettings = m_SSAO->GetSettings();
        ssaoSettings.Radius                         = 0.5f;
        ssaoSettings.ShadowMultiplier               = 0.6f;
        ssaoSettings.ShadowPower                    = 1.5f;
        ssaoSettings.QualityLevel                   = 1;
        ssaoSettings.BlurPassCount                  = 1;
        ssaoSettings.DetailShadowStrength           = 2.5f;
    #if 0 // drop to low quality for more perf
        ssaoSettings.QualityLevel                   = 0;
        ssaoSettings.ShadowMultiplier               = 0.4f;
    #endif
    }

    {
        vaFileStream fileIn;
        if( fileIn.Open( CameraFileName(-1), FileCreationMode::Open ) )
        {
            m_camera->Load( fileIn );
        } else if( fileIn.Open( vaCore::GetExecutableDirectory( ) + L"default.camerastate", FileCreationMode::Open ) )
        {
            m_camera->Load( fileIn );
        }
    }
    m_camera->AttachController( m_cameraFreeFlightController );

    m_lastDeltaTime = 0.0f;

    m_CMAA2 = VA_RENDERING_MODULE_CREATE_SHARED( vaCMAA2, GetRenderDevice() );

    m_zoomTool = std::make_shared<vaZoomTool>( GetRenderDevice() );
    m_imageCompareTool = std::make_shared<vaImageCompareTool>(GetRenderDevice());

    // create scene objects
    for( int i = 0; i < _countof(m_scenes); i++ )
        m_scenes[i] = std::make_shared<vaScene>();

    m_currentScene = nullptr;

    m_IBLProbeLocal     = std::make_shared<vaIBLProbe>( renderDevice );
    m_IBLProbeDistant   = std::make_shared<vaIBLProbe>( renderDevice );

    m_lighting->SetLocalIBL( m_IBLProbeLocal );
    m_lighting->SetDistantIBL( m_IBLProbeDistant );

    LoadAssetsAndScenes();
}

VanillaSample::~VanillaSample( )
{ 
    GetRenderDevice().GetAssetPackManager().UnloadAllPacks();

#if 1 || defined( _DEBUG )
    SaveCamera( );
#endif
}

const char * VanillaSample::GetVRSOptionName( VariableRateShadingType type )
{
    switch( type )
    {
    case VanillaSample::VariableRateShadingType::None:                  return "None";
    case VanillaSample::VariableRateShadingType::Tier1_Static_1x2_2x1:  return "VRS_Static_1x2_2x1";
    case VanillaSample::VariableRateShadingType::Tier1_Static_2x2:      return "VRS_Static_2x2";
    case VanillaSample::VariableRateShadingType::Tier1_Static_2x4_4x2:  return "VRS_Static_2x4_4x2";
    case VanillaSample::VariableRateShadingType::Tier1_Static_4x4:      return "VRS_Static_4x4";
    case VanillaSample::VariableRateShadingType::Tier1_DoF_Driven:      return "Tier1_DoF_Driven";
    //case VanillaSample::VariableRateShadingType::VRS_Tier2_DoF_Driven:   return "VRS_Tier2_DoF_Driven";
    case VanillaSample::VariableRateShadingType::MaxValue:
    default:
        assert( false );
        return nullptr;
        break;
    }
}

bool VanillaSample::GetVRSOptionSupported( VariableRateShadingType type )
{
    switch( type )
    {
    case VanillaSample::VariableRateShadingType::None:                  return "None";
    case VanillaSample::VariableRateShadingType::Tier1_Static_1x2_2x1:  return GetRenderDevice( ).GetCapabilities( ).VariableShadingRate.Tier1;
    case VanillaSample::VariableRateShadingType::Tier1_Static_2x2:      return GetRenderDevice( ).GetCapabilities( ).VariableShadingRate.Tier1;
    case VanillaSample::VariableRateShadingType::Tier1_Static_2x4_4x2:  return GetRenderDevice( ).GetCapabilities( ).VariableShadingRate.AdditionalShadingRatesSupported;
    case VanillaSample::VariableRateShadingType::Tier1_Static_4x4:      return GetRenderDevice( ).GetCapabilities( ).VariableShadingRate.AdditionalShadingRatesSupported;
    case VanillaSample::VariableRateShadingType::Tier1_DoF_Driven:      return GetRenderDevice( ).GetCapabilities( ).VariableShadingRate.Tier1;
    //case VanillaSample::VariableRateShadingType::VRS_Tier2_DoF_Driven:   return "VRS_Tier2_DoF_Driven";
    case VanillaSample::VariableRateShadingType::MaxValue:
    default:
        assert( false );
        return false;
        break;
    }
}

bool VanillaSample::SetVRSOption( VariableRateShadingType type )
{
    // in case not supported
    if( !GetVRSOptionSupported( type ) )
    {
        VA_LOG_WARNING( "Specified VRS path not supported by the device" );
        return false;
    }
    else
    {
        m_settings.VariableRateShadingOption = type;
        return true;
    }
}

bool VanillaSample::LoadCamera( int index )
{
    vaFileStream fileIn;
    if( fileIn.Open( CameraFileName(index), FileCreationMode::Open ) )
    {
        m_camera->Load( fileIn );
        m_camera->AttachController( m_cameraFreeFlightController );
        return true;
    }
    return false;
}

void VanillaSample::SaveCamera( int index )
{
    vaFileStream fileOut;
    if( fileOut.Open( CameraFileName(index), FileCreationMode::Create ) )
    {
        m_camera->Save( fileOut );
    }
}

void VanillaSample::LoadAssetsAndScenes( )
{
    // this loads and initializes asset pack manager - and at the moment loads assets
    GetRenderDevice().GetAssetPackManager( ).LoadPacks( "*", true );  // these should be loaded automatically by scenes that need them but for now just load all in the asset folder

    wstring mediaRootFolder = vaCore::GetExecutableDirectory() + L"Media\\";

    // Bistro scene
    {
        // Load from file
        m_scenes[(int32)SceneSelectionType::LumberyardBistro]->Name() = "LumberyardBistro";
        m_scenes[(int32)SceneSelectionType::LumberyardBistro]->Load( mediaRootFolder + L"Bistro.scene.xml" );

        vaMatrix3x3 mat = vaMatrix3x3::RotationAxis( vaVector3( 0, 0, 1 ), -0.25f * VA_PIf ) * vaMatrix3x3::RotationAxis( vaVector3( 1, 0, 0 ), -0.5f * VA_PIf );
//        m_scenes[(int32)SceneSelectionType::LumberyardBistro]->SetEnvmap( GetRenderDevice(), "Media\\Bistro_Interior_cube.dds", mat, 0.04f );
        m_scenes[(int32)SceneSelectionType::LumberyardBistro]->SetSkybox( GetRenderDevice(), "Media\\Bistro_Exterior_Dark_cube.dds", mat, 0.01f );
    }
}

void VanillaSample::OnBeforeStopped( )
{
#ifdef VA_ENABLE_TEXTURE_REDUCTION_TOOL
    if( vaTextureReductionTestTool::GetInstancePtr() != nullptr )
    {
        vaTextureReductionTestTool::GetInstance().ResetCamera( m_camera );
        delete vaTextureReductionTestTool::GetInstancePtr();
    }
#endif
    m_postProcessTonemap = nullptr;
    m_CMAA2 = nullptr;
//    m_SSAccumulationColor = nullptr;
}

void VanillaSample::OnTick( float deltaTime )
{
    VA_TRACE_CPU_SCOPE( OnTick );

    vaDrawResultFlags prevDrawResultFlags = m_currentDrawResults;
    m_currentDrawResults = vaDrawResultFlags::None;

    m_settings.Validate( );
    
    // additional validate for VRS
    if( !GetVRSOptionSupported( m_settings.VariableRateShadingOption ) )
    {
        m_settings.VariableRateShadingOption = VanillaSample::VariableRateShadingType::None;
        VA_LOG_WARNING( "Specified VRS path not supported by the device" );
    }

    // if we're stuck on loading / compiling, slow down the 
    if( prevDrawResultFlags != vaDrawResultFlags::None )
        vaThreading::Sleep(30); 

    // if everything was OK with the last tick we can continue with the autobench, otherwise skip the frame until everything is loaded / compiled
#if 0
    if( prevDrawResultFlags == vaDrawResultFlags::None )
        m_autoBench->Tick( deltaTime );
#endif

    // verify any settings changes that require global shader macros change
    if( m_lastSettings.EnableGradientFilterExtension != m_settings.EnableGradientFilterExtension )
        m_globalShaderMacrosDirty = true;
    if( m_lastSettings.ShadingComplexity != m_settings.ShadingComplexity )
    {
        m_IBLProbeLocal->Reset();
        m_IBLProbeDistant->Reset();
    }
    m_lastSettings = m_settings;

    // update global shader macros if needed - these are not meant to be changed every frame and the change triggers shader recompile
    if( m_globalShaderMacrosDirty )
    {
        vector< pair< string, string > > globalShaderMacros;

        globalShaderMacros.push_back( { "VA_NO_ALPHA_TEST_IN_MAIN_DRAW", "" } );

        if( m_settings.EnableGradientFilterExtension )
            globalShaderMacros.push_back( { "VA_INTEL_EXT_GRADIENT_FILTER_EXTENSION_ENABLED", "" } );

        GetRenderDevice( ).GetMaterialManager( ).SetGlobalShaderMacros( globalShaderMacros );
    }

    m_camera->SetYFOV( m_settings.CameraYFov );

    bool freezeMotionAndInput = false;

#ifdef VA_ENABLE_TEXTURE_REDUCTION_TOOL
    if( vaTextureReductionTestTool::GetInstancePtr( ) != nullptr && vaTextureReductionTestTool::GetInstance( ).IsRunningTests( ) )
        freezeMotionAndInput = true;
#endif

    {
        std::shared_ptr<vaCameraControllerBase> wantedCameraController = ( freezeMotionAndInput ) ? ( nullptr ) : ( m_cameraFreeFlightController );

        if( m_cameraFlythroughPlay )
            wantedCameraController = m_cameraFlythroughController;

        if( m_camera->GetAttachedController( ) != wantedCameraController )
            m_camera->AttachController( wantedCameraController );
    }

    {
        const float minValidDelta = 0.0005f;
        if( deltaTime < minValidDelta )
        {
            // things just not correct when the framerate is so high
            VA_LOG_WARNING( "frame delta time too small, clamping" );
            deltaTime = minValidDelta;
        }

        const float maxValidDelta = 0.3f;
        if( deltaTime > maxValidDelta )
        {
            // things just not correct when the framerate is so low
            // VA_LOG_WARNING( "frame delta time too large, clamping" );
            deltaTime = maxValidDelta;
        }

        if( freezeMotionAndInput )
            deltaTime = 0.0f;

        m_lastDeltaTime = deltaTime;
    }

#ifdef VA_ENABLE_TEXTURE_REDUCTION_TOOL
    if( vaTextureReductionTestTool::GetInstancePtr( ) != nullptr )
    {
        auto controller = m_camera->GetAttachedController( );
        m_camera->AttachController( nullptr );
        vaTextureReductionTestTool::GetInstance( ).TickCPU( m_camera );
        m_camera->AttachController( controller );   // <- this actually updates the controller to current camera values so it doesn't override us in Tick()
        //m_camera->Tick( 0.0f, false );

        if( !vaTextureReductionTestTool::GetInstance( ).IsEnabled( ) )
            delete vaTextureReductionTestTool::GetInstancePtr( );
    }
#endif
    m_camera->Tick( deltaTime, m_application.HasFocus( ) && !freezeMotionAndInput );

    // DoF setup
    {
        if( m_camera->GetAttachedController() == m_cameraFlythroughController )
        {
            m_settings.DoFFocalLength   = m_cameraFlythroughController->GetLastUserParams().x;
            m_settings.DoFRange         = m_cameraFlythroughController->GetLastUserParams().y;
        }
        if( m_DepthOfField != nullptr )
        {
            // this is not physically correct :(
            m_DepthOfField->Settings().InFocusFrom          = m_settings.DoFFocalLength * (1.0f - m_settings.DoFRange);
            m_DepthOfField->Settings().InFocusTo            = m_settings.DoFFocalLength * (1.0f + m_settings.DoFRange);
        
            const float transitionRange = 2.0f * m_settings.DoFRange;
            m_DepthOfField->Settings().NearTransitionRange  = m_settings.DoFFocalLength * (2.0f * m_settings.DoFRange);
            m_DepthOfField->Settings().FarTransitionRange   = m_settings.DoFFocalLength * (4.0f * m_settings.DoFRange);
        }
    }

    // Scene stuff
    {
        VA_TRACE_CPU_SCOPE( SceneStuff );

        auto prevScene = m_currentScene;
        
        m_currentScene = m_scenes[ (int32)SceneSelectionType::LumberyardBistro ];// m_scenes[ (int32)m_settings.SceneChoice ];
        assert( m_currentScene != nullptr );
        
        m_currentSceneChanged = m_currentScene != prevScene;

        if( m_currentSceneChanged )
        {
            m_IBLProbeLocal->Reset();
            m_IBLProbeDistant->Reset();
            m_shadowsStable = false;
            m_IBLsStable = false;
        }

        if( m_currentScene != nullptr )
            m_currentScene->Tick( deltaTime );

        // custom lights setups
        bool lenabled[16*3] = { 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
                                0, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0,
                                0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1  };
        // start from 2 to skip ambient/directional
        for( int i = 0; i < m_currentScene->Lights().size(); i++ )
        {
            int lind = 16*m_settings.ShadingComplexity + i;
            if( lind < _countof(lenabled) )
                m_currentScene->Lights()[i]->Enabled = lenabled[lind];
            //m_currentScene
        }

        assert( m_selectedOpaque.MeshList->Count() == 0 && m_selectedTransparent.MeshList->Count() == 0 ); // leftovers from before? shouldn't happen!

        auto sceneObjectFilter = [ &settings = m_settings, &dofEffect = m_DepthOfField, &camera = m_camera ]( const vaSceneObject &, const vaMatrix4x4 &, const vaOrientedBoundingBox & obb, const vaRenderMesh &, const vaRenderMaterial &, int & outBaseShadingRate, vaVector4 & outCustomColor ) -> bool
        {
            if( settings.VariableRateShadingOption == VanillaSample::VariableRateShadingType::Tier1_DoF_Driven )
            {
                //if( obb.NearestDistanceToPoint( m_mouseCursor3DWorldPosition ) <= 0.01f )
                //    GetRenderDevice().GetCanvas3D( ).DrawBox( obb, 0xFF00FF00, 0x04202020 );

                float vrsFactor = dofEffect->ComputeConservativeBlurFactor( *camera, obb );
                vrsFactor = std::max( 0.0f, vrsFactor + settings.DoFDrivenVRSTransitionOffset );
                outBaseShadingRate = (int)( vrsFactor * (float)settings.DoFDrivenVRSMaxRate + 0.5f );

                vaVector4 debugColors[] = { vaVector4( 0.0f, 0.0f, 1.0f, 0.3f ),
                                            vaVector4( 0.0f, 1.0f, 0.0f, 0.3f ),
                                            vaVector4( 0.8f, 0.8f, 0.0f, 0.3f ),
                                            vaVector4( 0.9f, 0.5f, 0.0f, 0.3f ),
                                            vaVector4( 1.0f, 0.0f, 0.0f, 0.3f ), };
                outBaseShadingRate = vaMath::Clamp( outBaseShadingRate, 0, settings.DoFDrivenVRSMaxRate );
                assert( outBaseShadingRate < _countof( debugColors ) );

                if( settings.VisualizeVRS )
                    outCustomColor = debugColors[outBaseShadingRate];
            }
            else if( settings.VariableRateShadingOption == VanillaSample::VariableRateShadingType::None )
                outBaseShadingRate = 0;
            else if( settings.VariableRateShadingOption == VanillaSample::VariableRateShadingType::Tier1_Static_1x2_2x1 )
                outBaseShadingRate = 1;
            else if( settings.VariableRateShadingOption == VanillaSample::VariableRateShadingType::Tier1_Static_2x2 )
                outBaseShadingRate = 2;
            else if( settings.VariableRateShadingOption == VanillaSample::VariableRateShadingType::Tier1_Static_2x4_4x2 )
                outBaseShadingRate = 3;
            else if( settings.VariableRateShadingOption == VanillaSample::VariableRateShadingType::Tier1_Static_4x4 )
                outBaseShadingRate = 4;
            return true;
        };

        if( m_currentScene != nullptr )
        {
            assert( m_selectedOpaque.MeshList->Count( ) == 0 );
            assert( m_selectedTransparent.MeshList->Count( ) == 0 );

            m_currentDrawResults |= m_currentScene->SelectForRendering( &m_selectedOpaque, &m_selectedTransparent, vaRenderSelection::FilterSettings::FrustumCull( *m_camera ), sceneObjectFilter );

            // This is where we would start the async sorts if we had that implemented - or actually at some point below
            // if we're changing VRS shading rate
            m_sortDepthPrepass  = vaRenderSelection::SortSettings::Standard( *m_camera, true, false );
            m_sortOpaque        = vaRenderSelection::SortSettings::Standard( *m_camera, true, m_settings.SortByVRS );
            m_sortTransparent   = vaRenderSelection::SortSettings::Standard( *m_camera, false, false );

            // Lights
            m_currentScene->ApplyToLighting( *m_lighting );
        }

        // Sky
        if( m_lighting->GetDistantIBL( ) == nullptr || !m_lighting->GetDistantIBL( )->HasSkybox( ) )
        {
            shared_ptr<vaTexture> skyboxTexture = nullptr; vaMatrix3x3 skyboxRotation = vaMatrix3x3::Identity; float skyboxColorMultiplier = 1.0f;
            if( m_currentScene != nullptr )
                m_currentScene->GetSkybox( skyboxTexture, skyboxRotation, skyboxColorMultiplier );
            m_skybox->SetCubemap( skyboxTexture );
            m_skybox->Settings( ).Rotation = skyboxRotation;
            m_skybox->Settings( ).ColorMultiplier = skyboxColorMultiplier;
        }
        else
            m_lighting->GetDistantIBL( )->SetToSkybox( *m_skybox );

    }

    // tick lighting
    m_lighting->Tick( deltaTime );

    // m_mouseCursor3DWorldPosition is one frame old because the update happens later during rendering; this can be fixed but it doesn't really matter in this case
    // GetRenderDevice().GetCanvas3D().DrawSphere( m_mouseCursor3DWorldPosition, 0.1f, 0xFF0000FF ); //, 0xFFFFFFFF );


    // do we need to redraw a shadow map?
    {
        VA_TRACE_CPU_SCOPE( Shadowmaps );

        assert( m_queuedShadowmap == nullptr ); // we should have reseted it already
        m_queuedShadowmap = m_lighting->GetNextHighestPriorityShadowmapForRendering( );
        m_shadowsStable = m_queuedShadowmap == nullptr;

        // don't record shadows if assets are still loading - they will be broken
        if( m_currentDrawResults != vaDrawResultFlags::None )
            m_queuedShadowmap = nullptr;

        if( m_queuedShadowmap != nullptr )
        {
            // we intend to select meshes, so make sure the list for storage is created
            assert( m_queuedShadowmapRenderSelection.MeshList->Count( ) == 0 ); // leftovers from before? shouldn't happen!

            // should have a radius-based filter here
            // vaRenderSelection::FilterSettings selectionFilter( *m_camera );

            if( m_currentScene != nullptr )
                m_currentDrawResults |= m_currentScene->SelectForRendering( &m_queuedShadowmapRenderSelection, nullptr, vaRenderSelection::FilterSettings::ShadowmapCull( *m_queuedShadowmap ) );
        }
        if( m_currentDrawResults != vaDrawResultFlags::None )
        {
            m_queuedShadowmap = nullptr;
            m_queuedShadowmapRenderSelection.Reset( );
        }
    }

    if( !freezeMotionAndInput && m_application.HasFocus( ) && !vaInputMouseBase::GetCurrent( )->IsCaptured( ) 
#ifdef VA_IMGUI_INTEGRATION_ENABLED
        && !ImGui::GetIO().WantTextInput 
#endif
        )
    {
        static float notificationStopTimeout = 0.0f;
        notificationStopTimeout += deltaTime;

        vaInputKeyboardBase & keyboard = *vaInputKeyboardBase::GetCurrent( );
        if( keyboard.IsKeyDown( vaKeyboardKeys::KK_LEFT ) || keyboard.IsKeyDown( vaKeyboardKeys::KK_RIGHT ) || keyboard.IsKeyDown( vaKeyboardKeys::KK_UP ) || keyboard.IsKeyDown( vaKeyboardKeys::KK_DOWN ) ||
            keyboard.IsKeyDown( ( vaKeyboardKeys )'W' ) || keyboard.IsKeyDown( ( vaKeyboardKeys )'S' ) || keyboard.IsKeyDown( ( vaKeyboardKeys )'A' ) ||
            keyboard.IsKeyDown( ( vaKeyboardKeys )'D' ) || keyboard.IsKeyDown( ( vaKeyboardKeys )'Q' ) || keyboard.IsKeyDown( ( vaKeyboardKeys )'E' ) )
        {
            if( notificationStopTimeout > 3.0f )
            {
                notificationStopTimeout = 0.0f;
                vaLog::GetInstance().Add( vaVector4( 1.0f, 0.0f, 0.0f, 1.0f ), L"To switch into free flight (move&rotate) mode, use mouse right click." );
            }
        }

#ifdef VA_IMGUI_INTEGRATION_ENABLED
        if( !ImGui::GetIO().WantCaptureMouse )
#endif
            m_zoomTool->HandleMouseInputs( *vaInputMouseBase::GetCurrent() );
    }
    
#if 0 // save/load camera locations
    // let the ImgUI controls have input priority
    if( !freezeMotionAndInput && !ImGui::GetIO( ).WantCaptureKeyboard && (vaInputKeyboard::GetCurrent( ) != nullptr) ) 
    {
        int numkeyPressed = -1;
        for( int i = 0; i <= 9; i++ )
            numkeyPressed = ( vaInputKeyboard::GetCurrent( )->IsKeyClicked( (vaKeyboardKeys)('0'+i) ) ) ? ( i ) : ( numkeyPressed );

        if( vaInputKeyboard::GetCurrent( )->IsKeyDownOrClicked( KK_LCONTROL ) && ( numkeyPressed != -1 ) )
            SaveCamera( numkeyPressed );
        
        if( vaInputKeyboard::GetCurrent( )->IsKeyDownOrClicked( KK_LSHIFT ) && ( numkeyPressed != -1 ) )
            LoadCamera( numkeyPressed );
    }
#endif

    m_application.TickUI( *m_camera );

#ifdef VA_ENABLE_TEXTURE_REDUCTION_TOOL
    if( vaTextureReductionTestTool::GetInstancePtr( ) != nullptr )
    {
        vaTextureReductionTestTool::GetInstance( ).TickUI( GetRenderDevice(), m_camera, !vaInputMouseBase::GetCurrent()->IsCaptured() );
    }
#endif

    // Do the rendering tick and present 
    {
        VA_TRACE_CPU_SCOPE( RenderingSection );

        GetRenderDevice().BeginFrame( deltaTime );

        m_currentDrawResults |= RenderTick( deltaTime );

        m_allLoadedPrecomputedAndStable = ( m_currentDrawResults == vaDrawResultFlags::None ) && m_shadowsStable && m_IBLsStable;

        // this does all the UI - imgui and debugcanvas2d/debugcanvas3d
        m_application.DrawUI( *GetRenderDevice().GetMainContext(), m_GBuffer->GetDepthBuffer( ) );

        GetRenderDevice().EndAndPresentFrame( (m_application.GetVsync())?(1):(0) );
    }
}

vaDrawResultFlags VanillaSample::DrawSceneDepthPrePass( vaRenderDeviceContext & renderContext, const vaRenderSelection & renderSelection, const vaCameraBase & camera, const shared_ptr<vaTexture> & depth, const vaSceneDrawContext::GlobalSettings & globalSettings, const vaRenderSelection::SortSettings & sortSettings )
{
    vaDrawResultFlags drawResults = vaDrawResultFlags::None;

    // clear the main render target / depth
    depth->ClearDSV( renderContext, true, camera.GetUseReversedZ( ) ? ( 0.0f ) : ( 1.0f ), false, 0 );

    // Depth pre-pass
    {
        VA_TRACE_CPUGPU_SCOPE( ZPrePass, renderContext );
        vaSceneDrawContext drawContext( renderContext, camera, vaDrawContextOutputType::DepthOnly, vaDrawContextFlags::None, nullptr, globalSettings );

        // set main depth only
        renderContext.SetRenderTarget( nullptr, depth, true );

        vaRenderMeshDrawFlags flags = vaRenderMeshDrawFlags::EnableDepthTest | vaRenderMeshDrawFlags::EnableDepthWrite | vaRenderMeshDrawFlags::DisableVRS;
        drawResults |= GetRenderDevice( ).GetMeshManager( ).Draw( drawContext, *renderSelection.MeshList, vaBlendMode::Opaque, flags, sortSettings );
    }
    return drawResults;
}

vaDrawResultFlags VanillaSample::DrawSceneOpaque( vaRenderDeviceContext & renderContext, const vaRenderSelection & renderSelection, const vaCameraBase & camera, const shared_ptr<vaTexture> & depth, const shared_ptr<vaTexture> & color, bool sky, bool applySSAO, const vaSceneDrawContext::GlobalSettings & globalSettings, const vaRenderSelection::SortSettings & sortSettings )
{
    vaDrawResultFlags drawResults = vaDrawResultFlags::None;


    shared_ptr<vaTexture> ssaoBuffer = nullptr;
    if( m_SSAO != nullptr && applySSAO )
    {
        if( m_SSAOScratchBuffer == nullptr || m_SSAOScratchBuffer->GetSize() != depth->GetSize() )
        {
#ifdef _DEBUG   // just an imperfect way of making sure SSAO scratch buffer isn't getting re-created continuously (will also get triggered if you change res 1024 times... so that's why it's imperfect)
            static int recreateCount = 0;
            recreateCount++;
            assert( recreateCount < 1024 );
#endif
            m_SSAOScratchBuffer = vaTexture::Create2D( GetRenderDevice(), vaResourceFormat::R8_UNORM, depth->GetWidth(), depth->GetHeight(), 2, 1, 1, vaResourceBindSupportFlags::ShaderResource | vaResourceBindSupportFlags::RenderTarget );
            m_SSAOScratchBufferMIP0 = vaTexture::CreateView( m_SSAOScratchBuffer, vaTextureFlags::None, 0, 1 );
            m_SSAOScratchBufferMIP1 = vaTexture::CreateView( m_SSAOScratchBuffer, vaTextureFlags::None, 1, 1 );
        }
        ssaoBuffer = m_SSAOScratchBuffer;
        renderContext.SetRenderTarget( ssaoBuffer, nullptr, true );
        drawResults |= m_SSAO->Draw( renderContext, camera.GetProjMatrix( ), depth, false, nullptr );

        // create the 1st mip level needed for at least some filtering at 4x4
        renderContext.SetRenderTarget( m_SSAOScratchBufferMIP1, nullptr, true );
        drawResults |= renderContext.StretchRect( m_SSAOScratchBufferMIP0 );
    }
    m_lighting->SetAOMap( ssaoBuffer );

    {
        // Forward opaque
        VA_TRACE_CPUGPU_SCOPE( Forward, renderContext );

        vaSceneDrawContext drawContext( renderContext, camera, vaDrawContextOutputType::Forward, vaDrawContextFlags::None, m_lighting.get( ), globalSettings );

        renderContext.SetRenderTarget( color, depth, true );

        vaRenderMeshDrawFlags drawFlags = vaRenderMeshDrawFlags::EnableDepthTest | vaRenderMeshDrawFlags::DepthTestEqualOnly | vaRenderMeshDrawFlags::DepthTestIncludesEqual;
        drawResults |= GetRenderDevice( ).GetMeshManager( ).Draw( drawContext, *renderSelection.MeshList, vaBlendMode::Opaque, drawFlags, sortSettings );

        m_lighting->SetAOMap( nullptr );

        // opaque skybox
        if( sky )
        {
            VA_TRACE_CPUGPU_SCOPE( Sky, renderContext );
            drawResults |= m_skybox->Draw( drawContext );
        }
    }

    return drawResults;
}
 
vaDrawResultFlags VanillaSample::DrawScenePostOpaque( vaRenderDeviceContext& renderContext, const vaRenderSelection & renderSelection, const vaCameraBase & camera, const shared_ptr<vaTexture> & depth, const shared_ptr<vaTexture> & color, bool transparencies, const vaSceneDrawContext::GlobalSettings & globalSettings, const vaRenderSelection::SortSettings & sortSettings )
{
    vaDrawResultFlags drawResults = vaDrawResultFlags::None;

    vaSceneDrawContext drawContext( renderContext, camera, vaDrawContextOutputType::Forward, vaDrawContextFlags::None, m_lighting.get( ), globalSettings );

    renderContext.SetRenderTarget( color, depth, true );

    // transparencies
    if( transparencies )
    {
        VA_TRACE_CPUGPU_SCOPE( Transparencies, renderContext );

        vaRenderMeshDrawFlags drawFlags = vaRenderMeshDrawFlags::EnableDepthTest | vaRenderMeshDrawFlags::DepthTestIncludesEqual;

        drawResults |= GetRenderDevice( ).GetMeshManager( ).Draw( drawContext, *renderSelection.MeshList, vaBlendMode::AlphaBlend, drawFlags, sortSettings );
    }
    return drawResults;
}

vaDrawResultFlags VanillaSample::DrawScene( vaRenderCamera & camera, vaGBuffer & gbufferOutput, const shared_ptr<vaTexture> & gbufferColorScratch, bool & colorScratchContainsFinal, const vaViewport & mainViewport, const vaSceneDrawContext::GlobalSettings & globalSettings, bool skipCameraLuminanceUpdate )
{
    VA_TRACE_CPU_SCOPE( VanillaSample_DrawScene );

    vaDrawResultFlags drawResults = vaDrawResultFlags::None;
    vaRenderDeviceContext & mainContext = *GetRenderDevice().GetMainContext( );

    mainViewport;
    colorScratchContainsFinal;
    gbufferColorScratch;

    // m_renderDevice.GetMaterialManager().SetTexturingDisabled( m_debugTexturingDisabled ); 

    // decide on the main render target / depth
    shared_ptr<vaTexture> mainColorRT = gbufferOutput.GetOutputColor( );  // m_renderDevice->GetCurrentBackbuffer();
    shared_ptr<vaTexture> mainColorRTIgnoreSRGBConvView = gbufferOutput.GetOutputColorIgnoreSRGBConvView( );  // m_renderDevice->GetCurrentBackbuffer();
    shared_ptr<vaTexture> mainDepthRT = gbufferOutput.GetDepthBuffer( );

    // normal scene rendering path
    {
        // Depth pre-pass
        drawResults |= DrawSceneDepthPrePass( mainContext, m_selectedOpaque, camera, mainDepthRT, globalSettings, m_sortDepthPrepass );

        // this clear is not actually needed as every pixel should be drawn into anyway! but draw pink debug background in debug to validate this
#ifdef _DEBUG
        // clear light accumulation (radiance) RT
        gbufferOutput.GetRadiance( )->ClearRTV( mainContext, vaVector4( 1.0f, 0.0f, 1.0f, 0.0f ) );
#endif

#if 0
        {
            static shared_ptr<vaTexture>        overrideDepth           = nullptr;
            static shared_ptr<vaPixelShader>    depthCopyPixelShader    = nullptr;
            if( overrideDepth != nullptr && overrideDepth->GetSize() != mainDepthRT->GetSize() )
                overrideDepth = nullptr;
            if( overrideDepth == nullptr )
                overrideDepth = vaTexture::Create2D( GetRenderDevice(), vaResourceFormat::R32_TYPELESS, mainDepthRT->GetSizeX(), mainDepthRT->GetSizeY(), 1, 1, 1, vaResourceBindSupportFlags::DepthStencil | vaResourceBindSupportFlags::ShaderResource, 
                                                        vaResourceAccessFlags::Default, vaResourceFormat::R32_FLOAT, vaResourceFormat::Unknown, vaResourceFormat::D32_FLOAT );

            if( depthCopyPixelShader == nullptr )
            {
                depthCopyPixelShader = VA_RENDERING_MODULE_CREATE_SHARED( vaPixelShader, vaRenderingModuleParams ( GetRenderDevice() ) );
                depthCopyPixelShader->CreateShaderFromBuffer( 
                    "Texture2D g_source           : register( t0 );                         \n"
                    "float main( in const float4 xPos : SV_Position ) : SV_Depth            \n"
                    "{                                                                      \n"
                    "   return g_source.Load( int3( xPos.xy, 0 ) ).x;                       \n"
                    "}                                                                      \n"
                    , "ps_5_0", "main", vaShaderMacroContaner(), true );
            }

            overrideDepth->ClearDSV( mainContext, true, camera.GetUseReversedZ( ) ? ( 0.0f ) : ( 1.0f ), false, 0 );

            vaGraphicsItem renderItem;
            GetRenderDevice().FillFullscreenPassRenderItem( renderItem );
            renderItem.ShaderResourceViews[0] = mainDepthRT;
            renderItem.PixelShader = depthCopyPixelShader;
            //renderItem.PixelShader->WaitFinishIfBackgroundCreateActive();
            renderItem.DepthEnable      = true;
            renderItem.DepthWriteEnable = true;
            renderItem.DepthFunc        = vaComparisonFunc::Always;
            mainContext.SetRenderTarget( nullptr, overrideDepth, true );
            mainContext.ExecuteSingleItem( renderItem, nullptr );
            
            mainDepthRT = overrideDepth;
        }
#endif

        // Opaque stuff
        drawResults |= DrawSceneOpaque( mainContext, m_selectedOpaque, camera, mainDepthRT, gbufferOutput.GetRadiance(), m_skybox->IsEnabled( ), true, globalSettings, m_sortOpaque );

        // Transparencies and effects
        drawResults |= DrawScenePostOpaque( mainContext, m_selectedTransparent, camera, mainDepthRT, gbufferOutput.GetRadiance(), true, globalSettings, m_sortTransparent );

        mainContext.SetRenderTarget( gbufferOutput.GetRadiance( ), mainDepthRT, true );

        // debugging & wireframe
        {
            vaSceneDrawContext drawContext( mainContext, camera, vaDrawContextOutputType::Forward, vaDrawContextFlags::None, m_lighting.get( ) );

            vaDebugCanvas3D & canvas3D = GetRenderDevice().GetCanvas3D( );
            canvas3D; // unreferenced in Release

#if 0
            if( m_importerMode )
            {
                //canvas3D.DrawPlane( vaPlane::FromPointNormal( {0, 0, 0}, {0, 0, 1} ), 0x04808080 );
                canvas3D.DrawAxis( vaVector3( 0, 0, 0 ), 10000.0f, NULL, 0.3f );

                for( float gridStep = 1.0f; gridStep <= 1000; gridStep *= 10 )
                {
                    int gridCount = 10;
                    for( int i = -gridCount; i <= gridCount; i++ )
                    {
                        canvas3D.DrawLine( { i * gridStep, - gridCount * gridStep, 0.0f }, { i * gridStep, + gridCount * gridStep, 0.0f }, 0x80000000 );
                        canvas3D.DrawLine( { - gridCount * gridStep, i * gridStep, 0.0f }, { + gridCount * gridStep, i * gridStep, 0.0f }, 0x80000000 );
                    }
                }
            }
#endif
        }

        // restore main render target / depth
        mainContext.SetRenderTarget( mainColorRT, nullptr, true );

        // Tonemap to final color & postprocess
        {
            VA_TRACE_CPUGPU_SCOPE( TonemapAndPostFX, mainContext );

            // stop unrendered items from messing auto exposure
            if( drawResults != vaDrawResultFlags::None )
                skipCameraLuminanceUpdate = true;

            vaPostProcessTonemap::AdditionalParams tonemapParams( skipCameraLuminanceUpdate );

            // export luma
            tonemapParams.OutExportLuma = m_exportedLuma;

            vaRenderDeviceContext::RenderOutputsState backupOutputs = mainContext.GetOutputs( );

            drawResults |= m_postProcessTonemap->TickAndApplyCameraPostProcess( mainContext, camera, gbufferOutput.GetRadiance( ), tonemapParams );

            bool ppAAApplied = false;

            // Apply depth of field!
            // (intentionally applied before CMAA because some of it has sharp depth-based cutoff that causes aliasing)
            if( m_settings.EnableDOF )
            {
                vaSceneDrawContext sceneDrawContext( mainContext, camera, vaDrawContextOutputType::Forward );
                // sceneContext needed for NDCToViewDepth to work - could be split out and made part of the constant buffer here
                drawResults |= m_DepthOfField->Draw( sceneDrawContext, mainDepthRT, mainColorRT, mainColorRTIgnoreSRGBConvView );
            }

            // Apply CMAA!
            {
                VA_TRACE_CPUGPU_SCOPE( CMAA2, mainContext );
                if( m_CMAA2 != nullptr )
                {
                    drawResults |= m_CMAA2->Draw( mainContext, mainColorRT, m_exportedLuma );
                    ppAAApplied = true;
                }
            }
            mainContext.SetOutputs( backupOutputs );
        }
       
        // wireframe & debug & 3D UI
        {
            VA_TRACE_CPUGPU_SCOPE( DebugAndUI3D, mainContext );
            vaSceneDrawContext sceneDrawContext( mainContext, camera, vaDrawContextOutputType::Forward, vaDrawContextFlags::SetZOffsettedProjMatrix | vaDrawContextFlags::DebugWireframePass, m_lighting.get( ), globalSettings );

            // we need depth for this!
            mainContext.SetRenderTarget( mainColorRT, mainDepthRT, true );

            if( m_settings.ShowWireframe )
            {
                drawResults |= GetRenderDevice( ).GetMeshManager( ).Draw( sceneDrawContext, *m_selectedOpaque.MeshList, vaBlendMode::Opaque, vaRenderMeshDrawFlags::EnableDepthTest | vaRenderMeshDrawFlags::DepthTestIncludesEqual );
                drawResults |= GetRenderDevice( ).GetMeshManager( ).Draw( sceneDrawContext, *m_selectedTransparent.MeshList, vaBlendMode::Opaque, vaRenderMeshDrawFlags::EnableDepthTest | vaRenderMeshDrawFlags::DepthTestIncludesEqual );
            }

            // restore to what it was
            mainContext.SetRenderTarget( mainColorRT, nullptr, true );
        }
    }

    // debug draw for showing depth / normals / stuff like that
#if 0
    {
        VA_TRACE_CPUGPU_SCOPE( DebugDraw, mainContext );
        vaSceneDrawContext drawContext( mainContext, camera, vaDrawContextOutputType::Forward, vaDrawContextFlags::None, m_lighting.get( ) );
        drawContext.GlobalMIPOffset = globalMIPOffset; drawContext.GlobalPixelScale = globalPixelScale;
        if( colorScratchContainsFinal )
            mainContext.SetRenderTarget( gbufferColorScratch, nullptr, true );
        else
            mainContext.SetRenderTarget( m_GBuffer->GetOutputColor( ), nullptr, false );
        m_renderingGlobals->DebugDraw( drawContext );
    }
#endif
    return drawResults;
}

vaDrawResultFlags VanillaSample::RenderTick( float deltaTime )
{
    VA_TRACE_CPU_SCOPE( VanillaSample_RenderTick );

    vaRenderDeviceContext & mainContext = *GetRenderDevice().GetMainContext( );

    // this is "comparer stuff" and the main render target stuff
    vaViewport mainViewport = mainContext.GetViewport( );

    m_camera->SetViewportSize( mainViewport.Width, mainViewport.Height );

    // this updates the exposure and etc.
    m_camera->PreRenderTick( mainContext, deltaTime ); //, m_requireDeterminism ); <- handling tonemapping determinism on the benchmark script side by waiting n script loops

    bool colorScratchContainsFinal = false;

    vaDrawResultFlags drawResults = vaDrawResultFlags::None;

    {
        // update GBuffer resources if needed

        m_GBuffer->UpdateResources( mainViewport.Width, mainViewport.Height, 1, m_GBufferFormats );

        if( m_scratchPostProcessColor == nullptr || m_scratchPostProcessColor->GetSizeX() != m_GBuffer->GetOutputColor()->GetSizeX() || m_scratchPostProcessColor->GetSizeY() != m_GBuffer->GetOutputColor()->GetSizeY() || m_scratchPostProcessColor->GetResourceFormat() != m_GBuffer->GetOutputColor()->GetResourceFormat() ) 
        {
            m_scratchPostProcessColor = vaTexture::Create2D( GetRenderDevice(), m_GBuffer->GetOutputColor()->GetResourceFormat(), m_GBuffer->GetOutputColor()->GetSizeX(), m_GBuffer->GetOutputColor()->GetSizeY(), 1, 1, 1, vaResourceBindSupportFlags::RenderTarget | vaResourceBindSupportFlags::ShaderResource | vaResourceBindSupportFlags::UnorderedAccess, vaResourceAccessFlags::Default, 
                m_GBuffer->GetOutputColor()->GetSRVFormat(), m_GBuffer->GetOutputColor()->GetRTVFormat(), m_GBuffer->GetOutputColor()->GetDSVFormat(), vaResourceFormatHelpers::StripSRGB( m_GBuffer->GetOutputColor()->GetSRVFormat() ) );
            // m_scratchPostProcessColorIgnoreSRGBConvView = vaTexture::CreateView( *m_scratchPostProcessColor, m_scratchPostProcessColor->GetBindSupportFlags(), 
            //     vaResourceFormatHelpers::StripSRGB( m_scratchPostProcessColor->GetSRVFormat() ), vaResourceFormatHelpers::StripSRGB( m_scratchPostProcessColor->GetRTVFormat() ), vaResourceFormatHelpers::StripSRGB( m_scratchPostProcessColor->GetDSVFormat() ), vaResourceFormatHelpers::StripSRGB( m_scratchPostProcessColor->GetUAVFormat() ) );
        }

        if( m_exportedLuma == nullptr || m_exportedLuma->GetSizeX() != m_GBuffer->GetOutputColor()->GetSizeX() || m_exportedLuma->GetSizeY() != m_GBuffer->GetOutputColor()->GetSizeY() ) 
        {
            m_exportedLuma = vaTexture::Create2D( GetRenderDevice(), vaResourceFormat::R8_UNORM, m_GBuffer->GetRadiance()->GetSizeX(), m_GBuffer->GetRadiance()->GetSizeY(), 1, 1, 1, vaResourceBindSupportFlags::RenderTarget | vaResourceBindSupportFlags::UnorderedAccess | vaResourceBindSupportFlags::ShaderResource, vaResourceAccessFlags::Default );
            //m_exportedLuma->ClearUAV( mainContext, vaVector4( 0.5f, 0.0f, 0.0f, 0.0f ) );
        }
    }

#ifndef VA_INTEL_GRADFILTER_ENABLED
    m_settings.EnableGradientFilterExtension = false;
#endif

    // draw shadowmaps if needed
    if( m_queuedShadowmap != nullptr )
    {
        VA_TRACE_CPU_SCOPE( VanillaSample_RenderTick_DrawShadowmaps );
        drawResults |= m_queuedShadowmap->Draw( mainContext, m_queuedShadowmapRenderSelection );
        m_queuedShadowmap = nullptr;
        m_queuedShadowmapRenderSelection.Reset();
    }

    if( m_IBLsStable && m_application.GetTickNumber() % 50 == 0 )
    {
        if( m_lighting->GetLocalIBL() ) 
            m_IBLsStable &= m_lighting->GetLocalIBL()->GetContentsData() == m_currentScene->LocalIBL();
        if( m_lighting->GetDistantIBL() )
            m_IBLsStable &= m_lighting->GetDistantIBL()->GetContentsData() == m_currentScene->DistantIBL();
    }

    // this is a bit of a hack - for this one scene, update the cube at a specific location and exclude some objects
    assert( m_currentScene != nullptr );
    if( m_shadowsStable && m_currentDrawResults == vaDrawResultFlags::None && !m_IBLsStable )
    {
        shared_ptr<vaIBLProbe> probes[2]    = { m_lighting->GetLocalIBL(), m_lighting->GetDistantIBL() };
        vaIBLProbeData probeDatas[2]      = { m_currentScene->LocalIBL(), m_currentScene->DistantIBL() };

        int probesOK = 0;
        for( int i = 0; i < countof(probes); i++ )
        {
            shared_ptr<vaIBLProbe> IBLProbe = probes[i];
           
            if( (IBLProbe == nullptr) || (!m_currentScene->LocalIBL().Enabled) )
            {
                probes[i]->Reset();
                probesOK++;
                continue;
            }
            if( probeDatas[i] == probes[i]->GetContentsData( ) )
            {
                probesOK++;
                continue;
            }

            if( probeDatas[i].ImportFilePath != "" )
                probes[i]->Import( mainContext, probeDatas[i] );
            else
            {
                vaRenderSelection selectionOpaque;
                vaRenderSelection selectionTransparent;
                auto results = m_currentScene->SelectForRendering( &selectionOpaque, &selectionTransparent, vaRenderSelection::FilterSettings::EnvironmentProbeCull( probeDatas[i] ), 
                    [ ]( const vaSceneObject & obj, const vaMatrix4x4 & , const vaOrientedBoundingBox &, const vaRenderMesh &, const vaRenderMaterial &, int & outBaseShadingRate, vaVector4 & ) -> bool
                { 
                    outBaseShadingRate;
                    return obj.GetName( ) != "FlightHelmet" && (obj.GetParent() == nullptr || obj.GetParent()->GetName( ) != "ShinyBalls" ); 
                } );
                if( results == vaDrawResultFlags::None )
                {
                    CubeFaceCaptureCallback faceCapture = [thisPtr=this, &selectionOpaque, &selectionTransparent] ( vaRenderDeviceContext & renderContext, const vaCameraBase & faceCamera, const shared_ptr<vaTexture> & faceDepth, const shared_ptr<vaTexture> & faceColor )
                    {
                        vaDrawResultFlags drawResults = vaDrawResultFlags::None;

                        vaSceneDrawContext::GlobalSettings globalSettings;
                        globalSettings.SpecialEmissiveScale = 0.1f;
                        globalSettings.DisableGI = true;

                        // Depth pre-pass
                        drawResults |= thisPtr->DrawSceneDepthPrePass( renderContext, selectionOpaque, faceCamera, faceDepth, globalSettings, vaRenderSelection::SortSettings::Standard( faceCamera, true, false ) );

                        // this clear is not actually needed!
                        // // clear light accumulation (radiance) RT
                        // gbufferOutput.GetRadiance( )->ClearRTV( mainContext, vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) );

                        // Opaque stuff
                        drawResults |= thisPtr->DrawSceneOpaque( renderContext, selectionOpaque, faceCamera, faceDepth, faceColor, false, false, globalSettings, vaRenderSelection::SortSettings::Standard( faceCamera, false, thisPtr->m_settings.SortByVRS ) );

                        // Transparent stuff
                        selectionTransparent; // <- not really doing it for now, a bit more complexity

                        return drawResults;
                    };
                    if( probes[i]->Capture( mainContext, probeDatas[i], faceCapture ) == vaDrawResultFlags::None )
                        probesOK++;
                    break;
                }
            }
        }
        m_IBLsStable = probesOK == countof(probes);
    }

    if( m_settings.SuperSamplingOption != VanillaSample::SuperSamplingType::None )
    {
        m_SSBuffersLastUsed = 0;

        if( m_SSAccumulationColor == nullptr || m_SSAccumulationColor->GetSizeX() != m_GBuffer->GetResolution().x || m_SSAccumulationColor->GetSizeY() != m_GBuffer->GetResolution().y )
            m_SSAccumulationColor   = vaTexture::Create2D( GetRenderDevice(), vaResourceFormat::R16G16B16A16_FLOAT, m_GBuffer->GetResolution().x, m_GBuffer->GetResolution().y,1, 1, 1, vaResourceBindSupportFlags::RenderTarget | vaResourceBindSupportFlags::ShaderResource );

        m_SSAccumulationColor->ClearRTV( mainContext, vaVector4( 0, 0, 0, 0 ) );

        assert( false ); // below not implemented 
#if 0 
        int SSGridRes       = GetSSGridRes( m_settings.CurrentAAOption == VanillaSample::AAType::SuperSampleReferenceFast );
        int SSResScale      = GetSSResScale( m_settings.CurrentAAOption == VanillaSample::AAType::SuperSampleReferenceFast );
        float SSDDXDDYBias  = GetSSDDXDDYBias( m_settings.CurrentAAOption == VanillaSample::AAType::SuperSampleReferenceFast );
        float SSMIPBias     = GetSSMIPBias( m_settings.CurrentAAOption == VanillaSample::AAType::SuperSampleReferenceFast );
        float SSSharpen     = GetSSSharpen( m_settings.CurrentAAOption == VanillaSample::AAType::SuperSampleReferenceFast );

        if( m_SSGBuffer == nullptr || m_SSGBuffer->GetResolution() != m_GBuffer->GetResolution() * SSResScale || m_SSGBuffer->GetSampleCount() != m_GBuffer->GetSampleCount() || m_SSGBuffer->GetFormats() != m_GBuffer->GetFormats() )
        {
            m_SSGBuffer = VA_RENDERING_MODULE_CREATE_SHARED( vaGBuffer, GetRenderDevice() );
            m_SSGBuffer->UpdateResources( m_GBuffer->GetResolution().x * SSResScale, m_GBuffer->GetResolution().y * SSResScale, m_GBuffer->GetSampleCount(), m_GBuffer->GetFormats(), m_GBuffer->GetDeferredEnabled() );
        }
        if( m_SSScratchColor == nullptr || m_SSScratchColor->GetSizeX() != m_SSGBuffer->GetOutputColor()->GetSizeX() || m_SSScratchColor->GetSizeY() != m_SSGBuffer->GetOutputColor()->GetSizeY() || m_SSScratchColor->GetResourceFormat() != m_SSGBuffer->GetOutputColor()->GetResourceFormat() ) 
        {
            m_SSScratchColor = vaTexture::Create2D( GetRenderDevice(), m_SSGBuffer->GetOutputColor()->GetResourceFormat(), m_SSGBuffer->GetOutputColor()->GetSizeX(), m_SSGBuffer->GetOutputColor()->GetSizeY(), 1, 1, 1, vaResourceBindSupportFlags::RenderTarget | vaResourceBindSupportFlags::ShaderResource | vaResourceBindSupportFlags::UnorderedAccess, vaResourceAccessFlags::Default, 
                m_SSGBuffer->GetOutputColor()->GetSRVFormat(), m_SSGBuffer->GetOutputColor()->GetRTVFormat(), m_SSGBuffer->GetOutputColor()->GetDSVFormat(), vaResourceFormatHelpers::StripSRGB( m_SSGBuffer->GetOutputColor()->GetSRVFormat() ) );
        }


        vaSceneDrawContext::GlobalSettings globalSSSettings;

        globalSSSettings.MIPOffset  = SSMIPBias;

        globalSSSettings.PixelScale = vaVector2::ComponentDiv( vaVector2( m_SSGBuffer->GetResolution() ), vaVector2( m_GBuffer->GetResolution() ) );

        // SS messes up with pixel size which messes up with specular as it is based on ddx/ddy so compensate a bit here
        globalSSSettings.PixelScale = vaMath::Lerp( globalSSSettings.PixelScale, vaVector2( 1.0f, 1.0f ), SSDDXDDYBias );

        float stepX = 1.0f / (float)SSGridRes;
        float stepY = 1.0f / (float)SSGridRes;
        float addMult = 1.0f / (SSGridRes*SSGridRes);
        for( int jx = 0; jx < SSGridRes; jx++ )
        {
            for( int jy = 0; jy < SSGridRes; jy++ )
            {
                vaVector2 offset( (jx+0.5f) * stepX - 0.5f, (jy+0.5f) * stepY - 0.5f );

                // vaVector2 rotatedOffset( offsetO.x * angleC - offsetO.y * angleS, offsetO.x * angleS + offsetO.y * angleC );
                
                // instead of angle-based rotation, do this weird grid shift
                offset.x += offset.y * stepX;
                offset.y += offset.x * stepY;

                mainContext.SetRenderTarget( m_SSGBuffer->GetOutputColor( ), nullptr, true );

                m_camera->SetViewportSize( m_SSGBuffer->GetResolution().x, m_SSGBuffer->GetResolution().y );
                m_camera->SetSubpixelOffset( offset );
                m_camera->Tick(0, false);

                bool dummy = false;
                drawResults |= DrawScene( *m_camera, *m_SSGBuffer, nullptr, dummy, mainViewport, globalSSSettings, jx != 0 || jy != 0 );
                assert( !dummy );

                mainContext.SetRenderTarget( m_SSAccumulationColor, nullptr, true );

                if( SSResScale == 4 )
                {
                    // first downsample from 4x4 to 1x1
                    drawResults |= GetRenderDevice().GetPostProcess().Downsample4x4to1x1( mainContext, m_GBuffer->GetOutputColor(), m_SSGBuffer->GetOutputColor(), 0.0f );

                    // then accumulate
                    drawResults |= GetRenderDevice().GetPostProcess().StretchRect( mainContext, m_GBuffer->GetOutputColor(), vaVector4( 0.0f, 0.0f, (float)m_GBuffer->GetResolution().x, (float)m_GBuffer->GetResolution().y ), 
                        vaVector4( 0.0f, 0.0f, (float)mainViewport.Width, (float)mainViewport.Height ), true, vaBlendMode::Additive, vaVector4( addMult, addMult, addMult, addMult ) );
                }
                else
                {
                    assert( SSResScale == 1 || SSResScale == 2 );
                    
                    // downsample and accumulate
                    drawResults |= GetRenderDevice().GetPostProcess().StretchRect( mainContext, m_SSGBuffer->GetOutputColor(), vaVector4( 0.0f, 0.0f, (float)m_SSGBuffer->GetResolution().x, (float)m_SSGBuffer->GetResolution().y ), 
                        vaVector4( 0.0f, 0.0f, (float)mainViewport.Width, (float)mainViewport.Height ), true, vaBlendMode::Additive, vaVector4( addMult, addMult, addMult, addMult ) );
                }
            }
        }
        // copy from accumulation
        mainContext.SetRenderTarget( m_scratchPostProcessColor, nullptr, true );
        drawResults |= GetRenderDevice().GetPostProcess().StretchRect( mainContext, m_SSAccumulationColor, vaVector4( 0.0f, 0.0f, (float)mainViewport.Width, (float)mainViewport.Height ), vaVector4( 0.0f, 0.0f, (float)mainViewport.Width, (float)mainViewport.Height ), true, vaBlendMode::Opaque );

        // sharpen
        if( SSSharpen > 0 )
            drawResults |= GetRenderDevice().GetPostProcess().SimpleBlurSharpen( mainContext, m_GBuffer->GetOutputColor(), m_scratchPostProcessColor, SSSharpen );
        else
            m_GBuffer->GetOutputColor()->CopyFrom( mainContext, m_scratchPostProcessColor );

        // restore to old RT
        mainContext.SetRenderTarget( m_GBuffer->GetOutputColor(), nullptr, true );

        // restore camera
        m_camera->SetViewportSize( mainViewport.Width, mainViewport.Height );
        m_camera->SetSubpixelOffset( );
        m_camera->Tick(0, false);
#endif
    }
    else
    {
        m_SSGBuffer = nullptr;

        drawResults |= DrawScene( *m_camera, *m_GBuffer, m_scratchPostProcessColor, colorScratchContainsFinal, mainViewport );
    }

    {
        VA_TRACE_CPU_SCOPE( VanillaSample_RenderTick_UpdateCursor );

        vaSceneDrawContext drawContext( mainContext, *m_camera, vaDrawContextOutputType::DepthOnly );

        // 3D cursor stuff -> maybe best to do it before transparencies
#if 0
        if( !m_autoBench->IsActive() )
        {
            vaVector2i mousePos = vaInputMouse::GetInstance().GetCursorClientPosDirect();
            
            GetRenderDevice().GetRenderGlobals().Update3DCursor( drawContext, m_GBuffer->GetDepthBuffer(), (vaVector2)mousePos );
            vaVector4 mouseCursorWorld = GetRenderDevice().GetRenderGlobals().GetLast3DCursorInfo();
            m_mouseCursor3DWorldPosition = mouseCursorWorld.AsVec3();
            // GetRenderDevice().GetCanvas3D().DrawSphere( mouseCursorWorld.AsVec3(), 0.1f, 0xFF0000FF ); //, 0xFFFFFFFF );
            
            // since this is where we got the info, inform scene about any clicks here to reduce lag - not really a "rendering" type of call though
            if( m_currentScene != nullptr 
#ifdef VA_IMGUI_INTEGRATION_ENABLED
                && !ImGui::GetIO().WantCaptureMouse 
#endif
                && vaInputMouseBase::GetCurrent()->IsKeyClicked( MK_Left ) )
                m_currentScene->OnMouseClick( m_mouseCursor3DWorldPosition );
        }
#endif
    }

    // we haven't used supersampling for x frames? release the texture
    m_SSBuffersLastUsed++;
    if( m_SSBuffersLastUsed > 1 )
    {
        m_SSAccumulationColor = nullptr;
        m_SSScratchColor = nullptr;
        m_SSGBuffer = nullptr;
    }

    // draw selection no longer needed
    m_selectedOpaque.Reset( );
    m_selectedTransparent.Reset();

    shared_ptr<vaTexture> finalOutColor                     = m_GBuffer->GetOutputColor( );
    shared_ptr<vaTexture> finalOutColorIgnoreSRGBConvView   = m_GBuffer->GetOutputColorIgnoreSRGBConvView( );
    if( colorScratchContainsFinal )
    {
        finalOutColor = m_scratchPostProcessColor;
        finalOutColorIgnoreSRGBConvView = m_scratchPostProcessColor; //m_scratchPostProcessColorIgnoreSRGBConvView;
    }

    // tick benchmark/testing scripts before any image tools
    m_currentFrameTexture = finalOutColor;
    m_miniScript.TickScript( m_lastDeltaTime );
    m_currentFrameTexture = nullptr;

    // various helper tools - at one point these should go and become part of the base app but for now let's just stick them in here
    {
        if( drawResults == vaDrawResultFlags::None && m_imageCompareTool != nullptr )
        {
            m_imageCompareTool->RenderTick( mainContext, finalOutColor );
        }

        m_zoomTool->Draw( mainContext, finalOutColorIgnoreSRGBConvView );
    }

    // restore main display buffers
    mainContext.SetRenderTarget( GetRenderDevice().GetCurrentBackbuffer(), nullptr, true );


#ifdef VA_ENABLE_TEXTURE_REDUCTION_TOOL
    if( vaTextureReductionTestTool::GetInstancePtr( ) != nullptr && ( m_currentDrawResults == vaDrawResultFlags::None ) && m_shadowsStable && m_IBLsStable )
    {
        vaTextureReductionTestTool::GetInstance( ).TickGPU( mainContext, finalOutColor );
    }
#endif

    // Final apply to screen (redundant copy for now, leftover from expanded screen thing)
    {
        VA_TRACE_CPUGPU_SCOPE( FinalApply, mainContext );

        GetRenderDevice().GetPostProcess().StretchRect( mainContext, finalOutColor, vaVector4( ( float )0.0f, ( float )0.0f, (float)mainViewport.Width, (float)mainViewport.Height ), vaVector4( 0.0f, 0.0f, (float)mainViewport.Width, (float)mainViewport.Height ), false );
        //mainViewport = mainViewportBackup;
    }

    // keyboard-based selection
    {
        if( m_application.HasFocus( ) && !vaInputMouseBase::GetCurrent( )->IsCaptured( ) 
#ifdef VA_IMGUI_INTEGRATION_ENABLED
            && !ImGui::GetIO( ).WantCaptureKeyboard 
#endif
            )
        {
#if 0
            auto keyboard = vaInputKeyboardBase::GetCurrent( );
            if( !keyboard->IsKeyDown( vaKeyboardKeys::KK_SHIFT ) && !keyboard->IsKeyDown( vaKeyboardKeys::KK_CONTROL ) && !keyboard->IsKeyDown( vaKeyboardKeys::KK_ALT ) )
            {
                if( keyboard->IsKeyClicked( ( vaKeyboardKeys )'1' ) )       m_settings.VariableRateShadingOption = VanillaSample::AAType::None;
                if( keyboard->IsKeyClicked( ( vaKeyboardKeys )'2' ) )       m_settings.VariableRateShadingOption = VanillaSample::AAType::FXAA;
                if( keyboard->IsKeyClicked( ( vaKeyboardKeys )'3' ) )       m_settings.VariableRateShadingOption = VanillaSample::AAType::CMAA2;
                if( keyboard->IsKeyClicked( ( vaKeyboardKeys )'4' ) )       m_settings.VariableRateShadingOption = VanillaSample::AAType::SMAA;
                if( keyboard->IsKeyClicked( ( vaKeyboardKeys )'5' ) )       m_settings.VariableRateShadingOption = VanillaSample::AAType::MSAA2x;
                if( keyboard->IsKeyClicked( ( vaKeyboardKeys )'6' ) )       m_settings.VariableRateShadingOption = VanillaSample::AAType::MSAA2xPlusCMAA2;
                if( keyboard->IsKeyClicked( ( vaKeyboardKeys )'7' ) )       m_settings.VariableRateShadingOption = VanillaSample::AAType::SMAA_S2x;
                if( keyboard->IsKeyClicked( ( vaKeyboardKeys )'8' ) )       m_settings.VariableRateShadingOption = VanillaSample::AAType::MSAA4x;
                if( keyboard->IsKeyClicked( ( vaKeyboardKeys )'9' ) )       m_settings.VariableRateShadingOption = VanillaSample::AAType::MSAA4xPlusCMAA2;
                if( keyboard->IsKeyClicked( ( vaKeyboardKeys )'0' ) )       m_settings.VariableRateShadingOption = VanillaSample::AAType::MSAA8x;
                if( keyboard->IsKeyClicked( vaKeyboardKeys::KK_OEM_MINUS ) )m_settings.VariableRateShadingOption = VanillaSample::AAType::MSAA8xPlusCMAA2;
                if( keyboard->IsKeyClicked( vaKeyboardKeys::KK_OEM_PLUS ) ) m_settings.VariableRateShadingOption = VanillaSample::AAType::SuperSampleReference;
            }
#endif
        }
    }

    return drawResults;
}

void VanillaSample::OnSerializeSettings( vaXMLSerializer & serializer )
{
    m_settings.Serialize( serializer );

    if( serializer.SerializeOpenChildElement( "DoFSettings" ) )
    {
        m_DepthOfField->Settings().Serialize( serializer );

        bool ok = serializer.SerializePopToParentElement( "DoFSettings" );
        assert( ok ); ok;
    }

    m_lastSettings = m_settings;
}

void VanillaSample::UIPanelTick( vaApplicationBase & application )
{
    application;
    //ImGui::Checkbox( "Texturing disabled", &m_debugTexturingDisabled );

#ifdef VA_IMGUI_INTEGRATION_ENABLED

    if( m_miniScript.IsActive( ) )
    {
        ImGui::Text( "SCRIPT ACTIVE" );
        ImGui::Text( "" );
        m_miniScript.TickUI();
        return;
    }
   
    if( !IsAllLoadedPrecomputedAndStable() )
    {
        ImGui::Text("Asset/shader still loading or compiling");
        ImGui::NewLine();
        ImGui::NewLine();
        ImGui::Separator();
    }

    ImGui::PushItemWidth( 180.0f );

    ImGui::Text( "Use F1 to hide/show UI" );

    ImGui::Separator();

    ImGui::Checkbox( "Flythrough animation", &m_cameraFlythroughPlay );
    if( m_cameraFlythroughPlay )
    {
        ImGui::Indent( );
        ImGui::PushID( "FFCUI" );
        m_cameraFlythroughController->UIPropertiesItemTick( m_application );
        ImGui::PopID( );
        ImGui::Unindent( );
    }

    ImGuiEx_Combo( "Shading complexity", m_settings.ShadingComplexity, { "LOW", "MEDIUM", "HIGH" } );

    ImGui::Checkbox( "Show wireframe", &m_settings.ShowWireframe );
    if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Wireframe" );

    ImGui::Separator( );

    {
        if( !GetRenderDevice( ).GetCapabilities( ).VariableShadingRate.Tier1 )
            ImGui::TextColored( {1.0f, 0.2f, 0.2f, 1.0f}, "VRS is not supported by the device" );

        VariableRateShadingType vrsOption = m_settings.VariableRateShadingOption;
        std::vector<string> vals((int)VariableRateShadingType::MaxValue);
        for( int i = 0; i < vals.size(); i++ ) 
        {
            vals[i] = GetVRSOptionName( (VariableRateShadingType) i );
            if( !GetVRSOptionSupported( (VariableRateShadingType) i ) )
                vals[i] = "(N/A) " + vals[i];
        }

        ImGui::ListBox( "VRS option", (int*)&vrsOption, ImguiEx_VectorOfStringGetter, (void*)&vals, (int)vals.size(), (int)vals.size() );
        if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Various VRS rates - use Tier1_DoF_Driven for per-object VRS based on the amount of blur receiving from the DoF effect" );

        if( vrsOption != m_settings.VariableRateShadingOption )
            SetVRSOption( vrsOption );

        if( m_settings.VariableRateShadingOption == VanillaSample::VariableRateShadingType::Tier1_DoF_Driven ) 
        {
            ImGui::Text("DoF-driven VRS settings:");
            ImGui::Indent();
            ImGui::SliderFloat( "Transition offset", &m_settings.DoFDrivenVRSTransitionOffset, -1.0f, 1.0f );
            if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Offset the point where to start applying VRS; 0.0 is default; negative value will delay VRS while positive will introduce it sooner (probably not very useful)" );
            ImGuiEx_Combo( "Max VRS rate", m_settings.DoFDrivenVRSMaxRate,  { "1x1", "2x1", "2x2", "4x2", "4x4" } );
            if( ImGui::IsItemHovered( ) ) ImGui::SetTooltip( "Maximum VRS to apply when objects are fully blurred by DoF effect" );

            ImGui::Checkbox( "Sort draw calls by VRS shading rate", &m_settings.SortByVRS );
            ImGui::Checkbox( "Show VRS visualization", &m_settings.VisualizeVRS );
            ImGui::Unindent();
            ImGui::Separator();
        }
    }

    ImGui::Separator();
#if defined( VA_INTEL_GRADFILTER_ENABLED )
    ImGui::Text( "!!!THIS IS FOR GRADIENT FILTER EXTENSION TESTING!!!");
    if( ImGui::Checkbox( "EnableGradientFilterExtension", &m_settings.EnableGradientFilterExtension) )
    {
    }
    ImGui::Text( "!!!THIS IS FOR GRADIENT FILTER EXTENSION TESTING!!!");
    ImGui::Separator();
#endif

    // manually show the DoF UI here in-place, not as a separate window
    m_DepthOfField->UIPanelSetVisible( false );
    ImGui::Checkbox( "Depth of Field", &m_settings.EnableDOF );
    if( m_settings.EnableDOF )
    {
        if( ImGui::CollapsingHeader( "DoF settings", ImGuiTreeNodeFlags_Framed ) )
        {
            ImGui::SliderFloat( "Focal length", &m_settings.DoFFocalLength, 0.0f, 100.0f, "%.3f", 2.0f );
            ImGui::SliderFloat( "DoF range", &m_settings.DoFRange, 0.0f, 1.0f );
            ImGui::Indent();
            static_cast<vaUIPanel&>(*m_DepthOfField).UIPanelTick( application );
            ImGui::Unindent();
        }
    }

    // ImGuiEx_Combo( "SuperSampling", (int&)m_settings.SuperSamplingOption, { "Disabled", "2x", "4x" } );
    ImGui::Separator();

    // Benchmarking/scripting
    // if( m_settings.SceneChoice == VanillaSample::SceneSelectionType::LumberyardBistro )
    {
        ScriptedTests( application );
    }

    ImGui::PopItemWidth( );


#endif
}

namespace Vanilla
{
    class AutoBenchTool
    {
        VanillaSample& m_parent;
        vaMiniScriptInterface& m_scriptInterface;

        wstring                                 m_reportDir;
        wstring                                 m_reportName;
        vector<vector<string>>                  m_reportCSV;
        string                                  m_reportTXT;

        // backups
        vaRenderCamera::AllSettings             m_backupCameraSettings;
        vaMemoryStream                          m_backupCameraStorage;
        VanillaSample::VanillaSampleSettings    m_backupSettings;
        vaDepthOfField::DoFSettings             m_backupDoFSettings;
        float                                   m_backupFlythroughCameraTime;
        float                                   m_backupFlythroughCameraSpeed;
        bool                                    m_backupFlythroughCameraEnabled;

        string                                  m_statusInfo = "-";
        bool                                    m_shouldStop = false;

    public:
        AutoBenchTool( VanillaSample& parent, vaMiniScriptInterface& scriptInterface, bool ensureVisualDeterminism, bool writeReport );
        ~AutoBenchTool( );

        void                                    ReportAddRowValues( const vector<string>& row ) { m_reportCSV.push_back( row ); FlushRowValues( ); }
        void                                    ReportAddText( const string& text ) { m_reportTXT += text; }
        wstring                                 ReportGetDir( ) { return m_reportDir; }

        void                                    SetUIStatusInfo( const string& statusInfo ) { m_statusInfo = statusInfo; }
        bool                                    GetShouldStop( ) const { return m_shouldStop; }

    private:
        void                                    FlushRowValues( );
    };
}

void VanillaSample::ScriptedTests( vaApplicationBase & application )
{
    application;

    assert( !m_miniScript.IsActive() );
    if( m_miniScript.IsActive() )
        return;

    ImGuiTreeNodeFlags headerFlags = 0;
    // headerFlags |= ImGuiTreeNodeFlags_Framed;
    // headerFlags |= ImGuiTreeNodeFlags_DefaultOpen;
    if( !ImGui::CollapsingHeader( "Scripted tests", headerFlags ) )
        return;

    ImGui::Separator( );

    if( ImGui::Button("Run quality analysis") )
    {
        m_miniScript.Start( [ thisPtr = this] (vaMiniScriptInterface & msi)
        {
            // this sets up some globals and also backups all the sample settings
            AutoBenchTool autobench( *thisPtr, msi, true, true );

            // enable DoF for quality analysis
            thisPtr->Settings().EnableDOF = true;

            // animation stuff
            const float c_framePerSecond    = 30;
            const float c_frameDeltaTime    = 1.0f / (float)c_framePerSecond;
            const int   c_totalFrameCount   = (int)(thisPtr->GetFlythroughCameraController()->GetTotalTime() / c_frameDeltaTime);
            const int   c_totalFramesToTest = 8;
            vector<float> timePointsToTest;
            // instead of manually settings them just spread them out over c_totalFramesToTest locations, should be good enough
            for( int i = 0; i < c_totalFramesToTest; i++ )
                timePointsToTest.push_back( c_frameDeltaTime * ((float)c_totalFrameCount*((float)i+0.5f)/(float)c_totalFramesToTest) );
            thisPtr->SetFlythroughCameraEnabled( true );
            thisPtr->GetFlythroughCameraController( )->SetPlaySpeed( 0.0f );

            // allocate and zero out storage for averages
            float avgMSE[(int)VariableRateShadingType::MaxValue-1];
            for( VariableRateShadingType rpt = (VariableRateShadingType)1; rpt < VariableRateShadingType::MaxValue; rpt = (VariableRateShadingType)(int(rpt)+1) )
                avgMSE[(int)rpt-1] = 0.0f;

            // info
            {
                autobench.ReportAddText( "\r\nQuality testing of various VRS modes; metric is PSNR and reference is identical frame image with VRS disabled\r\n" );

                vector<string> reportRow;
                reportRow.push_back( "Frame" );
                for( VariableRateShadingType rpt = (VariableRateShadingType)1; rpt < VariableRateShadingType::MaxValue; rpt = (VariableRateShadingType)(int(rpt)+1) )
                {
                    if( !thisPtr->GetVRSOptionSupported( rpt) )                    
                        reportRow.push_back( "NotAvailable" );
                    else
                        reportRow.push_back( thisPtr->GetVRSOptionName( rpt ) );
                }
                autobench.ReportAddRowValues(reportRow);
            }

            // ok let's go
            for( int testFrame = 0; testFrame < timePointsToTest.size(); testFrame++ )
            {
                thisPtr->GetFlythroughCameraController( )->SetPlayTime( timePointsToTest[ testFrame ] );

                vector<string> reportRowPSNR;
                reportRowPSNR.push_back( vaStringTools::Format("%d", testFrame) );

                for( VariableRateShadingType rpt = (VariableRateShadingType)0; rpt < VariableRateShadingType::MaxValue; rpt = (VariableRateShadingType)(int(rpt)+1) )
                {
                    thisPtr->SetVRSOption( rpt );
                    autobench.SetUIStatusInfo( "running " + (string)thisPtr->GetVRSOptionName( rpt ) );

                    // run however many frames needed to get IsAllLoadedPrecomputedAndStable to be true, plus then make sure tonemapping has stabilized
                    for( int ii = 0; ii < vaRenderCamera::c_backbufferCount+10; )
                    { 
                        if( !msi.YieldExecutionFor( 1 ) || autobench.GetShouldStop() )
                            return;
                        if( thisPtr->IsAllLoadedPrecomputedAndStable( ) )
                            ii++;
                    }

                    // capture reference
                    if( rpt == VanillaSample::VariableRateShadingType::None )
                    {
                        thisPtr->ImageCompareTool()->SaveAsReference( *thisPtr->GetRenderDevice().GetMainContext(), thisPtr->CurrentFrameTexture() );
                        VA_LOG_SUCCESS( " reference captured...");
                    }
                    else
                    {
                        vaVector4 diff = thisPtr->ImageCompareTool()->CompareWithReference( *thisPtr->GetRenderDevice().GetMainContext(), thisPtr->CurrentFrameTexture() );

                        if( diff.x == -1 )
                        {
                            reportRowPSNR.push_back( "error" );
                            VA_LOG_ERROR( "Error: Reference image not captured, or size/format mismatch - please capture a reference image first." );
                        }
                        else
                        {
                            reportRowPSNR.push_back( vaStringTools::Format( "%.3f", diff.y ) );
                            avgMSE[(int)rpt-1] += diff.x / (float)timePointsToTest.size();
                            VA_LOG_SUCCESS( " '%30s' : PSNR: %.3f (MSE: %f)", thisPtr->GetVRSOptionName( rpt ), diff.y, diff.x );
                        }
                    }

                    wstring folderName = autobench.ReportGetDir() + vaStringTools::SimpleWiden( thisPtr->GetVRSOptionName( rpt ) ) + L"\\";
                    vaFileTools::EnsureDirectoryExists( folderName );
                    thisPtr->CurrentFrameTexture()->SaveToPNGFile( *thisPtr->GetRenderDevice().GetMainContext(), 
                        folderName + vaStringTools::Format( L"frame_%04d.png", testFrame ) );
                }

                autobench.ReportAddRowValues( reportRowPSNR );
            }
            vector<string> reportRowAvgPSNR;
            reportRowAvgPSNR.push_back( vaStringTools::Format("PSNR avg") );
            for( VariableRateShadingType rpt = (VariableRateShadingType)1; rpt < VariableRateShadingType::MaxValue; rpt = (VariableRateShadingType)(int(rpt)+1) )
                reportRowAvgPSNR.push_back( vaStringTools::Format( "%.3f", vaMath::PSNR(avgMSE[(int)rpt-1], 1.0f) ) );
            autobench.ReportAddRowValues( reportRowAvgPSNR );
        } );
    }

    bool isDebug = false;
#ifdef _DEBUG
//        isDebug = true;
#endif

    ImGui::Separator( );

    if( isDebug )
    {
        ImGui::Text( "Perf analysis doesn't work in debug builds" );
    }
    else 
        if( ImGui::Button("Run perf analysis") )
    {
        m_miniScript.Start( [ thisPtr = this] (vaMiniScriptInterface & msi)
        {
            // this sets up some globals and also backups all the sample settings
            AutoBenchTool autobench( *thisPtr, msi, false, true );

            // disable DoF for these tests
            thisPtr->Settings().EnableDOF = true; // false;

            // animation stuff
            const float c_framePerSecond    = 10;
            const float c_frameDeltaTime    = 1.0f / (float)c_framePerSecond;
            const float c_totalTime         = thisPtr->GetFlythroughCameraController()->GetTotalTime();
            const int   c_totalFrameCount   = (int)(c_totalTime / c_frameDeltaTime);
            thisPtr->SetFlythroughCameraEnabled( true );
            thisPtr->GetFlythroughCameraController( )->SetPlaySpeed( 0.0f );

            // info
            vector<string> columnHeadersRow;
            {
                autobench.ReportAddText( "\r\nPerformance testing of various VRS modes - measuring average time (milliseconds) spent in per frame \r\nand in 'Forward' and 'Transparency' passes (F+T) which are the only ones using VRS.\r\n" );

                columnHeadersRow.push_back( "" );
                for( VariableRateShadingType rpt = (VariableRateShadingType)0; rpt < VariableRateShadingType::MaxValue; rpt = (VariableRateShadingType)(int(rpt)+1) )
                {
                    if( !thisPtr->GetVRSOptionSupported( rpt) )                    
                        columnHeadersRow.push_back( "NotAvailable" );
                    else
                        columnHeadersRow.push_back( thisPtr->GetVRSOptionName( rpt ) );
                }
            }

            auto tracerView = std::make_shared<vaTracerView>( );

            // testPass: 0: standard pass, 1: gradient filter
            for( int testPass = 0; testPass < 2; testPass++ ) 
            {
                autobench.ReportAddText( vaStringTools::Format( "\r\nPASS %d: ", testPass ) );  

                bool gradientFilterEnable = testPass == 1;
                if( gradientFilterEnable )
                {
#if defined( VA_INTEL_GRADFILTER_ENABLED )
                    autobench.ReportAddText( "Gradient Filter enabled!" );  
#else
                    autobench.ReportAddText( "VA_INTEL_GRADFILTER_ENABLED not set, can't test gradient filter performance" );  
                    continue;
#endif
                }
                autobench.ReportAddText( "\r\n" );  

                thisPtr->Settings().EnableGradientFilterExtension = gradientFilterEnable;

                autobench.ReportAddRowValues(columnHeadersRow);

                for( int shadingComplexity = 0; shadingComplexity < 3; shadingComplexity++ )
                //int shadingComplexity = 1;
                {
                    const char * scString[] = { "LOW", "MEDIUM", "HIGH" };
                    autobench.ReportAddText( vaStringTools::Format( "\r\nShading complexity: %s\r\n", scString[shadingComplexity] ) );
                    thisPtr->Settings().ShadingComplexity = shadingComplexity;
                    vector<string> reportRowAvgTime, reportRowAvgTimeForward;
                    reportRowAvgTime.push_back( "Avg frame time (ms)" );
                    reportRowAvgTimeForward.push_back( "Avg F+T time (ms)" );

                    // do an empty run first
                    bool warmupPass = true;
                    for( VariableRateShadingType rpt = (VariableRateShadingType)0; rpt < VariableRateShadingType::MaxValue; rpt = (VariableRateShadingType)(int(rpt)+1) )
                    {
                        thisPtr->SetVRSOption( rpt );
                
                        string status = "running ";
                        status += (string)thisPtr->GetVRSOptionName( rpt );
                        if( warmupPass )
                            status += " (warmup pass)";
                
                        autobench.SetUIStatusInfo( status + ", preparing..."  );

                        thisPtr->GetFlythroughCameraController( )->SetPlayTime( 0 );

                        // wait until IsAllLoadedPrecomputedAndStable and then run 4 more loops - this ensures the numbers from the vaProfiler 
                        // are not from previous test case.
                        int startupLoops = 3;
                        do
                        { 
                            if( !thisPtr->IsAllLoadedPrecomputedAndStable( ) )
                                startupLoops = 3;
                            startupLoops--;
                            if( !msi.YieldExecutionFor( 1 ) || autobench.GetShouldStop() )
                                return;
                        }
                        while( startupLoops > 0 );
                

                        using namespace std::chrono;
                        high_resolution_clock::time_point t1 = high_resolution_clock::now();

                        //float totalTime = 0.0f;
                        float totalTimeInForward = 0.0f;

                        if( !warmupPass )
                            tracerView->ConnectToThreadContext( string(vaGPUContextTracer::c_threadNamePrefix) + "*", VA_FLOAT_HIGHEST );

                        // ok let's go
                        for( int testFrame = 0; testFrame < c_totalFrameCount; testFrame++ )
                        {
                            autobench.SetUIStatusInfo( "shading complexity " + string(scString[shadingComplexity]) + ", " + status + ", " + vaStringTools::Format( "%.1f%%", (float)testFrame/(float)(c_totalFrameCount-1)*100.0f ) );
                    
                            thisPtr->GetFlythroughCameraController( )->SetPlayTime( testFrame * c_frameDeltaTime );
                            if( !msi.YieldExecution( ) || autobench.GetShouldStop() )
                                return;

                            // totalTime += msi.GetDeltaTime();
                            if( warmupPass )
                                testFrame += 3;
                        }

                        if( !warmupPass )
                        {
                            tracerView->Disconnect();
                            const vaTracerView::Node * node = tracerView->FindNodeRecursive( "Forward" );
                            if( node != nullptr )
                            {
                                totalTimeInForward = (float)node->TimeTotal;
                                assert( node->Instances == c_totalFrameCount );
                            }
                            node = tracerView->FindNodeRecursive( "Transparencies" );
                            if( node != nullptr )
                            {
                                totalTimeInForward += (float)node->TimeTotal;
                                assert( node->Instances == c_totalFrameCount );
                            }
                        }

                        high_resolution_clock::time_point t2 = high_resolution_clock::now();
                        float totalTime = (float)duration_cast<duration<double>>(t2 - t1).count();

                        if( warmupPass )
                        {
                            rpt = (VariableRateShadingType)((int)rpt-1); //restarts at 0
                            warmupPass = false;
                        }
                        else
                        {
                            reportRowAvgTime.push_back( vaStringTools::Format("%.2f", totalTime * 1000.0f / (float)c_totalFrameCount) );
                            reportRowAvgTimeForward.push_back( vaStringTools::Format("%.2f", totalTimeInForward * 1000.0f / (float)c_totalFrameCount) );
                        }
                    }
                    autobench.ReportAddRowValues( reportRowAvgTime );
                    autobench.ReportAddRowValues( reportRowAvgTimeForward );
                }
                autobench.ReportAddText( "\r\n" );
            }
        } );        
    }

    ImGui::Separator( );

    // for conversion to mpeg one option is to download ffmpeg and then do 'ffmpeg -r 60 -f image2 -s 1920x1080 -i frame_%05d.png -vcodec libx264 -crf 13  -pix_fmt yuv420p outputvideo.mp4'
    if( ImGui::Button( "Record a flythrough" ) )
    {
        m_miniScript.Start( [ thisPtr = this ]( vaMiniScriptInterface& msi )
        {
            auto userSettings = thisPtr->Settings( );

            // this sets up some globals and also backups all the sample settings
            AutoBenchTool autobench( *thisPtr, msi, false, true );

            thisPtr->Settings( ) = userSettings;

            // for the flythrough recording, use the user settings

            // animation stuff
            const float c_framePerSecond = 60;
            const float c_frameDeltaTime = 1.0f / (float)c_framePerSecond;
            const float c_totalTime = thisPtr->GetFlythroughCameraController( )->GetTotalTime( );
            const int   c_totalFrameCount = (int)( c_totalTime / c_frameDeltaTime );
            thisPtr->SetFlythroughCameraEnabled( true );
            thisPtr->GetFlythroughCameraController( )->SetPlaySpeed( 0.0f );

            // info
            autobench.ReportAddText( "\r\nRecording a flythrough with current settings\r\n" );

            autobench.ReportAddText( "\r\n" );

            const char* scString[] = { "LOW", "MEDIUM", "HIGH" };
            autobench.ReportAddText( vaStringTools::Format( "\r\nShading complexity: %s\r\n", scString[thisPtr->Settings().ShadingComplexity] ) );

            string status = "running ";

            autobench.SetUIStatusInfo( status + ", preparing..." );

            thisPtr->GetFlythroughCameraController( )->SetPlayTime( 0 );

            int width = thisPtr->CurrentFrameTexture( )->GetWidth( )/2;
            int height = thisPtr->CurrentFrameTexture( )->GetHeight( )/2;
            shared_ptr<vaTexture> lowResTexture = vaTexture::Create2D( thisPtr->GetRenderDevice(), vaResourceFormat::R8G8B8A8_UNORM_SRGB, width, height, 1, 1, 1, vaResourceBindSupportFlags::ShaderResource | vaResourceBindSupportFlags::RenderTarget );

            // wait until IsAllLoadedPrecomputedAndStable and then run 4 more loops - this ensures the numbers from the vaProfiler 
            // are not from previous test case.
            int startupLoops = 3;
            do
            {
                if( !thisPtr->IsAllLoadedPrecomputedAndStable( ) )
                    startupLoops = 3;
                startupLoops--;
                if( !msi.YieldExecutionFor( 1 ) || autobench.GetShouldStop( ) )
                    return;
            } while( startupLoops > 0 );

            bool warmupPass = true;
            for( int loop = 0; loop < 2; loop++, warmupPass = false )
            {
                // ok let's go
                for( int testFrame = 0; testFrame < c_totalFrameCount; testFrame++ )
                {
                    autobench.SetUIStatusInfo( status + ", " + vaStringTools::Format( "%.1f%%", (float)testFrame / (float)( c_totalFrameCount - 1 ) * 100.0f ) );

                    thisPtr->GetFlythroughCameraController( )->SetPlayTime( testFrame * c_frameDeltaTime );
                    if( !msi.YieldExecution( ) || autobench.GetShouldStop( ) )
                        return;

                    if( warmupPass )
                        testFrame += 10;
                    else
                    {
                        wstring folderName = autobench.ReportGetDir( );
                        vaFileTools::EnsureDirectoryExists( folderName );

                        thisPtr->GetRenderDevice( ).GetMainContext( )->StretchRect( lowResTexture, thisPtr->CurrentFrameTexture( ), {0,0,0,0} );

                        // thisPtr->CurrentFrameTexture( )->SaveToPNGFile( *thisPtr->GetRenderDevice( ).GetMainContext( ),
                        //     folderName + vaStringTools::Format( L"frame_%05d.png", testFrame ) );

                        lowResTexture->SaveToPNGFile( *thisPtr->GetRenderDevice( ).GetMainContext( ),
                            folderName + vaStringTools::Format( L"hframe_%05d.png", testFrame ) );
                    }
                }
            }

            autobench.ReportAddText( "\r\n" );
        } );
    }
    // for conversion to mpeg one option is to download ffmpeg and then do 'ffmpeg -r 60 -f image2 -s 1920x1080 -i frame_%05d.png -vcodec libx264 -crf 13  -pix_fmt yuv420p outputvideo.mp4'

    // small helper - loop until quality loss stable
    auto setVRSAndLoopUntilStable = [ ]( VariableRateShadingType vrsType, VanillaSample* thisPtr, vaMiniScriptInterface& msi, AutoBenchTool& autobench )
    {
        thisPtr->SetVRSOption( vrsType );
        // run however many frames needed to get IsAllLoadedPrecomputedAndStable to be true, plus then make sure tonemapping has stabilized*/
        for( int ii = 0; ii < ( vaRenderCamera::c_backbufferCount * 2 ) + 1; )
        {
            if( !msi.YieldExecutionFor( 1 ) || autobench.GetShouldStop( ) )
                return false;
            if( thisPtr->IsAllLoadedPrecomputedAndStable( ) )
                ii++;
        }
        return true;
    };

    ImGui::Separator( );

    if( ImGui::Button( "Optimize material horiz/vert VRS preference" ) )
    {
        m_miniScript.Start( [ thisPtr = this, setVRSAndLoopUntilStable ]( vaMiniScriptInterface& msi )
        {
            // this sets up some globals and also backups all the sample settings
            AutoBenchTool autobench( *thisPtr, msi, true, true );

            // no need to enable DoF for this analysis
            thisPtr->Settings( ).EnableDOF = false;

            // animation stuff
            const float c_framePerSecond = 30;
            const float c_frameDeltaTime = 1.0f / (float)c_framePerSecond;
            const int   c_totalFrameCount = (int)( thisPtr->GetFlythroughCameraController( )->GetTotalTime( ) / c_frameDeltaTime );
            const int   c_totalFramesToTest = 32;
            vector<float> timePointsToTest;
            // instead of manually settings them just spread them out over c_totalFramesToTest locations, should be good enough
            for( int i = 0; i < c_totalFramesToTest; i++ )
                timePointsToTest.push_back( c_frameDeltaTime * ( (float)c_totalFrameCount * ( (float)i + 0.5f ) / (float)c_totalFramesToTest ) );
            thisPtr->SetFlythroughCameraEnabled( true );
            thisPtr->GetFlythroughCameraController( )->SetPlaySpeed( 0.0f );

            // get a list of all materials to play with
            vaAssetPackManager & assetPackManager = thisPtr->m_renderDevice.GetAssetPackManager();
            vector<shared_ptr<vaAsset>> materials = assetPackManager.FindAssets( [] (vaAsset & asset) { return asset.Type == vaAssetType::RenderMaterial; } );

            // this is the level we test for v/h
            const VariableRateShadingType testShadingRate = VanillaSample::VariableRateShadingType::Tier1_Static_1x2_2x1;

            autobench.ReportAddText( vaStringTools::Format( "\r\nTesting %d materials to find best (horizontal or vertical) option for %s \r\n",
                (int)materials.size(), thisPtr->GetVRSOptionName( testShadingRate ) ) );

            autobench.ReportAddRowValues( {"Material name", "H/V preference", "horiz better", "vert better", "same" } );

            int totalChanges    = 0;
            int totalHoriz      = 0;
            int totalVert       = 0;
            int totalUnknown    = 0;

            for( int i = 0; i < materials.size( ); i++ )
            {
                autobench.SetUIStatusInfo( "running material " + std::to_string(i+1) + " out of " + std::to_string(materials.size()) );

                shared_ptr<vaRenderMaterial> material = materials[i]->GetResource<vaRenderMaterial>();
                if( material == nullptr )
                {
                    assert( false );
                    continue;
                }

                // negative is horizontal, positive is vertical
                int hvPreferenceScore = 0;
                auto originalMaterialSettings = material->GetMaterialSettings( );

                float hvScores[3] = { 0, 0, 0 };  // 0 - times horizontal is better, 1 - times vertical is better, 2 - times they're the same

                // going through ALL frames
                for( int testFrame = 0; testFrame < timePointsToTest.size( ); testFrame++ )
                {
                    thisPtr->GetFlythroughCameraController( )->SetPlayTime( timePointsToTest[testFrame] );

                    // ok, first the reference
                    if( !setVRSAndLoopUntilStable( VariableRateShadingType::None, thisPtr, msi, autobench ) )
                        return;

                    thisPtr->ImageCompareTool( )->SaveAsReference( *thisPtr->GetRenderDevice( ).GetMainContext( ), thisPtr->CurrentFrameTexture( ) );

                    // now compute the hvScores[hvPreference]
                    float hvMSRs[2];
                    for( int hvPreference = 0; hvPreference < 2; hvPreference++ )
                    {
                        auto materialSettings = originalMaterialSettings;
                        materialSettings.VRSPreferHorizontal = hvPreference == 0;
                        material->SetMaterialSettings( materialSettings );

                        if( !setVRSAndLoopUntilStable( testShadingRate, thisPtr, msi, autobench ) )
                            return;

                        vaVector4 diff = thisPtr->ImageCompareTool( )->CompareWithReference( *thisPtr->GetRenderDevice( ).GetMainContext( ), thisPtr->CurrentFrameTexture( ) );

                        if( diff.x == -1 )
                        {
                            VA_LOG_ERROR( "Error: Reference image not captured, or size/format mismatch - please capture a reference image first." );
                            hvMSRs[hvPreference] = 0;
                        }
                        else
                        {
                            hvMSRs[hvPreference] = diff.x;
                        }
                    }

                    // bigger score actually means bigger error
                    if( hvMSRs[0] != hvMSRs[1] )
                    {
                        hvPreferenceScore += (hvMSRs[0]>hvMSRs[1])?(1):(-1);
                        hvScores[(hvMSRs[0]>hvMSRs[1])?(1):(0)]++;
                    }
                    else
                        hvScores[2]++;
                }

                string materialReport;
                auto newMaterialSettings = originalMaterialSettings;
                if( hvPreferenceScore == 0 )
                {
                    materialReport = "no preference";
                    totalUnknown++;
                }
                else
                {
                    materialReport = (hvPreferenceScore<0)?("horizontal"):("vertical");
                    newMaterialSettings.VRSPreferHorizontal = (hvPreferenceScore<0);

                    material->SetMaterialSettings( newMaterialSettings );

                    if( newMaterialSettings.VRSPreferHorizontal != originalMaterialSettings.VRSPreferHorizontal )
                        totalChanges++;
                    totalHoriz      += (hvPreferenceScore<0) ?(1):(0);
                    totalVert       += (hvPreferenceScore>0) ?(1):(0);
                    totalUnknown    += (hvPreferenceScore==0)?(1):(0);
                }

                autobench.ReportAddRowValues( {material->GetParentAsset()->Name(), materialReport, std::to_string(hvScores[0]), std::to_string(hvScores[1]), std::to_string(hvScores[2]) } );
            }
            
            autobench.ReportAddText( "\r\n" );  

            autobench.ReportAddText( vaStringTools::Format("Total horiz: %d, total vert: %d, total no preference %d, total changes: %d", totalHoriz, totalVert, totalUnknown, totalChanges ) ); 
        } );
    }

    ImGui::Separator();

    ImGui::Text( "Material VRSRateOffset setup" );

    auto resetAllVRSRateOffsets = [ ]( vaAssetPackManager & assetPackManager )
    {
        int count = 0;
        vector<shared_ptr<vaAsset>> materials = assetPackManager.FindAssets( [ ]( vaAsset& asset ) { return asset.Type == vaAssetType::RenderMaterial; } );
        for( int i = 0; i < materials.size( ); i++ )
        {
            shared_ptr<vaRenderMaterial> material = materials[i]->GetResource<vaRenderMaterial>( );
            if( material != nullptr )
            {
                auto settings = material->GetMaterialSettings( );
                settings.VRSRateOffset = 0;
                material->SetMaterialSettings(settings);
                count++;
            }
        }
        VA_LOG_SUCCESS( "VRSRateOffset set to 0 for %d materials", count );
    };

    if( ImGui::Button( "Reset all VRS offsets to 0" ) )
        resetAllVRSRateOffsets( m_renderDevice.GetAssetPackManager( ) );

    ImGui::InputFloat( "AutoVRSRateOffsetThreshold", &m_settings.AutoVRSRateOffsetThreshold, 0.01f, 0.1f, "%.2f" );
    m_settings.AutoVRSRateOffsetThreshold = vaMath::Clamp( m_settings.AutoVRSRateOffsetThreshold, 0.0f, 2.0f );

    if( ImGui::Button( "Optimize material VRSRateOffsets" ) )
    {
        resetAllVRSRateOffsets( m_renderDevice.GetAssetPackManager( ) );
        m_miniScript.Start( [ thisPtr = this, setVRSAndLoopUntilStable, autoVRSRateOffsetThreshold = m_settings.AutoVRSRateOffsetThreshold ]( vaMiniScriptInterface& msi )
        {
            // this sets up some globals and also backups all the sample settings
            AutoBenchTool autobench( *thisPtr, msi, true, true );

            // no need to enable DoF for this analysis
            thisPtr->Settings( ).EnableDOF = false;

            // animation stuff
            const float c_framePerSecond = 30;
            const float c_frameDeltaTime = 1.0f / (float)c_framePerSecond;
            const int   c_totalFrameCount = (int)( thisPtr->GetFlythroughCameraController( )->GetTotalTime( ) / c_frameDeltaTime );
            const int   c_totalFramesToTest = 32;
            vector<float> timePointsToTest;
            // instead of manually settings them just spread them out over c_totalFramesToTest locations, should be good enough
            for( int i = 0; i < c_totalFramesToTest; i++ )
                timePointsToTest.push_back( c_frameDeltaTime * ( (float)c_totalFrameCount * ( (float)i + 0.5f ) / (float)c_totalFramesToTest ) );
            thisPtr->SetFlythroughCameraEnabled( true );
            thisPtr->GetFlythroughCameraController( )->SetPlaySpeed( 0.0f );

            // get a list of all materials to play with
            vector<shared_ptr<vaAsset>> materials = thisPtr->m_renderDevice.GetAssetPackManager( ).FindAssets( [ ]( vaAsset& asset ) { return asset.Type == vaAssetType::RenderMaterial; } );

            // this is the level we test for v/h
            const VariableRateShadingType testShadingRate = VanillaSample::VariableRateShadingType::Tier1_Static_2x2;
            int totalNumberOfDrops = (int)(autoVRSRateOffsetThreshold * materials.size());

            autobench.ReportAddText( vaStringTools::Format( "\r\nTesting %d materials to find those worst affected by VRS option for %s and then reducing their individual VRS rates, %d time in total\r\n",
                (int)materials.size( ), thisPtr->GetVRSOptionName( testShadingRate ), totalNumberOfDrops ) );

            autobench.ReportAddRowValues( { "Material name", "VRSRateOffset" } );

            for( int dropIndex = 0; dropIndex < totalNumberOfDrops; dropIndex++ )
            {
                float smallestMSE = std::numeric_limits<float>::max();
                shared_ptr<vaRenderMaterial> smallestMSEMaterial = nullptr;

                for( int i = 0; i < materials.size( ); i++ )
                {
                    autobench.SetUIStatusInfo( "looping " + std::to_string( dropIndex + 1 ) + " out of " + std::to_string( totalNumberOfDrops ) + ", material " + std::to_string(i) + " of " + std::to_string(materials.size()) );

                    shared_ptr<vaRenderMaterial> material = materials[i]->GetResource<vaRenderMaterial>( );
                    if( material == nullptr )
                    {
                        assert( false );
                        continue;
                    }

                    auto originalMaterialSettings = material->GetMaterialSettings( );

                    float avgMSE = 0.0f;

                    // now compute the improvement in quality that we get by dropping 1 level for this specific material
                    auto materialSettings = originalMaterialSettings;
                    materialSettings.VRSRateOffset = vaMath::Clamp( materialSettings.VRSRateOffset - 1, -2, 0 );
                    material->SetMaterialSettings( materialSettings );

                    // going through ALL frames
                    for( int testFrame = 0; testFrame < timePointsToTest.size( ); testFrame++ )
                    {
                        thisPtr->GetFlythroughCameraController( )->SetPlayTime( timePointsToTest[testFrame] );

                        // ok, first the reference
                        if( !setVRSAndLoopUntilStable( VariableRateShadingType::None, thisPtr, msi, autobench ) )
                            return;
                        thisPtr->ImageCompareTool( )->SaveAsReference( *thisPtr->GetRenderDevice( ).GetMainContext( ), thisPtr->CurrentFrameTexture( ) );

                        // now the one we're testing
                        if( !setVRSAndLoopUntilStable( testShadingRate, thisPtr, msi, autobench ) )
                            return;

                        vaVector4 diff = thisPtr->ImageCompareTool( )->CompareWithReference( *thisPtr->GetRenderDevice( ).GetMainContext( ), thisPtr->CurrentFrameTexture( ) );

                        if( diff.x == -1 )
                        {
                            VA_LOG_ERROR( "Error: Reference image not captured, or size/format mismatch - please capture a reference image first." );
                        }
                        else
                        {
                            avgMSE += diff.x;
                        }
                    }

                    material->SetMaterialSettings( originalMaterialSettings );

                    avgMSE /= (float)timePointsToTest.size( );

                    if( avgMSE < smallestMSE )
                    {
                        smallestMSE = avgMSE;
                        smallestMSEMaterial = material;
                    }

                }

                if( smallestMSEMaterial == nullptr )
                {
                    VA_ERROR( "Can't find more materials to drop VRS rate with VRSRateOffset and improve quality?" );
                    break;
                }
                else
                {
                    VA_LOG( "Next best material to reduce VRSRateOffset is %s", smallestMSEMaterial->GetParentAsset()->Name().c_str() );
                    auto materialSettings =  smallestMSEMaterial->GetMaterialSettings();
                    materialSettings.VRSRateOffset = vaMath::Clamp( materialSettings.VRSRateOffset - 1, -2, 0 );
                    smallestMSEMaterial->SetMaterialSettings( materialSettings );
                }
            }

            for( int i = 0; i < materials.size( ); i++ )
            {
                shared_ptr<vaRenderMaterial> material = materials[i]->GetResource<vaRenderMaterial>( );
                if( material != nullptr )
                    autobench.ReportAddRowValues( { material->GetParentAsset()->Name().c_str(), std::to_string( material->GetMaterialSettings().VRSRateOffset ) } );
            }

            autobench.ReportAddText( "\r\n" );
        } );
    }
    ImGui::Separator( );
}



AutoBenchTool::AutoBenchTool( VanillaSample & parent, vaMiniScriptInterface & scriptInterface, bool ensureVisualDeterminism, bool writeReport ) : m_parent( parent ), m_scriptInterface( scriptInterface ), m_backupCameraStorage( (int64)0, (int64)1024 ) 
{ 
    // must call this so we can call any stuff allowed only on the main thread
    vaThreading::SetSyncedWithMainThread();
    // must call this so we can call any rendering functions that are allowed only on the render thread
    vaRenderDevice::SetSyncedWithRenderThread();

    m_backupSettings                = m_parent.Settings();
    m_backupDoFSettings             = m_parent.DoFSettings();
    m_parent.Camera()->Save( m_backupCameraStorage ); m_backupCameraStorage.Seek(0);
    m_backupCameraSettings          = m_parent.Camera()->Settings();
    m_backupFlythroughCameraTime    = m_parent.GetFlythroughCameraController( )->GetPlayTime();
    m_backupFlythroughCameraSpeed   = m_parent.GetFlythroughCameraController( )->GetPlaySpeed();
    m_backupFlythroughCameraEnabled = m_parent.GetFlythroughCameraEnabled( );

    // disable so we can do our own views
    vaTracer::SetTracerViewingUIEnabled( false );

    // display script UI
    m_scriptInterface.SetUICallback( [&statusInfo = m_statusInfo, &shouldStop = m_shouldStop] 
    { 
        ImGui::TextColored( {0.3f, 0.3f, 1.0f, 1.0f}, "Script running, status:" );
        ImGui::Indent();
        ImGui::TextWrapped( statusInfo.c_str() );
        ImGui::Unindent();
        if( ImGui::Button("STOP SCRIPT") )
            shouldStop = true;
        ImGui::Separator();
    } );

    // use default settings
    m_parent.Settings().ShowWireframe                   = false;
    m_parent.Settings().CameraYFov                      = 50.0f / 180.0f * VA_PIf;
    m_parent.Settings().DisableTransparencies           = false;
    m_parent.Settings().SortByVRS                       = true;
    m_parent.Settings().VisualizeVRS                    = false;
    m_parent.Settings().DoFDrivenVRSTransitionOffset    = 0.05f;
    m_parent.Settings().DoFDrivenVRSMaxRate             = 3;
    m_parent.Settings().DoFFocalLength                  = 2.0f;
    m_parent.Settings().DoFRange                        = 0.3f;
    m_parent.Settings().EnableGradientFilterExtension   = false;


    // initialize report dir and start it
    if( writeReport )
    {
        assert( m_reportDir == L"" );
        assert( m_reportCSV.size() == 0 );

        m_reportDir = vaCore::GetExecutableDirectory();

        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);

        std::wstringstream ss;
    #pragma warning ( suppress : 4996 )
        ss << std::put_time(std::localtime(&in_time_t), L"%Y%m%d_%H%M%S");
        m_reportDir += L"AutoBench\\" + ss.str() + L"\\";

        m_reportName = ss.str();

        vaFileTools::DeleteDirectory( m_reportDir );
        vaFileTools::EnsureDirectoryExists( m_reportDir );

        // add system info
        string info = "System info:  " + vaCore::GetCPUIDName() + ", " + m_parent.GetRenderDevice().GetAdapterNameShort( );
        info += "\r\nAPI:  " + m_parent.GetRenderDevice().GetAPIName();
        if( (m_parent.GetRenderDevice().GetAPIName() == "DirectX12") )
            info += " (not yet fully optimized implementation)";
        ReportAddText( info + "\r\r\n\n" );

        ReportAddText( vaStringTools::Format("Resolution:   %d x %d\r\n", m_parent.GetApplication().GetWindowClientAreaSize().x, m_parent.GetApplication().GetWindowClientAreaSize().y ) );
        ReportAddText( vaStringTools::Format("Vsync:        ") + ((m_parent.GetApplication().GetSettings().Vsync)?("!!ON!!"):("OFF")) + "\r\n" );

        string fullscreenState;
        switch( m_parent.GetApplication().GetFullscreenState() )
        {
        case ( vaFullscreenState::Windowed ):               fullscreenState = "Windowed"; break;
        case ( vaFullscreenState::Fullscreen ):             fullscreenState = "Fullscreen"; break;
        case ( vaFullscreenState::FullscreenBorderless ):   fullscreenState = "Fullscreen Borderless"; break;
        case ( vaFullscreenState::Unknown ) : 
        default : fullscreenState = "Unknown";
            break;
        }

        ReportAddText( "Fullscreen:   " + fullscreenState + "\r\n" );
        ReportAddText( "\r\n" );
    }

    // determinism stuff
    m_parent.SetRequireDeterminism( ensureVisualDeterminism );
    m_parent.Camera()->Settings().GeneralSettings.DefaultAvgLuminanceWhenDataNotAvailable = 0.00251505827f;
    m_parent.Camera()->Settings().GeneralSettings.AutoExposureAdaptationSpeed = std::numeric_limits<float>::infinity();
}

AutoBenchTool::~AutoBenchTool( )
{
    m_parent.Settings()                                 = m_backupSettings;
    m_parent.DoFSettings()                              = m_backupDoFSettings;
    m_parent.Camera()->Load( m_backupCameraStorage );
    m_parent.Camera()->Settings()                       = m_backupCameraSettings;
    m_parent.GetFlythroughCameraController( )->SetPlayTime( m_backupFlythroughCameraTime );
    m_parent.GetFlythroughCameraController( )->SetPlaySpeed( m_backupFlythroughCameraSpeed );
    m_parent.SetFlythroughCameraEnabled( m_backupFlythroughCameraEnabled );
    m_parent.SetRequireDeterminism( false );

    vaTracer::SetTracerViewingUIEnabled( true );

    m_scriptInterface.SetUICallback( nullptr );

    // report finish
    if( m_reportDir != L"" )
    {
        // {
        //     vaFileStream outFile;
        //     outFile.Open( m_reportDir + m_reportName + L"_info.txt", (false)?(FileCreationMode::Append):(FileCreationMode::Create) );
        //     outFile.WriteTXT( m_reportTXT );
        // }

        FlushRowValues();

        if( !m_shouldStop )
        {
            vaFileStream outFile;
            outFile.Open( m_reportDir + m_reportName + L"_results.csv", (false)?(FileCreationMode::Append):(FileCreationMode::Create) );
            outFile.WriteTXT( m_reportTXT );
            outFile.WriteTXT( "\r\n" );
            VA_LOG( L"Report written to '%s'", m_reportDir.c_str() );
        }
        else
        {
            VA_WARN( L"Script stopped, no report written out!" );
        }
    }
}

void    AutoBenchTool::FlushRowValues( )
{
    for( int i = 0; i < m_reportCSV.size( ); i++ )
    {
        vector<string> row = m_reportCSV[i];
        string rowText;
        for( int j = 0; j < row.size( ); j++ )
        {
            rowText += row[j] + ", ";
        }
        m_reportTXT += rowText + "\r\n";
    }
    m_reportCSV.clear();
}

