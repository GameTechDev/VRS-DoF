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

#include "vaRenderMesh.h"

#include "Rendering/vaRenderDeviceContext.h"

#include "Rendering/vaRenderMaterial.h"

#include "Rendering/vaStandardShapes.h"

#include "Core/System/vaFileTools.h"

#include "Core/vaXMLSerialization.h"

#include "IntegratedExternals/vaImguiIntegration.h"

#include "Rendering/vaAssetPack.h"

using namespace Vanilla;

//vaRenderMeshManager & renderMeshManager, const vaGUID & uid

const int c_renderMeshFileVersion = 3;


vaRenderMesh::vaRenderMesh( vaRenderMeshManager & renderMeshManager, const vaGUID & uid ) : vaAssetResource(uid), m_trackee(renderMeshManager.GetRenderMeshTracker(), this), m_renderMeshManager( renderMeshManager )
{
    m_frontFaceWinding          = vaWindingOrder::CounterClockwise;
    m_boundingBox               = vaBoundingBox::Degenerate;
    //m_tangentBitangentValid     = true;
}

vaRenderMesh::SubPart::SubPart( int indexStart, int indexCount, const weak_ptr<vaRenderMaterial>& material )
    : IndexStart( indexStart ), IndexCount( indexCount ), CachedMaterialRef( material )
{
    auto matL = material.lock( );
    if( matL != nullptr )
        MaterialID = matL->UIDObject_GetUID( );
}

shared_ptr<vaRenderMaterial> vaRenderMesh::GetMaterial( ) const
{
    if( m_part.MaterialID == vaGUID::Null )
        return m_renderMeshManager.GetRenderDevice().GetMaterialManager().GetDefaultMaterial( );
    else
        return vaUIDObjectRegistrar::GetInstance( ).FindCached( m_part.MaterialID, m_part.CachedMaterialRef ); 
}

void vaRenderMesh::SetMaterial( const shared_ptr<vaRenderMaterial>& m )
{
    if( m == nullptr )
    {
        m_part.MaterialID = vaGUID::Null;
        m_part.CachedMaterialRef.reset();
        return;
    }
    assert( m->UIDObject_GetUID( ) != vaCore::GUIDNull( ) );
    m_part.MaterialID = m->UIDObject_GetUID( );
    m_part.CachedMaterialRef = m;
}

void vaRenderMesh::SetTriangleMesh( const shared_ptr<StandardTriangleMesh> & mesh )
{
    m_triangleMesh = mesh;
    UpdateAABB( );
}

void vaRenderMesh::CreateTriangleMesh( vaRenderDevice & device, const vector<StandardVertex> & vertices, const vector<uint32> & indices )
{
    shared_ptr<StandardTriangleMesh> mesh = std::make_shared< vaRenderMesh::StandardTriangleMesh> ( device );

    mesh->Vertices()    = vertices;
    mesh->Indices()     = indices;

    SetTriangleMesh( mesh );
}

void vaRenderMesh::UpdateAABB( ) 
{ 
    if( m_triangleMesh != nullptr ) 
        m_boundingBox = vaTriangleMeshTools::CalculateBounds( m_triangleMesh->Vertices() );
    else 
        m_boundingBox = vaBoundingBox::Degenerate; 
}

void vaRenderMesh::RebuildNormals( )
{
    m_triangleMesh->GenerateNormals( m_frontFaceWinding );
    m_triangleMesh->SetDataDirty( );
}

vaRenderMeshManager::vaRenderMeshManager( const vaRenderingModuleParams & params ) : 
    vaRenderingModule( params ), 
    vaUIPanel( "RenderMeshManager", 0, false, vaUIPanel::DockLocation::DockedLeftBottom ),
    m_constantsBuffer( params )
{
    m_isDestructing = false;
    m_renderMeshes.SetAddedCallback( std::bind( &vaRenderMeshManager::RenderMeshesTrackeeAddedCallback, this, std::placeholders::_1 ) );
    m_renderMeshes.SetBeforeRemovedCallback( std::bind( &vaRenderMeshManager::RenderMeshesTrackeeBeforeRemovedCallback, this, std::placeholders::_1, std::placeholders::_2 ) );

    //m_totalDrawListEntriesQueued = 0;
}

vaRenderMeshManager::~vaRenderMeshManager( )
{
    m_isDestructing = true;
    //m_renderMeshesMap.clear();

    // this must absolutely be true as they contain direct reference to this object
    assert( m_renderMeshes.size( ) == 0 );
}

shared_ptr<vaRenderMesh> vaRenderMeshManager::CreateRenderMesh( const vaGUID & uid, bool startTrackingUIDObject ) 
{ 
    shared_ptr<vaRenderMesh> ret = shared_ptr<vaRenderMesh>( new vaRenderMesh( *this, uid ) ); 

    if( startTrackingUIDObject )
    {
        assert( vaThreading::IsMainThread( ) ); // warning, potential bug - don't automatically start tracking if adding from another thread; rather finish initialization completely and then manually call UIDObject_Track
        ret->UIDObject_Track( ); // needs to be registered to be visible/searchable by various systems such as rendering
    }

    return ret;
}

void vaRenderMeshManager::RenderMeshesTrackeeAddedCallback( int newTrackeeIndex )
{
    newTrackeeIndex; // unreferenced
//    m_renderMeshesDrawLists.push_back( std::vector< MeshDrawListEntry >() );
//    assert( (int)m_renderMeshesDrawLists.size()-1 == newTrackeeIndex );
}

