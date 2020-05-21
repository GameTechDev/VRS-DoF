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

#include "vaRendering.h"

#include "vaRenderingIncludes.h"

#include "Rendering/vaRenderMesh.h"
#include "Rendering/vaRenderMaterial.h"

#include "vaTexture.h"

#include "vaAssetPack.h"

#include "vaRenderMesh.h"

#include "Rendering/vaTextureHelpers.h"

#include "Core/System/vaFileTools.h"


using namespace Vanilla;

void vaAssetResource::RegisterUsedAssetPacks( std::function<void( const vaAssetPack & )> registerFunction ) 
{ 
    assert( m_parentAsset != nullptr ); 
    if( m_parentAsset != nullptr ) registerFunction( m_parentAsset->GetAssetPack( ) ); 
};

void vaRenderingModuleRegistrar::RegisterModule( const std::string & deviceTypeName, const std::string & name, std::function< vaRenderingModule * ( const vaRenderingModuleParams & )> moduleCreateFunction )
{
    std::unique_lock<std::recursive_mutex> lockRegistrarMutex( GetInstance().m_mutex );

    assert( name != "" );
    string cleanedUpName = deviceTypeName + "<=>" + name;

    auto it = GetInstance().m_modules.find( cleanedUpName );
    if( it != GetInstance().m_modules.end( ) )
    {
        VA_ERROR( L"vaRenderingCore::RegisterModule - cleanedUpName '%s' already registered!", cleanedUpName.c_str() );
        return;
    }
    GetInstance().m_modules.insert( std::pair< std::string, ModuleInfo >( cleanedUpName, ModuleInfo( moduleCreateFunction ) ) );
}

vaRenderingModule * vaRenderingModuleRegistrar::CreateModule( const std::string & deviceTypeName, const std::string & name, const vaRenderingModuleParams & params )
{
    std::unique_lock<std::recursive_mutex> lockRegistrarMutex( GetInstance().m_mutex );

    string cleanedUpName = deviceTypeName + "<=>" + name;
    auto it = GetInstance().m_modules.find( cleanedUpName );
    if( it == GetInstance().m_modules.end( ) )
    {
        //it = GetInstance().m_modules.find( cleanedUpName );
        //if( it == GetInstance().m_modules.end( ) )
        {
            wstring wname = vaStringTools::SimpleWiden( cleanedUpName );
            VA_ERROR( L"vaRenderingCore::CreateModule - name '%s' not registered.", wname.c_str( ) );
            return nullptr;
        }
    }

    vaRenderingModule * ret = ( *it ).second.ModuleCreateFunction( params );

    ret->InternalRenderingModuleSetTypeName( cleanedUpName );

    return ret;
}

void vaRenderSelection::Reset( )
{
    if( MeshList != nullptr )
        MeshList->Reset( );
}

//void vaRenderingTools::IHO_Draw( ) 
//{ 
//}

// depends on the shadowmap type
vaRenderSelection::FilterSettings vaRenderSelection::FilterSettings::ShadowmapCull( const vaShadowmap & shadowmap )
{
    FilterSettings ret;
    shadowmap.SetToRenderSelectionFilter( ret );
    return ret;
}

vaRenderSelection::FilterSettings vaRenderSelection::FilterSettings::EnvironmentProbeCull( const vaIBLProbeData & probeData )
{
    FilterSettings ret;

    probeData;
    // make a frustum cube based on
    // probeData.Position
    // probeData.ClipFar

    return ret;
}

