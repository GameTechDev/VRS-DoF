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

#include "vaAssetImporter.h"

#include "Core/System/vaMemoryStream.h"
#include "Core/System/vaFileTools.h"

#include "Rendering/vaRenderMesh.h"
#include "Rendering/vaRenderMaterial.h"

#include "IntegratedExternals/vaImguiIntegration.h"

using namespace Vanilla;

bool LoadFileContents_Assimp( const wstring & path, vaAssetImporter::ImporterContext & parameters );

vaAssetImporter::vaAssetImporter( vaRenderDevice & device ) : 
    vaUIPanel( "Asset Importer", 0,
#ifdef VA_ASSIMP_INTEGRATION_ENABLED
        true
#else
        false
#endif
        , vaUIPanel::DockLocation::DockedRight
        ),
    m_device( device )
{
}
vaAssetImporter::~vaAssetImporter( )
{
    Clear();
}


bool vaAssetImporter::LoadFileContents( const wstring & path, ImporterContext & importerContext )
{
    wstring filename;
    wstring ext;
    wstring textureSearchPath;
    vaFileTools::SplitPath( path.c_str( ), &textureSearchPath, &filename, &ext );
    ext = vaStringTools::ToLower( ext );
    if( ext == L".sdkmesh" )
    {
        // this path doesn't work anymore, sorry :(
        assert( false );

        // std::shared_ptr<vaMemoryStream> fileContents;
        // wstring usedPath;
        // 
        // // try asset paths
        // if( fileContents == nullptr )
        // {
        //     usedPath = path;
        //     fileContents = vaFileTools::LoadFileToMemoryStream( usedPath.c_str( ) );
        // }
        // 
        // // found? try load and return!
        // if( fileContents == nullptr )
        // {
        //     vaFileTools::EmbeddedFileData embeddedFile = vaFileTools::EmbeddedFilesFind( ( L"textures:\\" + path ).c_str( ) );
        //     if( embeddedFile.HasContents( ) )
        //         fileContents = embeddedFile.MemStream;
        //     //
        // }
        // return LoadFileContents_SDKMESH( fileContents, textureSearchPath, filename, parameters, outContent );
    }
    else
    {
        return LoadFileContents_Assimp( path, importerContext );
    }

    //        VA_WARN( L"vaAssetImporter::LoadFileContents - don't know how to parse '%s' file type!", ext.c_str( ) );
    return false;
}

vaAssetImporter::ImporterContext::~ImporterContext( )
{
    if( AssetPack != nullptr )
        Device.GetAssetPackManager().UnloadPack( AssetPack );
}

void vaAssetImporter::Clear( )
{
    if( m_importerTask != nullptr )
    {
        if( !vaBackgroundTaskManager::GetInstance( ).IsFinished( m_importerTask ) )
        {
            m_importerContext->Abort();
            vaBackgroundTaskManager::GetInstance( ).WaitUntilFinished( m_importerTask );
        }
        m_importerTask = nullptr;
    }
    m_importerContext = nullptr;
    m_readyToImport = true;
}

void vaAssetImporter::UIPanelTickAlways( vaApplicationBase & )
{
    // these shouldn't ever appear anywhere unless we draw them
    if( GetAssetPack( ) != nullptr )
        GetAssetPack( )->UIPanelSetVisible( false );
    if( GetScene( ) != nullptr )
        GetScene( )->UIPanelSetVisible( false );
}