void vaRenderMeshManager::RenderMeshesTrackeeBeforeRemovedCallback( int toBeRemovedTrackeeIndex, int toBeReplacedByTrackeeIndex )
{
    toBeRemovedTrackeeIndex; // unreferenced
    toBeReplacedByTrackeeIndex; // unreferenced
//    assert( m_renderMeshesDrawLists.size( ) == m_renderMeshes.size( ) );
//    if( toBeReplacedByTrackeeIndex != -1 )
//        m_renderMeshesDrawLists[ toBeRemovedTrackeeIndex ] = m_renderMeshesDrawLists[ toBeReplacedByTrackeeIndex ];
//    m_renderMeshesDrawLists.pop_back();

    // // if ever need be, to make this faster track whether the mesh is in the library and if it is only then do the search below 
    // if( !m_isDestructing )
    // {
    //     const wstring & name = m_renderMeshes[toBeRemovedTrackeeIndex]->GetName( );
    //     const auto iterator = m_renderMeshesMap.find( name );
    //     if( iterator != m_renderMeshesMap.end( ) )
    //     {
    //         m_renderMeshesMap.erase( iterator );
    //     }
    // }
}

// void vaRenderMeshManager::ResetDrawEntries( )
// {
//     for( size_t i = 0; i < m_renderMeshesDrawLists.size(); i++ )
//         m_renderMeshesDrawLists[i].clear( );
//     m_totalDrawListEntriesQueued = 0;
// }

void vaRenderMeshManager::UIPanelTick( vaApplicationBase & )
{
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    static int selected = 0;
    ImGui::BeginChild( "left pane", ImVec2( 150, 0 ), true );
    for( int i = 0; i < 7; i++ )
    {
        char label[128];
        sprintf_s( label, _countof( label ), "MyObject %d", i );
        if( ImGui::Selectable( label, selected == i ) )
            selected = i;
    }
    ImGui::EndChild( );
    ImGui::SameLine( );

    // right
    ImGui::BeginGroup( );
    ImGui::BeginChild( "item view", ImVec2( 0, -ImGui::GetFrameHeightWithSpacing( ) ) ); // Leave room for 1 line below us
    ImGui::Text( "MyObject: %d", selected );
    ImGui::Separator( );
    ImGui::TextWrapped( "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. " );
    ImGui::EndChild( );
    ImGui::BeginChild( "buttons" );
    if( ImGui::Button( "Revert" ) ) { }
    ImGui::SameLine( );
    if( ImGui::Button( "Save" ) ) { }
    ImGui::EndChild( );
    ImGui::EndGroup( );
#endif
}

bool vaRenderMesh::SaveAPACK( vaStream & outStream )
{
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int32>( c_renderMeshFileVersion ) );

    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int32>( (int32)m_frontFaceWinding ) );
    //VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<bool>( m_tangentBitangentValid ) );

    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValueVector<uint32>( m_triangleMesh->Indices() ) );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValueVector<StandardVertex>( m_triangleMesh->Vertices() ) );

    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int32>( 1 ) ); //(int32)m_parts.size( ) ) );

    {
        VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int32>( m_part.IndexStart ) );
        VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int32>( m_part.IndexCount ) );

        shared_ptr<vaRenderMaterial> material = vaUIDObjectRegistrar::GetInstance().FindCached( m_part.MaterialID, m_part.CachedMaterialRef );
        VERIFY_TRUE_RETURN_ON_FALSE( SaveUIDObjectUID( outStream, material ) );
    }

    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<vaBoundingBox>( m_boundingBox ) );

    return true;
}

bool vaRenderMesh::LoadAPACK( vaStream & inStream )
{
    int32 fileVersion = 0;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( fileVersion ) );
    if( !( (fileVersion >= 3 ) && (fileVersion <= c_renderMeshFileVersion) ) )
    {
        VA_LOG( L"vaRenderMesh::Load(): unsupported file version" );
        return false;
    }

    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( reinterpret_cast<int32&>(m_frontFaceWinding) ) );

    shared_ptr<StandardTriangleMesh> triMesh = std::make_shared< vaRenderMesh::StandardTriangleMesh> ( m_renderMeshManager.GetRenderDevice() );

    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValueVector<uint32>( triMesh->Indices() ) );
    
    // std::vector<StandardVertexOld> VerticesOld;
    // VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValueVector<StandardVertexOld>( VerticesOld ) );
    // triMesh->Vertices.resize( VerticesOld.size() );
    // for( int i = 0; i < VerticesOld.size(); i++ )
    //     triMesh->Vertices[i] = VerticesOld[i];

    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValueVector<StandardVertex>( triMesh->Vertices() ) );

    SetTriangleMesh( triMesh );


    int32 partCount = 0;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( partCount ) );

    // multipart no longer supported
    assert( partCount <= 1 );

    if( partCount > 0 )
    {
        VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( m_part.IndexStart ) );
        VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( m_part.IndexCount ) );
        VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaGUID>( m_part.MaterialID ) );
    }
    else
    {
        m_part = SubPart( );
    }

    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaBoundingBox>( m_boundingBox ) );

    return true;
}

bool vaRenderMesh::SerializeUnpacked( vaXMLSerializer & serializer, const wstring & assetFolder )
{
    assetFolder;
    int32 fileVersion = c_renderMeshFileVersion;
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "FileVersion", fileVersion ) );
    VERIFY_TRUE_RETURN_ON_FALSE( fileVersion == c_renderMeshFileVersion );


    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "FrontFaceWinding", reinterpret_cast<int32&>(m_frontFaceWinding) ) );
    //VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<bool>( "TangentBitangentValid", m_tangentBitangentValid ) );

    if( serializer.IsReading() )
    {
        shared_ptr<StandardTriangleMesh> triMesh = std::make_shared< vaRenderMesh::StandardTriangleMesh> ( m_renderMeshManager.GetRenderDevice() );
        SetTriangleMesh( triMesh );
        triMesh->SetDataDirty();
    }

    int32 indexCount = (int32)m_triangleMesh->Indices().size();
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "IndexCount", indexCount ) );
    if( serializer.IsReading() )
    {
        m_triangleMesh->Indices().resize( indexCount );
        vaFileTools::ReadBuffer( assetFolder + L"/Indices.bin", &m_triangleMesh->Indices()[0], sizeof(uint32) * m_triangleMesh->Indices().size() );
    }
    else if( serializer.IsWriting( ) )
    {
        vaFileTools::WriteBuffer( assetFolder + L"/Indices.bin", &m_triangleMesh->Indices()[0], sizeof(uint32) * m_triangleMesh->Indices().size() );
    }
    else { assert( false ); return false; }

    int32 vertexCount = (int32)m_triangleMesh->Vertices().size();
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "VertexCount", vertexCount ) );
    if( serializer.IsReading() )
    {
        m_triangleMesh->Vertices().resize( vertexCount );
        vaFileTools::ReadBuffer( assetFolder + L"/Vertices.bin", &m_triangleMesh->Vertices()[0], sizeof(vaRenderMesh::StandardVertex) * m_triangleMesh->Vertices().size() );
    }
    else if( serializer.IsWriting( ) )
    {
        vaFileTools::WriteBuffer( assetFolder + L"/Vertices.bin", &m_triangleMesh->Vertices()[0], sizeof(vaRenderMesh::StandardVertex) * m_triangleMesh->Vertices().size() );
    }
    else { assert( false ); return false; }
        

    // int32 partCount = 1;
    // VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "PartCount", partCount ) );

    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaVector3>( "AABBMin", m_boundingBox.Min ) );
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaVector3>( "AABBSize", m_boundingBox.Size ) );

    //assert( partCount < 9999 );
    //if( serializer.IsReading() )
    //{
    //    // multipart no longer supported
    //    // assert( partCount <= 1 );
    //}
    //if( partCount > 0 )
    {
        // string partName = vaStringTools::Format( "Part%04d", partCount );
        //VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeOpenChildElement( partName.c_str() ) );
        VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "PartIndexStart", m_part.IndexStart ) );
        VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "PartIndexCount", m_part.IndexCount ) );
        VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<vaGUID>( "PartMaterialID", m_part.MaterialID ) );

        if( serializer.IsWriting() )
        {
            shared_ptr<vaRenderMaterial> material = vaUIDObjectRegistrar::GetInstance().FindCached( m_part.MaterialID, m_part.CachedMaterialRef );
            // serializer.WriteAttribute( "MaterialID", vaCore::GUIDToStringA( (material == nullptr)?(vaCore::GUIDNull()):(material->UIDObject_GetUID()) ) );
            assert( ( (material == nullptr)?(vaCore::GUIDNull()):(material->UIDObject_GetUID()) ) == m_part.MaterialID );
        } 

        //VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializePopToParentElement( partName.c_str() ) );
    }
    return true;
}

// void vaRenderMesh::ReconnectDependencies( )
// {
//     for( size_t i = 0; i < m_parts.size(); i++ )
//     {
//         std::shared_ptr<vaRenderMaterial> materialSharedPtr;
//         vaUIDObjectRegistrar::GetInstance( ).ReconnectDependency<vaRenderMaterial>( materialSharedPtr, m_parts[i].MaterialID );
//         m_parts[i].Material = materialSharedPtr;
//     }
// }

void vaRenderMesh::RegisterUsedAssetPacks( std::function<void( const vaAssetPack & )> registerFunction )
{
    vaAssetResource::RegisterUsedAssetPacks( registerFunction );
    //for( auto subPart : m_parts )
    {
        shared_ptr<vaRenderMaterial> material = vaUIDObjectRegistrar::GetInstance().FindCached( m_part.MaterialID, m_part.CachedMaterialRef );
        if( material != nullptr )
        {
            material->RegisterUsedAssetPacks( registerFunction );
        }
        else
        {
            // asset is missing?
            assert( m_part.MaterialID == vaCore::GUIDNull() );
        }
    }
}

void vaRenderMesh::EnumerateUsedAssets( const std::function<void(vaAsset * asset)> & callback )
{
    callback( GetParentAsset( ) );
    shared_ptr<vaRenderMaterial> material = vaUIDObjectRegistrar::GetInstance( ).FindCached( m_part.MaterialID, m_part.CachedMaterialRef );
    if( material != nullptr )
    {
        material->EnumerateUsedAssets( callback );
    }
    else
    {
        // asset is missing?
        assert( m_part.MaterialID == vaCore::GUIDNull( ) );
    }
}