void vaAssetImporter::UIPanelTick( vaApplicationBase & application )
{
    application;
#ifdef VA_IMGUI_INTEGRATION_ENABLED
#ifdef VA_ASSIMP_INTEGRATION_ENABLED

    // importing assets UI
    if( m_readyToImport )
    {
        assert( m_importerContext == nullptr );

        ImGui::Text( "Importer options" );
        ImGui::Indent( );
        ImGui::Text( "Base transformation (applied to everything):" );

        ImGui::InputFloat3( "Base rotate yaw pitch roll", &m_settings.BaseRotateYawPitchRoll.x );
        vaVector3::Clamp( m_settings.BaseRotateYawPitchRoll, vaVector3( -180.0f, -180.0f, -180.0f ), vaVector3( 180.0f, 180.0f, 180.0f ) );
        if( ImGui::IsItemHovered() ) 
            ImGui::SetTooltip( "Yaw around the +Z (up) axis, a pitch around the +Y (right) axis, and a roll around the +X (forward) axis." );

        ImGui::InputFloat3( "Base scale", &m_settings.BaseTransformScaling.x );
        ImGui::InputFloat3( "Base offset", &m_settings.BaseTransformOffset.x );

        //ImGui::InputFloat3( "Base scaling", &m_uiContext.ImportingPopupBaseScaling.x );
        //ImGui::InputFloat3( "Base translation", &m_uiContext.ImportingPopupBaseTranslation.x );
        ImGui::Separator( );
        ImGui::Checkbox( "Assimp: force generate normals", &m_settings.AIForceGenerateNormals );
        // ImGui::Checkbox( "Assimp: generate normals (if missing)", &m_settings.AIGenerateNormalsIfNeeded );
        // ImGui::Checkbox( "Assimp: generate smooth normals (if generating)", &m_settings.AIGenerateSmoothNormalsIfGenerating );
        ImGui::Checkbox( "Assimp: SplitLargeMeshes", &m_settings.AISplitLargeMeshes );
        ImGui::Checkbox( "Assimp: FindInstances", &m_settings.AIFindInstances );
        ImGui::Checkbox( "Assimp: OptimizeMeshes", &m_settings.AIOptimizeMeshes );
        ImGui::Checkbox( "Assimp: OptimizeGraph", &m_settings.AIOptimizeGraph );
        ImGui::Separator( );
        ImGui::Checkbox( "Textures: GenerateMIPs", &m_settings.TextureGenerateMIPs );
        ImGui::Separator( );
        ImGui::InputText( "AssetNamePrefix", &m_settings.AssetNamePrefix );
        //ImGui::Checkbox( "Regenerate tangents/bitangents",      &m_uiContext.ImportingRegenerateTangents );
        ImGui::Unindent( );
        ImGui::Separator( );
        ImGui::InputText( "Input file", &m_inputFile, ImGuiInputTextFlags_AutoSelectAll );
        ImGui::SameLine( );
        if( ImGui::Button( "..." ) )
        {
            wstring fileName = vaFileTools::OpenFileDialog( vaStringTools::SimpleWiden( m_inputFile ), vaCore::GetExecutableDirectory( ) );
            if( fileName != L"" )
                m_inputFile = vaStringTools::SimpleNarrow( fileName );
        }
        ImGui::Separator( );

        if( vaFileTools::FileExists( m_inputFile ) )
        {
            if( ImGui::Button( "RUN IMPORTER", ImVec2( -1.0f, 0.0f ) ) )
            {
                assert( m_importerTask == nullptr );
                m_readyToImport = false;

                string fileName;
                vaFileTools::SplitPath( m_inputFile, nullptr, &fileName, nullptr );

                shared_ptr<vaAssetPack>     assetPack   = m_device.GetAssetPackManager( ).CreatePack( fileName + "_AssetPack" );

                if( assetPack == nullptr )
                {
                    Clear();
                    return;
                }

                shared_ptr<vaScene>         scene       = std::make_shared<vaScene>( fileName + "_Scene" );

                scene->DistantIBL().SetImportFilePath( "noon_grass_1k.hdr" );

                vaMatrix4x4 baseTransform
                    = vaMatrix4x4::Scaling( m_settings.BaseTransformScaling ) 
                    * vaMatrix4x4::FromYawPitchRoll( vaMath::DegreeToRadian( m_settings.BaseRotateYawPitchRoll.x ), vaMath::DegreeToRadian( m_settings.BaseRotateYawPitchRoll.y ), vaMath::DegreeToRadian( m_settings.BaseRotateYawPitchRoll.z ) ) 
                    * vaMatrix4x4::Translation( m_settings.BaseTransformOffset );

                m_importerContext = std::make_shared<ImporterContext>( m_device, m_inputFile, assetPack, scene, m_settings, baseTransform );

                auto importerLambda = [ importerContext = m_importerContext ] ( vaBackgroundTaskManager::TaskContext & ) -> bool 
                {
                    vaAssetImporter::LoadFileContents( vaStringTools::SimpleWiden(importerContext->FileName), *importerContext );
                    return true;
                };
                m_importerTask = vaBackgroundTaskManager::GetInstance().Spawn( "", vaBackgroundTaskManager::SpawnFlags::None, importerLambda );
            }
        }
        else
            ImGui::Text( "Select input file!" );
    }
    if( !m_readyToImport && m_importerContext != nullptr )
    {
        if( m_importerTask != nullptr && !vaBackgroundTaskManager::GetInstance().IsFinished( m_importerTask ) )
        {
            ImGui::ProgressBar( m_importerContext->GetProgress() );
            if( ImGui::Button( "Abort!", ImVec2( -1.0f, 0.0f ) ) )
            {
                m_importerContext->AddLog( "Aborting...\n" );
                m_importerContext->Abort( );
            }
        }
        else
        {
            ImGui::Text( "Import finished, log:" );
        }

        string logText = m_importerContext->GetLog();
        // how to scroll to bottom? no idea, see if these were resolved:
        // https://github.com/ocornut/imgui/issues/2072
        // https://github.com/ocornut/imgui/issues/1972
        // ImGui::InputTextMultiline( "Importer log", &logText, ImVec2(-1.0f, ImGui::GetTextLineHeight() * 8), ImGuiInputTextFlags_ReadOnly );
        ImGui::BeginChild( "Child1", ImVec2( -1, ImGui::GetTextLineHeight() * 8 ), true, ImGuiWindowFlags_HorizontalScrollbar );
        ImGui::Text( logText.c_str() );
        ImGui::SetScrollHereY( 1.0f );
        ImGui::EndChild();

        ImGui::Separator();

        if( vaBackgroundTaskManager::GetInstance().IsFinished( m_importerTask ) )
        {
            // if( ImGui::BeginChild( "ImporterDataStuff", ImVec2( 0.0f, 0.0f ), true ) )
            // {
            if( ImGui::Button( "Clear all imported data", ImVec2( -1.0f, 0.0f ) ) )
            {
                Clear( );
            }
            ImGui::Separator( );
            ImGui::Text( "Imported data:" );
            ImGui::Separator();
            if( GetAssetPack() == nullptr )
            {
                ImGui::Text( "Assets will appear here after importing" );
            }
            else
            {
                GetAssetPack()->UIPanelTickCollapsable( application, false, true );
            }
            ImGui::Separator();
            if( GetScene() == nullptr )
            {
                ImGui::Text( "Scene will appear here after importing" );
            }
            else
            {
                // if no lights, just add some defaults
                if( GetScene( )->Lights( ).size( ) == 0 )
                {
#if 0
                    //GetScene( )->Lights( ).push_back( std::make_shared<vaLight>( vaLight::MakeAmbient( "DefaultAmbient", vaVector3( 0.3f, 0.3f, 1.0f ), 0.1f ) ) );
                    //GetScene( )->Lights( ).push_back( std::make_shared<vaLight>( vaLight::MakeDirectional( "DefaultDirectional", vaVector3( 1.0f, 1.0f, 0.9f ), 1.0f, vaVector3( 0.0f, -1.0f, -1.0f ).Normalized() ) ) );
#else
                    // for bistro
                    float intensity = 100.0f;
                    GetScene( )->Lights( ).push_back( std::make_shared<vaLight>( vaLight::MakeSpot( "DefaultSpot0", 0.26f, vaVector3( 1.0f, 1.0f, 0.9f ), intensity, vaVector3( 4.995f, -0.51f, 3.11f ), vaVector3( 0.0f, 0.0f, -1.0f ).Normalized(), 1.0f, 1.6f ) ) );
                    GetScene( )->Lights( ).back( )->CastShadows = true;
                    GetScene( )->Lights( ).push_back( std::make_shared<vaLight>( vaLight::MakeSpot( "DefaultSpot1", 0.26f, vaVector3( 1.0f, 1.0f, 0.9f ), intensity, vaVector3( 7.99f,0.995f,3.11f ), vaVector3( 0.0f, 0.0f, -1.0f ).Normalized( ), 1.0f, 1.6f ) ) );
                    GetScene( )->Lights( ).back( )->CastShadows = true;
                    GetScene( )->Lights( ).push_back( std::make_shared<vaLight>( vaLight::MakeSpot( "DefaultSpot2", 0.26f, vaVector3( 1.0f, 1.0f, 0.9f ), intensity, vaVector3( 4.24f,-4.93f,3.11f ), vaVector3( 0.0f, 0.0f, -1.0f ).Normalized( ), 1.0f, 1.6f ) ) );
                    GetScene( )->Lights( ).back( )->CastShadows = true;
                    GetScene( )->Lights( ).push_back( std::make_shared<vaLight>( vaLight::MakeSpot( "DefaultSpot3", 0.26f, vaVector3( 1.0f, 1.0f, 0.9f ), intensity, vaVector3( 6.11f,-8.495f,2.97f ), vaVector3( 0.0f, 0.0f, -1.0f ).Normalized( ), 1.0f, 1.6f ) ) );
                    GetScene( )->Lights( ).back( )->CastShadows = true;
#endif
                }

                GetScene()->UIPanelTickCollapsable( application, false, true );
            }
        }
        //}
        // ImGui::End( );
    }
#else
    ImGui::Text("VA_ASSIMP_INTEGRATION_ENABLED not defined!");
#endif
#endif // #ifdef VA_IMGUI_INTEGRATION_ENABLED
}