// create mesh with provided triangle mesh, winding order and material
shared_ptr<vaRenderMesh> vaRenderMesh::Create( shared_ptr<StandardTriangleMesh> & triMesh, vaWindingOrder frontFaceWinding, const vaGUID & materialID, const vaGUID & uid, bool startTrackingUIDObject )
{
    shared_ptr<vaRenderMesh> mesh = triMesh->GetRenderDevice().GetMeshManager().CreateRenderMesh( uid, false );
    if( mesh == nullptr )
    {
        assert( false );
        return nullptr;
    }

    mesh->SetTriangleMesh( triMesh );
    mesh->SetFrontFaceWindingOrder( frontFaceWinding );
    mesh->SetPart( vaRenderMesh::SubPart( 0, (int)triMesh->Indices().size( ), materialID ) );

    if( startTrackingUIDObject )
    {
        assert( vaThreading::IsMainThread( ) ); // warning, potential bug - don't automatically start tracking if adding from another thread; rather finish initialization completely and then manually call UIDObject_Track
        mesh->UIDObject_Track( ); // needs to be registered to be visible/searchable by various systems such as rendering
    }

    return mesh;
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateShallowCopy( const vaRenderMesh & copy, const vaGUID& uid, bool startTrackingUIDObject )
{
    shared_ptr<vaRenderMesh> mesh = copy.GetManager().CreateRenderMesh( uid, false );
    if( mesh == nullptr )
    {
        assert( false );
        return nullptr;
    }

    mesh->SetTriangleMesh( copy.GetTriangleMesh() );
    mesh->SetFrontFaceWindingOrder( copy.GetFrontFaceWindingOrder() );
    mesh->SetPart( copy.GetPart() );

    if( startTrackingUIDObject )
    {
        assert( vaThreading::IsMainThread( ) ); // warning, potential bug - don't automatically start tracking if adding from another thread; rather finish initialization completely and then manually call UIDObject_Track
        mesh->UIDObject_Track( ); // needs to be registered to be visible/searchable by various systems such as rendering
    }

    return mesh;
}

shared_ptr<vaRenderMesh> vaRenderMesh::Create( vaRenderDevice & device, const vaMatrix4x4 & transform, const std::vector<vaVector3> & vertices, const std::vector<vaVector3> & normals, const std::vector<vaVector2> & texcoords0, const std::vector<vaVector2> & texcoords1, const std::vector<uint32> & indices, vaWindingOrder frontFaceWinding, const vaGUID & uid, bool startTrackingUIDObject )
{
    assert( ( vertices.size( ) == normals.size( ) ) && ( vertices.size( ) == texcoords0.size( ) ) && ( vertices.size( ) == texcoords1.size( ) ) );

    vector<vaRenderMesh::StandardVertex> newVertices( vertices.size( ) );

    for( int i = 0; i < (int)vertices.size( ); i++ )
    {
        vaRenderMesh::StandardVertex & svert = newVertices[i];
        svert.Position = vaVector3::TransformCoord( vertices[i], transform );
        svert.Color = 0xFFFFFFFF;
        svert.Normal = vaVector4( vaVector3::TransformNormal( normals[i], transform ), 0.0f );
        svert.TexCoord0 = texcoords0[i];
        svert.TexCoord1 = texcoords1[i];
    }

    shared_ptr<vaRenderMesh> mesh = device.GetMeshManager().CreateRenderMesh( uid, false );
    if( mesh == nullptr )
    {
        assert( false );
        return nullptr;
    }
    mesh->CreateTriangleMesh( device, newVertices, indices );
    mesh->SetPart( vaRenderMesh::SubPart( 0, (int)indices.size( ), weak_ptr<vaRenderMaterial>( ) ) );
    mesh->SetFrontFaceWindingOrder( frontFaceWinding );

    if( startTrackingUIDObject )
    {
        assert( vaThreading::IsMainThread( ) ); // warning, potential bug - don't automatically start tracking if adding from another thread; rather finish initialization completely and then manually call UIDObject_Track
        mesh->UIDObject_Track( ); // needs to be registered to be visible/searchable by various systems such as rendering
    }

    return mesh;
}

#define RMC_DEFINE_DATA                     \
    vector<vaVector3>  vertices;            \
    vector<vaVector3>  normals;             \
    vector<vaVector2>  texcoords0;          \
    vector<vaVector2>  texcoords1;          \
    vector<uint32>     indices; 

#define RMC_RESIZE_NTTT                     \
    normals.resize( vertices.size() );      \
    texcoords0.resize( vertices.size( ) );  \
    texcoords1.resize( vertices.size( ) );  \

shared_ptr<vaRenderMesh> vaRenderMesh::CreatePlane( vaRenderDevice & device, const vaMatrix4x4 & transform, float sizeX, float sizeY, bool doubleSided, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreatePlane( vertices, indices, sizeX, sizeY, doubleSided );
    vaWindingOrder windingOrder = vaWindingOrder::CounterClockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    for( size_t i = 0; i < vertices.size( ); i++ )
    {
        texcoords0[i] = vaVector2( vertices[i].x / sizeX / 2.0f + 0.5f, vertices[i].y / sizeY / 2.0f + 0.5f );
        texcoords1[i] = texcoords0[i];
    }

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, vaWindingOrder::CounterClockwise, uid );
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateGrid( vaRenderDevice & device, const vaMatrix4x4 & transform, int dimX, int dimY, float sizeX, float sizeY, const vaVector2 & uvOffsetMul, const vaVector2 & uvOffsetAdd, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateGrid( vertices, indices, dimX, dimY, sizeX, sizeY );
    vaWindingOrder windingOrder = vaWindingOrder::CounterClockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    for( size_t i = 0; i < vertices.size( ); i++ )
    {
        // texel-to-pixel mapping
        texcoords0[i] = vaVector2( vertices[i].x / sizeX + 0.5f, vertices[i].y / sizeY + 0.5f );
        // custom UV
        texcoords1[i] = vaVector2( vaVector2::ComponentMul( texcoords0[i], uvOffsetMul ) + uvOffsetAdd );
    }

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, vaWindingOrder::CounterClockwise, uid );
}

// dummy texture coords
void FillDummyTT( const vector<vaVector3> & vertices, const vector<vaVector3> & normals, vector<vaVector2> & texcoords0, vector<vaVector2> & texcoords1 )
{
    normals;
    for( size_t i = 0; i < vertices.size( ); i++ )
    {
        texcoords0[i] = vaVector2( vertices[i].x / 2.0f + 0.5f, vertices[i].y / 2.0f + 0.5f );
        texcoords1[i] = vaVector2( 0.0f, 0.0f );
    }
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateTetrahedron( vaRenderDevice & device, const vaMatrix4x4 & transform, bool shareVertices, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateTetrahedron( vertices, indices, shareVertices );
    vaWindingOrder windingOrder = vaWindingOrder::Clockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    FillDummyTT( vertices, normals, texcoords0, texcoords1 );

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, windingOrder, uid );
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateCube( vaRenderDevice & device, const vaMatrix4x4 & transform, bool shareVertices, float edgeHalfLength, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateCube( vertices, indices, shareVertices, edgeHalfLength );
    vaWindingOrder windingOrder = vaWindingOrder::Clockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    FillDummyTT( vertices, normals, texcoords0, texcoords1 );

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, windingOrder, uid );
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateOctahedron( vaRenderDevice & device, const vaMatrix4x4 & transform, bool shareVertices, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateOctahedron( vertices, indices, shareVertices );
    vaWindingOrder windingOrder = vaWindingOrder::Clockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    FillDummyTT( vertices, normals, texcoords0, texcoords1 );

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, windingOrder, uid );
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateIcosahedron( vaRenderDevice & device, const vaMatrix4x4 & transform, bool shareVertices, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateIcosahedron( vertices, indices, shareVertices );
    vaWindingOrder windingOrder = vaWindingOrder::Clockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    FillDummyTT( vertices, normals, texcoords0, texcoords1 );

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, windingOrder, uid );
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateDodecahedron( vaRenderDevice & device, const vaMatrix4x4 & transform, bool shareVertices, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateDodecahedron( vertices, indices, shareVertices );
    vaWindingOrder windingOrder = vaWindingOrder::Clockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    FillDummyTT( vertices, normals, texcoords0, texcoords1 );

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, windingOrder, uid );
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateSphere( vaRenderDevice & device, const vaMatrix4x4 & transform, int tessellationLevel, bool shareVertices, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateSphereUVWrapped( vertices, indices, texcoords0, tessellationLevel, shareVertices );
    vaWindingOrder windingOrder = vaWindingOrder::Clockwise;

    normals.resize( vertices.size() );  // RMC_RESIZE_NTTT;
    texcoords1.resize( vertices.size(), vaVector2( 0.0f, 0.0f ) );

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    if( shareVertices )
        vaTriangleMeshTools::MergeNormalsForEqualPositions( normals, vertices );

    // FillDummyTT( vertices, normals, texcoords0, texcoords1 );

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, windingOrder, uid );
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateCylinder( vaRenderDevice & device, const vaMatrix4x4 & transform, float height, float radiusBottom, float radiusTop, int tessellation, bool openTopBottom, bool shareVertices, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateCylinder( vertices, indices, height, radiusBottom, radiusTop, tessellation, openTopBottom, shareVertices );
    vaWindingOrder windingOrder = vaWindingOrder::Clockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    FillDummyTT( vertices, normals, texcoords0, texcoords1 );

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, windingOrder, uid );
}

shared_ptr<vaRenderMesh> vaRenderMesh::CreateTeapot( vaRenderDevice & device, const vaMatrix4x4 & transform, const vaGUID & uid )
{
    RMC_DEFINE_DATA;

    vaStandardShapes::CreateTeapot( vertices, indices );
    vaWindingOrder windingOrder = vaWindingOrder::Clockwise;

    RMC_RESIZE_NTTT;

    vaTriangleMeshTools::GenerateNormals( normals, vertices, indices, windingOrder );

    FillDummyTT( vertices, normals, texcoords0, texcoords1 );

    return Create( device, transform, vertices, normals, texcoords0, texcoords1, indices, windingOrder, uid );
}

vaRenderDevice & vaRenderMesh::GetRenderDevice( ) const
{
    return GetManager().GetRenderDevice();
}

bool vaRenderMesh::UIPropertiesDraw( vaApplicationBase & application )
{
    bool hadChanges = false;

    application;
    ImGui::Text( "Number of vertices: %d", (int)GetTriangleMesh( )->Vertices( ).size( ) );
    ImGui::Text( "Number of indices:  %d", (int)GetTriangleMesh( )->Indices( ).size( ) );
    ImGui::Text( "AA Bounding box:" );
    ImGui::Text( "  min{%.2f,%.2f,%.2f}, size{%.2f,%.2f,%.2f}", m_boundingBox.Min.x, m_boundingBox.Min.y, m_boundingBox.Min.z, m_boundingBox.Size.x, m_boundingBox.Size.y, m_boundingBox.Size.z );

    if( ImGui::Combo( "Front face winding order", (int*)&m_frontFaceWinding, "Undefined\0Clockwise\0CounterClockwise\0\0" ) )
    {
        hadChanges = true;
        m_frontFaceWinding = (vaWindingOrder)vaMath::Clamp( (int)m_frontFaceWinding, 1, 2 );
    }
    ImGui::Text( "(TODO: add 'force wireframe' here");

    if( ImGui::Button( "Rebuild normals" ) )
    {
        RebuildNormals( );
        hadChanges = true;
    }

    string materialName = (m_part.MaterialID == vaGUID::Null)?("None"):("Unknown");
    
    vaGUID materialID = m_part.MaterialID;
    auto material = GetMaterial();
    vaAsset* materialAsset = ( material != nullptr ) ? ( material->GetParentAsset( ) ) : ( nullptr );
    if( materialAsset != nullptr )
        materialName = materialAsset->Name();

    ImGui::Text( "Material: %s\n", materialName.c_str( ) );
    if( materialAsset != nullptr )
    {
        switch( ImGuiEx_SmallButtons( materialName.c_str( ), { "[unlink]", "[props]" } ) )
        {
        case( -1 ): break;
        case( 0 ):
        {
            SetMaterial( nullptr );
            hadChanges = true;
            materialAsset = nullptr;
        } break;
        case( 1 ):
        {
            vaUIManager::GetInstance( ).SelectPropertyItem( materialAsset->GetSharedPtr( ) );
        } break;
        default: assert( false ); break;
        }
    }
    else if( materialID != vaGUID::Null )
    {
        ImGui::Text( "ID present but asset not found" );
    }
    else
    {
        auto newAsset = std::dynamic_pointer_cast<vaRenderMaterial>( GetRenderDevice( ).GetAssetPackManager( ).UIAssetDragAndDropTarget( vaAssetType::RenderMaterial, "Drop material asset here to link", { -1, 0 } ) );
        if( newAsset != nullptr )
        {
            SetMaterial( newAsset );
            hadChanges = true;
        }
    }
    return hadChanges;
}

// version that takes the material off the mesh, if possible, and has defaults for everything else - super-simple
void vaRenderMeshDrawList::Insert( const std::shared_ptr<vaRenderMesh>& mesh, const vaMatrix4x4& transform, vaShadingRate shadingRate, const vaVector4& customColor )
{
    // resolve material here - both easier to manage and faster (when this gets parallelized)
    auto renderMaterial = mesh->GetMaterial( );
    if( renderMaterial == nullptr )
    {
        // drawResults |= vaDrawResultFlags::AssetsStillLoading;
        renderMaterial = mesh->GetManager( ).GetRenderDevice( ).GetMaterialManager( ).GetDefaultMaterial( );
    }
    Insert( mesh, renderMaterial, transform, shadingRate, customColor );
}

vaRenderMeshDrawList::Entry::Entry( const std::shared_ptr<vaRenderMesh> & mesh, const std::shared_ptr<vaRenderMaterial> & material, const vaMatrix4x4 & transform, vaShadingRate shadingRate, const vaVector4 & customColor ) 
    : Mesh( mesh ), Material( material ), Transform( transform ), ShadingRate( shadingRate ), CustomColor( customColor )
{
    assert( material != nullptr );
}

void vaRenderMeshDrawList::StartSort( const vaRenderSelection::SortSettings& sortSettings ) const
{
    if( sortSettings.SortByDistanceToPoint && sortSettings.ReferencePoint.x == std::numeric_limits<float>::infinity( ) )
    {
        assert( false ); // you haven't updated sortSettings.ReferencePoint
        m_sortState.Enabled = false;
        m_sortState.Sorted  = false;
        return;
    }

    m_sortState.Enabled         = sortSettings.SortByDistanceToPoint || sortSettings.SortByVRSType;
    m_sortState.Sorted          = !m_sortState.Enabled;
    m_sortState.SortSettings    = sortSettings;

    m_sortState.SortDistances.resize( m_drawList.size( ) );
    m_sortState.SortedIndices.resize( m_drawList.size( ) );
}

void vaRenderMeshDrawList::FinalizeSort( const vaRenderSelection::SortSettings & sortSettings ) const
{
    // all good, we're sorted
    if( m_sortState.Sorted )
        return;
    if( !m_sortState.Enabled )
        return;

    assert( m_sortState.SortSettings == sortSettings ); sortSettings;
    assert( m_sortState.SortDistances.size() == m_drawList.size( ) );
    assert( m_sortState.SortedIndices.size() == m_drawList.size( ) );

    for( int i = 0; i < m_drawList.size( ); i++ )
    {
        auto & item = m_drawList[i];

        assert( m_drawList[i].Material != nullptr );    // not allowed anymore

        int sortGroup = 0;

        // special decal case
        if( item.Material->GetMaterialSettings( ).LayerMode == vaLayerMode::Decal )
        {
            sortGroup = vaMath::Clamp( item.Material->GetMaterialSettings( ).DecalSortOrder, -65536, 65536 ) - 100000;
            assert( !m_sortState.SortSettings.SortByVRSType ); // these don't work together
        }
        else if( m_sortState.SortSettings.SortByVRSType )
            sortGroup = (int)m_drawList[i].ShadingRate;

        m_sortState.SortDistances[i] = { sortGroup, ( vaVector3::TransformCoord( m_drawList[i].Mesh->GetAABB( ).Center( ), m_drawList[i].Transform ) - m_sortState.SortSettings.ReferencePoint ).Length( ) };
        m_sortState.SortedIndices[i] = i;
    }

    std::sort( m_sortState.SortedIndices.begin( ), m_sortState.SortedIndices.end( ),
        [&sortedIndices = m_sortState.SortedIndices, &sortDistances = m_sortState.SortDistances, frontToBack = m_sortState.SortSettings.FrontToBack]( const int ia, const int ib ) -> bool
    {
        // first sort by special type
        if( sortDistances[ia].first != sortDistances[ib].first )
            return sortDistances[ia].first < sortDistances[ib].first;
        else // then by distance
            return (frontToBack)?(sortDistances[ia].second < sortDistances[ib].second):(sortDistances[ia].second > sortDistances[ib].second);
    } );
    m_sortState.Sorted = true;
}

vaDrawResultFlags vaRenderMeshManager::Draw( vaSceneDrawContext & drawContext, const vaRenderMeshDrawList & list, vaBlendMode blendMode, vaRenderMeshDrawFlags drawFlags, 
    const vaRenderSelection::SortSettings & sortSettings, std::function< void( const vaRenderMeshDrawList::Entry & entry, const vaRenderMaterial & material, vaGraphicsItem & renderItem ) > globalCustomizer )
{
    vaDrawResultFlags drawResults = vaDrawResultFlags::None;

    vaRenderMaterialShaderType shaderType;
    switch( drawContext.OutputType )
    {
    case( vaDrawContextOutputType::DepthOnly ):     shaderType = vaRenderMaterialShaderType::DepthOnly;     break;
    case( vaDrawContextOutputType::Forward   ):     shaderType = vaRenderMaterialShaderType::Forward;       break;
    //case( vaDrawContextOutputType::Deferred  ):     shaderType = vaRenderMaterialShaderType::Deferred;      break;
    case( vaDrawContextOutputType::CustomShadow ):  shaderType = vaRenderMaterialShaderType::CustomShadow;  break;
    default:
        assert( false );
        return vaDrawResultFlags::UnspecifiedError;
        break;
    }

    int ratechanges = 0;
    vaShadingRate lastrate = vaShadingRate::ShadingRate1X1;

    bool reverseOrder           = false; //(drawFlags & vaRenderMeshDrawFlags::ReverseDrawOrder )        != 0;  <- this messes with the decals 
    // bool skipTransparencies     = (drawFlags & vaRenderMeshDrawFlags::SkipTransparencies    )   != 0;
    // bool skipNonTransparencies  = (drawFlags & vaRenderMeshDrawFlags::SkipNonTransparencies )   != 0;
    bool skipNonShadowCasters   = (drawFlags & vaRenderMeshDrawFlags::SkipNonShadowCasters  )   != 0;
    //bool skipNoDepthPrePass     = (drawFlags & vaRenderMeshDrawFlags::SkipNoDepthPrePass )      != 0;
    
    bool enableDepthTest        = (drawFlags & vaRenderMeshDrawFlags::EnableDepthTest       )   != 0;
    bool invertDepthTest        = (drawFlags & vaRenderMeshDrawFlags::InvertDepthTest       )   != 0;
    bool enableDepthWrite       = (drawFlags & vaRenderMeshDrawFlags::EnableDepthWrite      )   != 0;
    bool depthTestIncludesEqual = (drawFlags & vaRenderMeshDrawFlags::DepthTestIncludesEqual)   != 0;
    bool depthTestEqualOnly     = (drawFlags & vaRenderMeshDrawFlags::DepthTestEqualOnly)   != 0;

    bool depthEnable            = enableDepthTest || enableDepthWrite;
    bool useReversedZ           = (invertDepthTest)?(!drawContext.Camera.GetUseReversedZ()):(drawContext.Camera.GetUseReversedZ());

    vaComparisonFunc depthFunc  = vaComparisonFunc::Always;
    if( enableDepthTest )
    {
        if( !depthTestEqualOnly )
            depthFunc               = ( depthTestIncludesEqual ) ? ( ( useReversedZ )?( vaComparisonFunc::GreaterEqual ):( vaComparisonFunc::LessEqual ) ):( ( useReversedZ )?( vaComparisonFunc::Greater ):( vaComparisonFunc::Less ) );
        else
            depthFunc               = vaComparisonFunc::Equal;
    }

    vaGraphicsItem commonRenderItem;

    // set up thingies common to all render items
    {
        // set our main constant buffer
        commonRenderItem.ConstantBuffers[ SHADERINSTANCE_CONSTANTSBUFFERSLOT ] = m_constantsBuffer;

        // these could be set outside of the loop but we'll keep them here in case of any overrides (commonRenderItemOverrides) messing up the states
        commonRenderItem.BlendMode        = blendMode;
        commonRenderItem.DepthFunc        = depthFunc;
        commonRenderItem.Topology         = vaPrimitiveTopology::TriangleList;
        commonRenderItem.DepthEnable      = depthEnable;
        commonRenderItem.DepthWriteEnable = enableDepthWrite;
    }

    list.StartSort( sortSettings );
    list.FinalizeSort( sortSettings );

    auto const & listSortState = list.SortState();
    if( listSortState.Enabled ) 
    {
        assert( listSortState.Sorted );
        assert( listSortState.SortDistances.size() == list.Count() && listSortState.SortedIndices.size() == list.Count() );
    }

    drawContext.RenderDeviceContext.BeginItems( vaRenderTypeFlags::Graphics, &drawContext );
    vaGraphicsItem renderItem;
    for( int i = 0; i < list.Count(); i++ )
    {
        int ii = i;
        if( list.m_sortState.Enabled )
            ii = listSortState.SortedIndices[(reverseOrder)?(list.Count()-1-i):(i)];
        const vaRenderMeshDrawList::Entry & entry = list[ii];

        if( entry.Mesh == nullptr ) 
        { VA_WARN( "vaRenderMeshManagerDX11::Draw - drawing empty mesh" ); continue; }

        const vaRenderMesh & mesh = *entry.Mesh.get();
        if( mesh.GetTriangleMesh( ) == nullptr )
            { assert( false ); continue; }

        // we can only render our own meshes
        assert( &mesh.GetManager() == this );

        if( mesh.GetTriangleMesh( )->Indices().size() == 0 )
            continue;   // should this assert? I guess empty mesh is valid?

        const vaRenderMesh::SubPart & subPart = mesh.GetPart(); // parts[subPartIndex];

        if( subPart.IndexCount == 0 )
            continue;

        shared_ptr<vaRenderMaterial> material = entry.Material;

        // at this point material must be valid
        assert( material != nullptr  );
            
        const vaRenderMaterial::MaterialSettings & materialSettings = material->GetMaterialSettings( );
        
        //if( skipNoDepthPrePass )
        //{
        //    if( materialSettings.NoDepthPrePass )
        //        continue;
        //}
        //if( skipTransparencies )
        //{
        //    assert( !skipNonTransparencies );
        //    if( materialSettings.IsTransparent() )
        //        continue;
        //}
        //if( skipNonTransparencies )
        //{
        //    assert( !skipTransparencies );
        //    if( !materialSettings.IsTransparent() )
        //        continue;
        //}
        if( skipNonShadowCasters )
        {
            if( !materialSettings.CastShadows )
                continue;
        }

        // reset render item
        renderItem = commonRenderItem;

        // should probably be modifiable by the material as well?
        renderItem.ShadingRate      = ((drawFlags & vaRenderMeshDrawFlags::DisableVRS)==0)?(entry.ShadingRate):(vaShadingRate::ShadingRate1X1);

        if( lastrate != renderItem.ShadingRate )
        {
            lastrate = renderItem.ShadingRate;
            ratechanges++;
        }

        bool isWireframe = ( ( drawContext.RenderFlags & vaDrawContextFlags::DebugWireframePass ) != 0 ) || materialSettings.Wireframe;

        // mesh data
        const shared_ptr<vaRenderMesh::StandardTriangleMesh> & triangleMesh = mesh.GetTriangleMesh( );

        triangleMesh->UpdateAndSetToRenderItem( drawContext.RenderDeviceContext, renderItem );

        // update per-instance constants
        ShaderInstanceConstants instanceConsts;
        {
            instanceConsts.World = entry.Transform;
            
            // this means 'do not override'
            instanceConsts.CustomColor = entry.CustomColor;

            // since we now support non-uniform scale, we need the 'normal matrix' to keep normals correct 
            // (for more info see : https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/geometry/transforming-normals or http://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/ )
            instanceConsts.NormalWorld = entry.Transform.Inversed( nullptr, false ).Transposed( );
            instanceConsts.NormalWorld.Row(0).w = 0.0f; instanceConsts.NormalWorld.Row(1).w = 0.0f; instanceConsts.NormalWorld.Row(2).w = 0.0f;
            instanceConsts.NormalWorld.Row(3).x = 0.0f; instanceConsts.NormalWorld.Row(3).y = 0.0f; instanceConsts.NormalWorld.Row(3).z = 0.0f; instanceConsts.NormalWorld.Row(3).w = 1.0f;

            //if( drawType != vaDrawType::ShadowmapGenerate )
            {
                ////instanceConsts.WorldView = instanceConsts.World * drawContext.Camera.GetViewMatrix( );
                //instanceConsts.ShadowWorldViewProj = vaMatrix4x4::Identity;

                ////instanceConsts.NormalWorldView = instanceConsts.NormalWorld * drawContext.Camera.GetViewMatrix( );
            }
            ////instanceConsts.ShadingRate = vaVector4( vaShadingRateToVector2( renderItem.ShadingRate ), 0, 0 );
        }

        assert( (subPart.IndexStart + subPart.IndexCount) <= (int)triangleMesh->Indices().size() );

        bool showMaterialSelected = mesh.GetUIShowSelectedFrameIndex( ) >= GetRenderDevice().GetCurrentFrameIndex() || material->GetUIShowSelectedFrameIndex( ) >= GetRenderDevice().GetCurrentFrameIndex();
        if( isWireframe )
        {
            if( material->IsTransparent() )
                instanceConsts.CustomColor = vaVector4( 0.0f, 0.5f, 0.0f, 1.0f );
            else
                instanceConsts.CustomColor = vaVector4( 0.5f, 0.0f, 0.0f, 1.0f );
        }
        if( showMaterialSelected )
        {
            float highlight = 0.5f * (float)vaMath::Sin( drawContext.RenderDeviceContext.GetRenderDevice().GetTotalTime( ) * VA_PI * 2.0 ) + 0.5f;
            instanceConsts.CustomColor = vaVector4( highlight, highlight, highlight, 1.0f - highlight );
        }

        if( !material->SetToRenderItem( renderItem, shaderType, drawResults ) )
        {
            // VA_WARN( "material->SetToRenderItem returns false, using default material instead" );
            material = nullptr;
            drawResults |= vaDrawResultFlags::AssetsStillLoading;
            continue;
        }

        m_constantsBuffer.Update( drawContext.RenderDeviceContext, instanceConsts );

        renderItem.FillMode                 = (isWireframe)?(vaFillMode::Wireframe):(vaFillMode::Solid);
        renderItem.CullMode                 = materialSettings.FaceCull;
        renderItem.FrontCounterClockwise    = mesh.GetFrontFaceWindingOrder() == vaWindingOrder::CounterClockwise;

        renderItem.SetDrawIndexed( subPart.IndexCount, subPart.IndexStart, 0 );

        // apply overrides, if any
        if( entry.CustomHandler != nullptr )
            entry.CustomHandler->ApplyMeshCustomization( drawContext, entry, *material, renderItem );
        if( globalCustomizer )
            globalCustomizer( entry, *material, renderItem );

#ifdef VA_AUTO_TWO_PASS_TRANSPARENCIES_ENABLED
        if( materialSettings.Transparent && materialSettings.FaceCull != vaFaceCull::None )
        {
            renderItem.CullMode = ( materialSettings.FaceCull == vaFaceCull::Front ) ? ( vaFaceCull::Back ) : ( vaFaceCull::Front );
            drawResults |= drawContext.RenderDeviceContext.ExecuteItem( renderItem );
            renderItem.CullMode = ( materialSettings.FaceCull == vaFaceCull::Front ) ? ( vaFaceCull::Front ) : ( vaFaceCull::Back );
            drawResults |= drawContext.RenderDeviceContext.ExecuteItem( renderItem );
        }
        else
#endif
        {
            drawResults |= drawContext.RenderDeviceContext.ExecuteItem( renderItem );
        }
    }

    drawContext.RenderDeviceContext.EndItems();
    return drawResults;
}
