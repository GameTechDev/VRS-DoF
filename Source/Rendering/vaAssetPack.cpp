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

#include "vaAssetPack.h"

#include "Rendering/vaRenderMesh.h"
#include "Rendering/vaRenderMaterial.h"

#include "Rendering/Misc/vaTextureReductionTestTool.h"

#include "Core/System/vaCompressionStream.h"

#include "Core/System/vaFileTools.h"

#include "Core/vaApplicationBase.h"

#include "IntegratedExternals/vaImguiIntegration.h"

#include "Rendering/vaTextureHelpers.h"

using namespace Vanilla;

vaAssetTexture::vaAssetTexture( vaAssetPack & pack, const shared_ptr<vaTexture> & texture, const string & name ) 
    : vaAsset( pack, vaAssetType::Texture, name, texture )
{ }

vaAssetRenderMesh::vaAssetRenderMesh( vaAssetPack & pack, const shared_ptr<vaRenderMesh> & mesh, const string & name ) 
    : vaAsset( pack, vaAssetType::RenderMesh, name, mesh )
{ }

vaAssetRenderMaterial::vaAssetRenderMaterial( vaAssetPack & pack, const shared_ptr<vaRenderMaterial> & material, const string & name ) 
    : vaAsset( pack, vaAssetType::RenderMaterial, name, material )
{ }

vaAssetPack::vaAssetPack( vaAssetPackManager & assetPackManager, const string & name ) : 
    m_assetPackManager(assetPackManager), m_name( name ), 
    vaUIPanel( "asset", 0, false, vaUIPanel::DockLocation::DockedRight, "Assets" )
{
    assert( vaThreading::IsMainThread() );
}

vaAssetPack::~vaAssetPack( )
{
    assert( vaThreading::IsMainThread() );

    WaitUntilIOTaskFinished( );
    RemoveAll( true );
}

void vaAssetPack::InsertAndTrackMe( shared_ptr<vaAsset> newAsset, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    m_assetMap.insert( std::pair< string, shared_ptr<vaAsset> >( vaStringTools::ToLower( newAsset->Name() ), newAsset ) );
    //    if( storagePath != L"" )
    //        m_assetMapByStoragePath.insert( std::pair< wstring, shared_ptr<vaAsset> >( vaStringTools::ToLower(newAsset->storagePath), newAsset ) );

    assert( newAsset->m_parentPackStorageIndex == -1 );
    m_assetList.push_back( newAsset );
    newAsset->m_parentPackStorageIndex = ((int)m_assetList.size())-1;

    // it's ok if it's already tracked - for ex. textures are always tracked 
    //if( !
        newAsset->GetResource()->UIDObject_Track( ); // )
    //    VA_LOG_ERROR_STACKINFO( "Error registering asset '%s' - UID already used; this means there's another asset somewhere with the same UID", newAsset->Name().c_str() );
}

namespace
{
    static const int c_assetItemMaxNameLength = 64;
    static bool IsNumber( char c ) { return c >= '0' && c <= '9'; }
};

string vaAssetPack::FindSuitableAssetName( const string & _nameSuggestion, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    string nameSuggestion = vaStringTools::ToLower( _nameSuggestion );
    std::replace( nameSuggestion.begin(), nameSuggestion.end(), '.', '_');
    std::replace( nameSuggestion.begin(), nameSuggestion.end(), ' ', '_');

    string newSuggestion = nameSuggestion;

    if( newSuggestion.length( ) > ( c_assetItemMaxNameLength - 3 ) )
        newSuggestion = newSuggestion.substr( 0, c_assetItemMaxNameLength - 3 );
    if( Find( newSuggestion, false ) == nullptr )
        return newSuggestion;

    int index = 0;
    do 
    {
        // if last 3 characters are in _00 format, remove them
        if( newSuggestion.length( ) > 3 && newSuggestion[newSuggestion.length( ) - 3] == '_' && IsNumber( newSuggestion[newSuggestion.length( ) - 2] ) && IsNumber( newSuggestion[newSuggestion.length( ) - 1] ) )
            newSuggestion = newSuggestion.substr( 0, newSuggestion.length( ) - 3 );
        if( newSuggestion.length( ) > ( c_assetItemMaxNameLength - 3 ) )
            newSuggestion = newSuggestion.substr( 0, c_assetItemMaxNameLength - 3 );

        if( index > 99 )
            newSuggestion = vaCore::GUIDToStringA( vaCore::GUIDCreate( ) );
        else
            newSuggestion += vaStringTools::Format( "_%02d", index );

        if( Find( newSuggestion, false ) == nullptr )
            return newSuggestion;

        index++;
    } while ( true );

}

shared_ptr<vaAssetTexture> vaAssetPack::Add( const std::shared_ptr<vaTexture> & texture, const string & name, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    if( Find( name, false ) != nullptr )
    {
        assert( false );
        VA_LOG_ERROR( "Unable to add asset '%s' to the asset pack '%s' because the name already exists", name.c_str( ), m_name.c_str( ) );
        return nullptr;
    }
//    assert( ( storagePath == L"" ) || ( FindByStoragePath( storagePath ) == nullptr ) );    // assets in packs must have unique names
    
    shared_ptr<vaAssetTexture> newItem = shared_ptr<vaAssetTexture>( new vaAssetTexture( *this, texture, name ) );

    InsertAndTrackMe( newItem, false );

    return newItem;
}

shared_ptr<vaAssetRenderMesh> vaAssetPack::Add( const std::shared_ptr<vaRenderMesh> & mesh, const string & name, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    if( Find( name, false ) != nullptr )
    {
        assert( false );
        VA_LOG_ERROR( "Unable to add asset '%s' to the asset pack '%s' because the name already exists", name.c_str( ), m_name.c_str( ) );
        return nullptr;
    }
//    assert( ( storagePath == L"" ) || ( FindByStoragePath( storagePath ) == nullptr ) );    // assets in packs must have unique names

    shared_ptr<vaAssetRenderMesh> newItem = shared_ptr<vaAssetRenderMesh>( new vaAssetRenderMesh( *this, mesh, name ) );

    InsertAndTrackMe( newItem, false );

    return newItem;
}

shared_ptr<vaAssetRenderMaterial> vaAssetPack::Add( const std::shared_ptr<vaRenderMaterial> & material, const string & name, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    if( Find( name, false ) != nullptr )
    {
        assert( false );
        VA_LOG_ERROR( "Unable to add asset '%s' to the asset pack '%s' because the name already exists", name.c_str(), m_name.c_str() );
        return nullptr;
    }
//    assert( (storagePath == L"") || (FindByStoragePath( storagePath ) == nullptr) );    // assets in packs must have unique names

    shared_ptr<vaAssetRenderMaterial> newItem = shared_ptr<vaAssetRenderMaterial>( new vaAssetRenderMaterial( *this, material, name ) );

    InsertAndTrackMe( newItem, false );

    return newItem;
}

string vaAsset::GetTypeNameString( vaAssetType assetType )
{
    switch( assetType )
    {
    case vaAssetType::Texture:          return "texture";       break;
    case vaAssetType::RenderMesh:       return "rendermesh";    break;
    case vaAssetType::RenderMaterial:   return "material";      break;
    default: assert( false );           return "unknown";       break;    
    }
}

bool vaAsset::Rename( const string & newName )    
{ 
    return m_parentPack.RenameAsset( *this, newName, true ); 
}

bool vaAssetPack::RenameAsset( vaAsset & asset, const string & newName, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    if( &asset.m_parentPack != this )
    {
        VA_LOG_ERROR( "Unable to change asset name from '%s' to '%s' in asset pack '%s' - not correct parent pack!", asset.Name().c_str(), newName.c_str(), this->m_name.c_str()  );
        return false;
    }
    if( newName == asset.Name() )
    {
        VA_LOG( "Changing asset name from '%s' to '%s' in asset pack '%s' - same name requested? Nothing changed.", asset.Name().c_str(), newName.c_str(), this->m_name.c_str()  );
        return true;
    }
    if( Find( newName, false ) != nullptr )
    {
        VA_LOG_ERROR( "Unable to change asset name from '%s' to '%s' in asset pack '%s' - name already used by another asset!", asset.Name().c_str(), newName.c_str(), this->m_name.c_str()  );
        return false;
    }

    {
        auto it = m_assetMap.find( vaStringTools::ToLower( asset.Name() ) );

        shared_ptr<vaAsset> assetSharedPtr;

        if( it != m_assetMap.end() ) 
        {
            assetSharedPtr = it->second;
            assert( assetSharedPtr.get() == &asset );
            m_assetMap.erase( it );
        }
        else
        {
            VA_LOG_ERROR( "Error changing asset name from '%s' to '%s' in asset pack '%s' - original asset not found!", asset.Name().c_str(), newName.c_str(), this->m_name.c_str()  );
            return false;
        }

        assetSharedPtr->m_name = newName;

        m_assetMap.insert( std::pair< string, shared_ptr<vaAsset> >( vaStringTools::ToLower( assetSharedPtr->Name() ), assetSharedPtr ) );
    }

    VA_LOG( "Changing asset name from '%s' to '%s' in asset pack '%s' - success!", asset.Name().c_str(), newName.c_str(), this->m_name.c_str() );
    return true;
}

void vaAssetPack::Remove( const shared_ptr<vaAsset> & asset, bool lockMutex )
{
    Remove( asset.get(), lockMutex );
}

void vaAssetPack::Remove( vaAsset * asset, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    if( asset == nullptr )
        return;

    if( !asset->GetResource()->UIDObject_Untrack( ) )
        VA_LOG_ERROR_STACKINFO( "Error untracking asset '%s' - not sure why it wasn't properly tracked", asset->Name().c_str() );

    assert( m_assetList[asset->m_parentPackStorageIndex].get() == asset );
    if( (int)m_assetList.size( ) != ( asset->m_parentPackStorageIndex + 1 ) )
    {
        m_assetList[ asset->m_parentPackStorageIndex ] = m_assetList[ m_assetList.size( ) - 1 ];
        m_assetList[ asset->m_parentPackStorageIndex ]->m_parentPackStorageIndex = asset->m_parentPackStorageIndex;
    }
    asset->m_parentPackStorageIndex = -1;
    m_assetList.pop_back();

    {
        auto it = m_assetMap.find( vaStringTools::ToLower(asset->Name()) );

        // possible memory leak! does the asset belong to another asset pack?
        assert( it != m_assetMap.end() );
        if( it == m_assetMap.end() ) 
            return; 
    
        m_assetMap.erase( it );
    }
    
//    if( asset->StoragePath != L"" )
//    {
//        auto it = m_assetMapByStoragePath.find( vaStringTools::ToLower(asset->StoragePath) );
//
//        // can't not be in!
//        assert( it != m_assetMapByStoragePath.end( ) );
//        if( it == m_assetMapByStoragePath.end( ) )
//            return;
//
//        m_assetMapByStoragePath.erase( it );
//    }

}

void vaAssetPack::RemoveAll( bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    assert( m_ioTask == nullptr || vaBackgroundTaskManager::GetInstance().IsFinished(m_ioTask) );
    m_storageMode = vaAssetPack::StorageMode::Unknown;

    // untrack the asset resources so no one can find them anymore using vaUIDObjectRegistrar
    for( int i = 0; i < m_assetList.size( ); i++ )
    {
        if( !m_assetList[i]->GetResource()->UIDObject_Untrack( ) )
            VA_LOG_ERROR_STACKINFO( "Error untracking asset '%s' - not sure why it wasn't properly tracked", m_assetList[i]->Name().c_str() );
    }

    m_assetList.clear( );
//    m_assetMapByStoragePath.clear( );

    for( auto it = m_assetMap.begin( ); it != m_assetMap.end( ); it++ )
    {
        // if this fails it means someone is still holding a reference to assets from this pack - this shouldn't ever happen, they should have been released!
        assert( it->second.use_count() == 1 ); // this assert is not entirely correct for multithreaded scenarios so beware
    }
    m_assetMap.clear();
}

const int c_packFileVersion = 3;

bool vaAssetPack::IsBackgroundTaskActive( ) const
{
    assert( vaThreading::IsMainThread() );
    if( m_ioTask != nullptr )
        return !vaBackgroundTaskManager::GetInstance().IsFinished( m_ioTask );
    return false;
}

void vaAssetPack::WaitUntilIOTaskFinished( bool breakIfSafe )
{
    assert( vaThreading::IsMainThread() );
    assert( !breakIfSafe ); // not implemented/tested
    breakIfSafe;

    if( m_ioTask != nullptr )
    {
        vaBackgroundTaskManager::GetInstance().WaitUntilFinished( m_ioTask );
        m_ioTask = nullptr;
    }
    {
        std::unique_lock<mutex> apackStorageLock(m_apackStorageMutex);
        assert( !m_apackStorage.IsOpen() );
    }
}

bool vaAssetPack::SaveAPACK( const wstring & fileName, bool lockMutex )
{
    WaitUntilIOTaskFinished( );

    std::unique_lock<mutex> apackStorageLock(m_apackStorageMutex);
    if( !m_apackStorage.Open( fileName, FileCreationMode::Create ) )
    {
        VA_LOG_ERROR( L"vaAssetPack::SaveAPACK(%s) - unable to create file for saving", fileName.c_str() );
        return false;
    }

    vaStream & outStream = m_apackStorage;

    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    VERIFY_TRUE_RETURN_ON_FALSE( outStream.CanSeek( ) );

    int64 posOfSize = outStream.GetPosition( );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int64>( 0 ) );

    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int32>( c_packFileVersion ) );

    bool useWholeFileCompression = true;
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<bool>( useWholeFileCompression ) );

    // If compressing, have to write into memory buffer for later compression because seeking while
    // writing directly to a compression stream is not supported, and it's used here.
    vaMemoryStream memStream( (int64)0, (useWholeFileCompression)?( 16*1024 ): (0) );
    vaStream & outStreamInner = (useWholeFileCompression)?( memStream ):( outStream );

    VERIFY_TRUE_RETURN_ON_FALSE( outStreamInner.WriteValue<int32>( (int)m_assetMap.size() ) );

    for( auto it = m_assetMap.begin( ); it != m_assetMap.end( ); it++ )
    {
        int64 posOfSubSize = outStreamInner.GetPosition( );
        VERIFY_TRUE_RETURN_ON_FALSE( outStreamInner.WriteValue<int64>( 0 ) );

        // write type
        VERIFY_TRUE_RETURN_ON_FALSE( outStreamInner.WriteValue<int32>( (int32)it->second->Type ) );

        // write name
        VERIFY_TRUE_RETURN_ON_FALSE( outStreamInner.WriteString( it->first ) );
        assert( vaStringTools::CompareNoCase( it->first, it->second->Name() ) == 0 );

        // write asset resource ID
        VERIFY_TRUE_RETURN_ON_FALSE( outStreamInner.WriteValue<vaGUID>( it->second->GetResourceObjectUID() ) );

        // write asset
        VERIFY_TRUE_RETURN_ON_FALSE( it->second->SaveAPACK( outStreamInner ) );

        int64 calculatedSubSize = outStreamInner.GetPosition() - posOfSubSize;
        // auto aaaa = outStreamInner.GetPosition();
        outStreamInner.Seek( posOfSubSize );
        VERIFY_TRUE_RETURN_ON_FALSE( outStreamInner.WriteValue<int64>( calculatedSubSize ) );
        outStreamInner.Seek( posOfSubSize + calculatedSubSize );
    }

    if( useWholeFileCompression )
    {
        vaCompressionStream outCompressionStream( false, &outStream );
        // just dump the whole buffer and we're done!
        VERIFY_TRUE_RETURN_ON_FALSE( outCompressionStream.Write( memStream.GetBuffer(), memStream.GetLength() ) );
    }

    int64 calculatedSize = outStream.GetPosition( ) - posOfSize;
    outStream.Seek( posOfSize );
    VERIFY_TRUE_RETURN_ON_FALSE( outStream.WriteValue<int64>( calculatedSize ) );
    outStream.Seek( posOfSize + calculatedSize );

    m_apackStorage.Close();
    m_storageMode = StorageMode::APACK;

    return true;
}

bool vaAssetPack::LoadAPACKInner( vaStream & inStream, vector< shared_ptr<vaAsset> > & loadedAssets, vaBackgroundTaskManager::TaskContext & taskContext )
{
    m_assetStorageMutex.assert_locked_by_caller();

    int32 numberOfAssets = 0;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( numberOfAssets ) );

    for( int i = 0; i < numberOfAssets; i++ )
    {
        taskContext.Progress = float(i) / float(numberOfAssets-1);

        int64 subSize = 0;
        VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int64>( subSize ) );

        // read type
        vaAssetType assetType;
        VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( (int32&)assetType ) );

        // read name
        string newAssetName;
        VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadString( newAssetName ) );

        string suitableName = FindSuitableAssetName( newAssetName, false );
        if( suitableName != newAssetName )
        {
            VA_LOG_WARNING( "There's already an asset with the name '%s' or the name has disallowed characters - renaming the new one to '%s'", newAssetName.c_str(), suitableName.c_str() );
            newAssetName = suitableName;
        }
        if( Find( newAssetName, false ) != nullptr )
        {
            VA_LOG_ERROR( L"vaAssetPack::Load(): duplicated asset name, stopping loading." );
            assert( false );
            return false;
        }

        shared_ptr<vaAsset> newAsset = nullptr;

        switch( assetType )
        {
        case Vanilla::vaAssetType::Texture:
            newAsset = shared_ptr<vaAsset>( vaAssetTexture::CreateAndLoadAPACK( *this, newAssetName, inStream ) );
            break;
        case Vanilla::vaAssetType::RenderMesh:
            newAsset = shared_ptr<vaAsset>( vaAssetRenderMesh::CreateAndLoadAPACK( *this, newAssetName, inStream ) );
            break;
        case Vanilla::vaAssetType::RenderMaterial:
            newAsset = shared_ptr<vaAsset>( vaAssetRenderMaterial::CreateAndLoadAPACK( *this, newAssetName, inStream ) );
            break;
        default:
            break;
        }

        if( newAsset == nullptr )
        {
            VA_LOG_ERROR( "Error while loading an asset - see log file above for more info - aborting loading." );
            return false;
        }
        else
        {
            VERIFY_TRUE_RETURN_ON_FALSE( newAsset != nullptr );

            InsertAndTrackMe( newAsset, false );

            loadedAssets.push_back( newAsset );
        }
    }
    return true;
}

bool vaAssetPack::LoadAPACK( const wstring & fileName, bool async, bool lockMutex )
{
    WaitUntilIOTaskFinished( );

    std::unique_lock<mutex> apackStorageLock(m_apackStorageMutex);
    if( !m_apackStorage.Open( fileName, FileCreationMode::Open, FileAccessMode::Read ) )
    {
        VA_LOG_ERROR( L"vaAssetPack::LoadAPACK(%s) - unable to open file for reading", fileName.c_str() );
        return false;
    }

    vaStream & inStream = m_apackStorage;

    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    RemoveAll( false );

    int64 size = 0;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int64>( size ) );

    int32 fileVersion = 0;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<int32>( fileVersion ) );
    if( fileVersion < 1 || fileVersion > 3 )
    {
        VA_LOG_ERROR( L"vaAssetPack::Load(): unsupported file version" );
        return false;
    }

    m_storageMode = StorageMode::APACK;

    bool useWholeFileCompression = false;
    if( fileVersion >= 3 )
    {
        VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<bool>( useWholeFileCompression ) );
    }

    // ok let the loading thread grab the locks again before continuing with file access 
    // if( lockMutex )
    //     assetStorageMutexLock.unlock();
    // apackStorageLock.unlock();

    // async stuff here. 
    auto loadingLambda = [this, &inStream, useWholeFileCompression]( vaBackgroundTaskManager::TaskContext & context ) 
    {
        vector< shared_ptr<vaAsset> > loadedAssets;

        std::unique_lock<mutex> apackStorageLock(m_apackStorageMutex);
        std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex);

        bool success;
        if( useWholeFileCompression )
        {
            vaCompressionStream decompressor( true, &inStream );
            success = LoadAPACKInner( decompressor, loadedAssets, context );
        }
        else
        {
            success = LoadAPACKInner( inStream, loadedAssets, context );
        }

        m_apackStorage.Close();

        if( !success )
        {
            VA_LOG_ERROR( L"vaAssetPack::Load(): internal error during loading" );
            RemoveAll( false );
        }

        return success;
    };

    if( async )
    {
        m_ioTask = vaBackgroundTaskManager::GetInstance().Spawn( vaStringTools::Format("Loading '%s.apack'", m_name.c_str() ), vaBackgroundTaskManager::SpawnFlags::ShowInUI, loadingLambda );
    }
    else
    {
        // assetStorageMutexLock.unlock();
        return loadingLambda( vaBackgroundTaskManager::TaskContext() );
    }

    return true;
}

bool vaAssetPack::SaveUnpacked( const wstring & folderRoot, bool lockMutex )
{
    assert(vaThreading::IsMainThread()); 
    WaitUntilIOTaskFinished( );

    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    if( vaFileTools::DirectoryExists(folderRoot) && !vaFileTools::DeleteDirectory( folderRoot ) )
    {
        VA_LOG_ERROR( L"vaAssetPack::SaveUnpacked - Unable to delete current contents of the folder '%s'", folderRoot.c_str( ) );
        return false;
    }
    vaFileTools::EnsureDirectoryExists( folderRoot );

    vaFileStream headerFile;
    if( !headerFile.Open( folderRoot + L"AssetPack.xml", FileCreationMode::OpenOrCreate, FileAccessMode::Write ) )
    {
        VA_LOG_ERROR( L"vaAssetPack::SaveUnpacked - Unable to open '%s'", (folderRoot + L"/AssetPack.xml").c_str() );
        return false;
    }

    vaXMLSerializer serializer;
    GetRenderDevice( ).GetMaterialManager( ).RegisterSerializationTypeConstructors( serializer );

    serializer.SerializeOpenChildElement( "VanillaAssetPack" );

    int32 packFileVersion = c_packFileVersion;
    serializer.Serialize<int32>( "FileVersion", packFileVersion );
    //serializer.WriteAttribute( "Name", m_name );

    bool hadError = false;

    for( vaAssetType assetType = (vaAssetType)0; assetType < vaAssetType::MaxVal; assetType = (vaAssetType)((int32)assetType+1) )
    {
        string assetTypeName = vaAsset::GetTypeNameString( assetType );
        wstring assetTypeFolder = folderRoot + vaStringTools::SimpleWiden(assetTypeName) + L"s";

        if( !vaFileTools::EnsureDirectoryExists( assetTypeFolder ) )
        {
            assert( false );
            hadError = true;
        }

        for( auto it = m_assetMap.begin( ); it != m_assetMap.end( ); it++ )
        {
            shared_ptr<vaAsset> & asset = it->second;
            if( asset->Type != assetType ) continue;
            if( asset->GetResourceObjectUID( ) == vaCore::GUIDNull( ) )
            {
                assert( false );
                hadError = true;
                continue;
            }

            wstring assetFolder = assetTypeFolder + L"\\" + vaStringTools::SimpleWiden( asset->Name() ) + L"." + vaCore::GUIDToString( asset->GetResourceObjectUID() );
            if( !vaFileTools::EnsureDirectoryExists( assetFolder ) )
            {
                assert( false );
                hadError = true;
                continue;
            }

            vaFileStream assetHeaderFile;
            if( !assetHeaderFile.Open( assetFolder + L"\\Asset.xml", FileCreationMode::OpenOrCreate, FileAccessMode::Write ) )
            {
                VA_LOG_ERROR( L"vaAssetPack::SaveUnpacked - Unable to open '%s'", (assetFolder + L"\\Asset.xml").c_str() );
                serializer.SerializePopToParentElement( "VanillaAssetPack" );
                return false;
            }
            vaXMLSerializer assetSerializer;
            GetRenderDevice( ).GetMaterialManager( ).RegisterSerializationTypeConstructors( assetSerializer );

            string storageName = string("Asset_") + assetTypeName;
            assetSerializer.SerializeOpenChildElement( storageName.c_str() );
            if( !it->second->SerializeUnpacked( assetSerializer, assetFolder ) )
            {
                assert( false );
                hadError = true;
            }
            assetSerializer.SerializePopToParentElement( storageName.c_str() );
            assetSerializer.WriterSaveToFile( assetHeaderFile );
            assetHeaderFile.Close();
        }
    }

    serializer.SerializePopToParentElement("VanillaAssetPack");

    serializer.WriterSaveToFile( headerFile );

    headerFile.Close();

    m_storageMode = StorageMode::Unpacked;

    return !hadError;
}

bool vaAssetPack::LoadUnpacked( const wstring & folderRoot, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();

    RemoveAll( false );

    vaFileStream headerFile;
    if( !headerFile.Open( folderRoot + L"\\AssetPack.xml", FileCreationMode::Open, FileAccessMode::Read ) )
    {
        VA_LOG_ERROR( L"vaAssetPack::LoadUnpacked - Unable to open '%s'", (folderRoot + L"\\AssetPack.xml").c_str() );
        return false;
    }

    vaXMLSerializer serializer( headerFile );
    GetRenderDevice( ).GetMaterialManager( ).RegisterSerializationTypeConstructors( serializer );
    headerFile.Close();

    bool oldFormat = serializer.SerializeOpenChildElement( "VertexAsylumAssetPack" );

    if( !oldFormat )
    {
        VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializeOpenChildElement( "VanillaAssetPack" ) );
    }

    int32 fileVersion = -1;
    VERIFY_TRUE_RETURN_ON_FALSE( serializer.Serialize<int32>( "FileVersion", (int32)fileVersion ) );
    // VERIFY_TRUE_RETURN_ON_FALSE( fileVersion == c_packFileVersion );

    // already named at creation
    // VERIFY_TRUE_RETURN_ON_FALSE( serializer.ReadAttribute( "Name", m_name ) );

    bool hadError = false;
    vector< shared_ptr<vaAsset> > loadedAssets;

    for( vaAssetType assetType = (vaAssetType)0; assetType < vaAssetType::MaxVal; assetType = (vaAssetType)((int32)assetType+1) )
    {
        string assetTypeName = vaAsset::GetTypeNameString( assetType );
        wstring assetTypeFolder = folderRoot + L"\\" + vaStringTools::SimpleWiden(assetTypeName) + L"s";

        if( !vaFileTools::DirectoryExists( assetTypeFolder ) )
        {
            // ok, just no assets of this type
            continue;
        }

        vector<wstring> subDirs = vaFileTools::FindDirectories( assetTypeFolder + L"\\" );

        for( const wstring & subDir : subDirs )
        { 
            size_t nameStartA = subDir.rfind('\\');
            size_t nameStartB = subDir.rfind('/');
            nameStartA = ( nameStartA == wstring::npos ) ? ( 0 ) : ( nameStartA + 1 );
            nameStartB = ( nameStartB == wstring::npos ) ? ( 0 ) : ( nameStartB + 1 );
            wstring dirName = subDir.substr( vaMath::Max( nameStartA, nameStartB ) );
            if( dirName.length() == 0 ) 
                { assert( false ); continue; }
            size_t separator = dirName.rfind('.');
            if( separator == wstring::npos )
                { assert( false ); continue; }
            string assetName   = vaStringTools::SimpleNarrow( dirName.substr( 0, separator ) );
            vaGUID assetGUID   = vaCore::GUIDFromString( dirName.substr( separator+1 ) );
            if( assetName == "" )
                { assert( false ); continue; }
            if( assetGUID == vaCore::GUIDNull() )
                { assert( false ); continue; }
            
            vaFileStream assetHeaderFile;
            if( !assetHeaderFile.Open( subDir + L"\\Asset.xml", FileCreationMode::Open, FileAccessMode::Read ) )
            {
                VA_LOG_ERROR( L"vaAssetPack::LoadUnpacked - Unable to open '%s'", (subDir + L"\\Asset.xml").c_str() );
                return false;
            }
            vaXMLSerializer assetSerializer( assetHeaderFile );
            GetRenderDevice().GetMaterialManager().RegisterSerializationTypeConstructors( assetSerializer ); 
            assetHeaderFile.Close();

            string suitableName = FindSuitableAssetName( assetName, false );
            if( suitableName != assetName )
            {
                VA_LOG_WARNING( "There's already an asset with the name '%s' - renaming the new one to '%s'", assetName.c_str(), suitableName.c_str() );
                assetName = suitableName;
            }
            if( Find( assetName, false ) != nullptr )
            {
                VA_LOG_ERROR( L"vaAssetPack::LoadUnpacked(): duplicated asset name, stopping loading." );
                assert( false );
                return false;
            }

            string storageName = string("Asset_") + assetTypeName;

            VERIFY_TRUE_RETURN_ON_FALSE( assetSerializer.SerializeOpenChildElement( storageName.c_str() ) );

            shared_ptr<vaAsset> newAsset = nullptr;

            switch( assetType )
            {
            case Vanilla::vaAssetType::Texture:
                newAsset = shared_ptr<vaAsset>( vaAssetTexture::CreateAndLoadUnpacked( *this, assetName, assetGUID, assetSerializer, subDir ) );
                break;
            case Vanilla::vaAssetType::RenderMesh:
                newAsset = shared_ptr<vaAsset>( vaAssetRenderMesh::CreateAndLoadUnpacked( *this, assetName, assetGUID, assetSerializer, subDir ) );
                break;
            case Vanilla::vaAssetType::RenderMaterial:
                newAsset = shared_ptr<vaAsset>( vaAssetRenderMaterial::CreateAndLoadUnpacked( *this, assetName, assetGUID, assetSerializer, subDir ) );
                break;
            default:
                break;
            }
            VERIFY_TRUE_RETURN_ON_FALSE( newAsset != nullptr );

            InsertAndTrackMe( newAsset, false );

            loadedAssets.push_back( newAsset );

            // if( !it->second->SerializeUnpacked( assetSerializer, assetFolder ) )
            // {
            //     assert( false );
            //     hadError = true;
            // }
            VERIFY_TRUE_RETURN_ON_FALSE( assetSerializer.SerializePopToParentElement( storageName.c_str() ) );
        }
    }

    if( oldFormat )
        VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializePopToParentElement( "VertexAsylumAssetPack" ) );
    else
        VERIFY_TRUE_RETURN_ON_FALSE( serializer.SerializePopToParentElement( "VanillaAssetPack" ) );

    if( !hadError )
    {
        m_storageMode = StorageMode::Unpacked;
        return true;
    }
    else
    {
        RemoveAll( false );
        return false;
    }
}

shared_ptr<vaAsset> vaAsset::GetSharedPtr( ) const 
{ 
    return m_parentPack.AssetAt( m_parentPackStorageIndex, true ); 
}

shared_ptr<vaAssetPack> vaAssetPack::GetSharedPtr( ) const
{
    return m_assetPackManager.FindLoadedPack( m_name );
}

void vaAsset::ReplaceAssetResource( const shared_ptr<vaAssetResource> & newResource )
{
    assert( m_resource != nullptr );

    // m_parentPack.GetAssetStorageMutex() ?

    assert( m_resource->UIDObject_IsTracked() );

    // This is done so that all other assets or systems referencing the texture by the ID now
    // point to the new one!
    vaUIDObjectRegistrar::GetInstance( ).SwapIDs( *m_resource, *newResource );

    m_resource->SetParentAsset( nullptr );
    m_resource = newResource;
    m_resource->SetParentAsset( this );

    assert( m_resource->UIDObject_IsTracked() );
}

bool vaAsset::SaveAPACK( vaStream & outStream )
{
    assert( m_resource != nullptr );
    return m_resource->SaveAPACK( outStream );
}

//void vaAsset::LoadAPACK( vaStream & inStream )
//{
//    assert( m_resource != nullptr );
//    m_resource->SaveAPACK( inStream );
//}

bool vaAsset::SerializeUnpacked( vaXMLSerializer & serializer, const wstring & assetFolder )
{
    assert( m_resource != nullptr );
    return m_resource->SerializeUnpacked( serializer, assetFolder );
}

vaAssetRenderMesh * vaAssetRenderMesh::CreateAndLoadAPACK( vaAssetPack & pack, const string & name, vaStream & inStream )
{
    vaGUID uid;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaGUID>( uid ) );

    shared_ptr<vaRenderMesh> newResource = pack.GetRenderDevice().GetMeshManager().CreateRenderMesh( uid, false );

    if( newResource == nullptr )
        return nullptr;

    if( newResource->LoadAPACK( inStream ) )
    {
        return new vaAssetRenderMesh( pack, newResource, name );
    }
    else
    {
        VA_LOG_ERROR_STACKINFO( "Error loading asset render mesh" );
        return nullptr;
    }
}

vaAssetRenderMaterial * vaAssetRenderMaterial::CreateAndLoadAPACK( vaAssetPack & pack, const string & name, vaStream & inStream )
{
    vaGUID uid;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaGUID>( uid ) );

    shared_ptr<vaRenderMaterial> newResource = pack.GetRenderDevice().GetMaterialManager().CreateRenderMaterial( uid, false );

    if( newResource == nullptr )
        return nullptr;

    if( newResource->LoadAPACK( inStream ) )
    {
        return new vaAssetRenderMaterial( pack, newResource, name );
    }
    else
    {
        return nullptr;
    }
}

vaAssetTexture * vaAssetTexture::CreateAndLoadAPACK( vaAssetPack & pack, const string & name, vaStream & inStream )
{
    vaGUID uid;
    VERIFY_TRUE_RETURN_ON_FALSE( inStream.ReadValue<vaGUID>( uid ) );

    shared_ptr<vaTexture> newResource = VA_RENDERING_MODULE_CREATE_SHARED( vaTexture, vaTextureConstructorParams( pack.GetRenderDevice(), uid ) );

    if( newResource == nullptr )
        return nullptr;

    if( newResource->LoadAPACK( inStream ) )
    {
        return new vaAssetTexture( pack, newResource, name );
    }
    else
    {
        return nullptr;
    }
}

vaAssetRenderMesh * vaAssetRenderMesh::CreateAndLoadUnpacked( vaAssetPack & pack, const string & name, const vaGUID & uid, vaXMLSerializer & serializer, const wstring & assetFolder )
{
    shared_ptr<vaRenderMesh> newResource = pack.GetRenderDevice().GetMeshManager().CreateRenderMesh( uid, false );

    if( newResource == nullptr )
        return nullptr;

    if( newResource->SerializeUnpacked( serializer, assetFolder ) )
    {
        return new vaAssetRenderMesh( pack, newResource, name );
    }
    else
    {
        return nullptr;
    }
}

vaAssetRenderMaterial * vaAssetRenderMaterial::CreateAndLoadUnpacked( vaAssetPack & pack, const string & name, const vaGUID & uid, vaXMLSerializer & serializer, const wstring & assetFolder )
{
    shared_ptr<vaRenderMaterial> newResource = pack.GetRenderDevice().GetMaterialManager().CreateRenderMaterial( uid, false );

    if( newResource == nullptr )
        return nullptr;

    vaAssetRenderMaterial * retVal = nullptr;
    if( newResource->SerializeUnpacked( serializer, assetFolder ) )
    {
        retVal = new vaAssetRenderMaterial( pack, newResource, name );

    }
    return retVal;
}

vaAssetTexture * vaAssetTexture::CreateAndLoadUnpacked( vaAssetPack & pack, const string & name, const vaGUID & uid, vaXMLSerializer & serializer, const wstring & assetFolder )
{
    shared_ptr<vaTexture> newResource = VA_RENDERING_MODULE_CREATE_SHARED( vaTexture, vaTextureConstructorParams( pack.GetRenderDevice(), uid ) );

    if( newResource == nullptr )
        return nullptr;

    if( newResource->SerializeUnpacked( serializer, assetFolder ) )
    {
        return new vaAssetTexture( pack, newResource, name );
    }
    else
    {
        return nullptr;
    }
}

void vaAssetTexture::ReplaceTexture( const shared_ptr<vaTexture> & newTexture ) 
{ 
    ReplaceAssetResource( newTexture ); 
}

void vaAssetRenderMesh::ReplaceRenderMesh( const shared_ptr<vaRenderMesh> & newRenderMesh ) 
{ 
    ReplaceAssetResource( newRenderMesh );
}

void vaAssetRenderMaterial::ReplaceRenderMaterial( const shared_ptr<vaRenderMaterial> & newRenderMaterial ) 
{ 
    ReplaceAssetResource( newRenderMaterial ); 
}

void vaAsset::UIPropertiesItemTick( vaApplicationBase & application )
{
    application;
#ifdef VA_IMGUI_INTEGRATION_ENABLED
    ImGui::PushID( UIPropertiesItemGetDisplayName( ).c_str( ) );

    ImGui::ButtonEx( "Highlight", {0,0}, ImGuiButtonFlags_Disabled );
    if( m_resource != nullptr && ImGui::IsItemHovered( ) )
        m_resource->SetUIShowSelectedFrameIndex( application.GetRenderDevice().GetCurrentFrameIndex()+1 );
    ImGui::SameLine();

    if( ImGui::Button( "Rename" ) )
        ImGuiEx_PopupInputStringBegin( "Rename asset", Name() );

    string newName;
    if( ImGuiEx_PopupInputStringTick( newName ) )
    {
        if( Rename( newName ) )
        {
            VA_LOG( "Asset name changed to '%s'", Name().c_str() );
        }
        else
        {
            VA_LOG_ERROR( "Unable to rename asset to '%s'", newName.c_str() );
        }
    }

    ImGui::SameLine();
    if( ImGui::ButtonEx( "Delete", {0,0} ) )
    {
        m_parentPack.Remove( this, true );
        ImGui::PopID( );
        return;
    }

    ImGui::SameLine();
    ImGui::ButtonEx( "Duplicate", {0,0}, ImGuiButtonFlags_Disabled );

    ImGui::SameLine();
    ImGui::ButtonEx( "Move", {0,0}, ImGuiButtonFlags_Disabled );


    assert( m_resource != nullptr );
    if( m_resource != nullptr )
        m_resource->UIPropertiesDraw( application );

    ImGui::PopID( );
#endif
}


#pragma warning ( suppress: 4505 ) // unreferenced local function has been removed
void vaAssetPack::SingleTextureImport( string _filePath, string assetName, vaTextureLoadFlags textureLoadFlags, vaTextureContentsType textureContentsType, bool generateMIPs, shared_ptr<string> & importedInfo )
{
    wstring filePath = vaStringTools::SimpleWiden( _filePath );

    wstring outDir, outName, outExt;
    vaFileTools::SplitPath( filePath, &outDir, &outName, &outExt );

    if( !vaFileTools::FileExists( filePath ) )
    {
        ( *importedInfo ) += vaStringTools::Format( "Unable to find texture file '%s'\n", _filePath.c_str( ) );
        VA_LOG( L"Unable to find texture file '%s'", filePath.c_str( ) );
        return;
    }

    importedInfo = std::make_shared<string>("");
    std::weak_ptr<string> importedInfoWeak = importedInfo;
    std::weak_ptr<vaAssetPack> assetPackWeak = GetSharedPtr();
    
    shared_ptr<vaAssetTexture> textureAssetOut;

    VA_LOG( L"Importing texture asset from '%s'.", filePath.c_str( ) );

    GetRenderDevice().AsyncInvokeAtBeginFrame( [importedInfoWeak, assetPackWeak, filePath, assetName, textureLoadFlags, textureContentsType, generateMIPs] ( vaRenderDevice & renderDevice, float )
    {
        auto importedInfo   = importedInfoWeak.lock();
        auto assetPack      = assetPackWeak.lock();
        if( importedInfo == nullptr || assetPack == nullptr )
        {
            assert( false ); // shouldn't happen
            return false;
        }
        string filePathA = vaStringTools::SimpleNarrow( filePath );

        shared_ptr<vaTexture> textureOut = vaTexture::CreateFromImageFile( renderDevice, filePath, textureLoadFlags, vaResourceBindSupportFlags::ShaderResource, textureContentsType );

        if( textureOut == nullptr )
        {
            ( *importedInfo ) += vaStringTools::Format( "Error while loading '%s'\n", filePathA.c_str( ) );
            VA_LOG( L"vaAssetPack::SingleTextureImport - Error while loading '%s'", filePath.c_str( ) );
            return false;
        }

        // this is valid because all of this happens after BeginFrame was called on the device but before main application/sample starts rendering anything
        vaRenderDeviceContext& renderContext = *renderDevice.GetMainContext( );

        if( textureContentsType == vaTextureContentsType::SingleChannelLinearMask && vaResourceFormatHelpers::GetChannelCount( textureOut->GetResourceFormat( ) ) > 1 )
        {
            vaResourceFormat outFormat = vaResourceFormat::Unknown;
            switch( textureOut->GetResourceFormat( ) )
            {
            case( vaResourceFormat::R8G8B8A8_UNORM ):
            case( vaResourceFormat::B8G8R8A8_UNORM ):
                outFormat = vaResourceFormat::R8_UNORM; break;
            }

            shared_ptr<vaTexture> singleChannelTextureOut = ( outFormat == vaResourceFormat::Unknown ) ? ( nullptr ) : ( vaTexture::Create2D( renderDevice, outFormat, textureOut->GetWidth( ), textureOut->GetHeight( ), 1, 1, 1,
                vaResourceBindSupportFlags::ShaderResource | vaResourceBindSupportFlags::RenderTarget, vaResourceAccessFlags::Default, outFormat, vaResourceFormat::Automatic, vaResourceFormat::Automatic, vaResourceFormat::Automatic,
                textureOut->GetFlags( ), textureOut->GetContentsType( ) ) );

            if( singleChannelTextureOut != nullptr && renderDevice.GetPostProcess( ).MergeTextures( renderContext, singleChannelTextureOut, textureOut, nullptr, nullptr, "float4( srcA.x, 0, 0, 0 )" ) == vaDrawResultFlags::None )
            {
                ( *importedInfo ) += "Successfully removed unnecessary color channels\n";
                VA_LOG( L"vaAssetPack::SingleTextureImport - Successfully removed unnecessary color channels for '%s' texture", filePath.c_str( ) );
                textureOut = singleChannelTextureOut;
            }
        }

        if( generateMIPs )
        {
            if( textureOut->GetMipLevels() > 1 )
            {
                string info = vaStringTools::Format("Loaded texture already has %d MIP levels", textureOut->GetMipLevels() );
                ( *importedInfo ) += info + "\n";
                VA_LOG( "vaAssetPack::SingleTextureImport - %s", info.c_str() );
            }
            else
            {
                auto mipmappedTexture = vaTexture::TryCreateMIPs( renderContext, textureOut );
                if( mipmappedTexture != nullptr )
                {
                    ( *importedInfo ) += "Successfully created MIPs\n";
                    VA_LOG( L"vaAssetPack::SingleTextureImport - Successfully created MIPs for '%s' texture", filePath.c_str( ) );
                    textureOut = mipmappedTexture;
                }
                else
                {
                    ( *importedInfo ) += "Error while creating MIPs\n";
                    VA_LOG( L"vaAssetPack::SingleTextureImport - Error while creating MIPs for '%s'", filePath.c_str( ) );
                }
            }
        }

        if( textureOut == nullptr )
        {
            ( *importedInfo ) += vaStringTools::Format( "Error while loading '%s'\n", filePathA.c_str( ) );
            VA_LOG( L"vaAssetPack::SingleTextureImport - Error while loading '%s'", filePath.c_str( ) );
            return false;
        }

        assert( vaThreading::IsMainThread( ) ); // remember to lock asset global mutex and switch these to 'false'
        auto newAsset = assetPack->Add( textureOut, assetPack->FindSuitableAssetName( assetName, true ), true );

        (*importedInfo) += vaStringTools::Format("Texture '%s' loaded ok.\n", filePath.c_str( ) );
        VA_LOG_SUCCESS( L"vaAssetPack::SingleTextureImport - Texture '%s' loaded ok.", filePath.c_str( ) );

        return true;
    } );

}

const char* GetDNDAssetTypeName( vaAssetType assetType )
{
    const char* payloadTypeName;
    switch( assetType )
    {
    case vaAssetType::Texture:          payloadTypeName = "DND_ASSET_TEXTURE";          break;
    case vaAssetType::RenderMesh:       payloadTypeName = "DND_ASSET_RENDERMESH";       break;
    case vaAssetType::RenderMaterial:   payloadTypeName = "DND_ASSET_RENDERMATERIAL";   break;
    case vaAssetType::MaxVal:           payloadTypeName = "DND_ASSET_ALL";              break;
    default: payloadTypeName = "DND_ASSET_ERROR"; assert( false );
    }
    return payloadTypeName;
}

void vaAssetPack::UIPanelTick( vaApplicationBase & )
{ 
    assert(vaThreading::IsMainThread()); 

#ifdef VA_IMGUI_INTEGRATION_ENABLED
    bool disableEdit = m_name == "default";
    
    // // at the moment just don't show default asset pack - in fact, maybe remove it completely
    // if( disableEdit )
    //     return;

    ImGui::PushItemWidth( 200.0f );

    ImGui::PushID( this );

    if( m_ioTask != nullptr && !vaBackgroundTaskManager::GetInstance( ).IsFinished( m_ioTask ) )
    {
        vaBackgroundTaskManager::GetInstance( ).ImGuiTaskProgress( m_ioTask );
        disableEdit = true;
    }

    if( !disableEdit )
    {
        std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex );

        // rename UI
        if( ImGui::Button( "Rename" ) )
            ImGuiEx_PopupInputStringBegin( "Rename asset pack", GetName() );

        string newName;
        if( ImGuiEx_PopupInputStringTick( newName ) )
        {
            if( m_assetPackManager.FindLoadedPack( newName ) != nullptr )
            {
                VA_LOG_WARNING( "Cannot change name from '%s' to '%s' as the new name is already in use", m_name.c_str(), newName.c_str() );
            }
            else if( vaStringTools::ToLower( newName ) == vaStringTools::ToLower( "Importer_AssetPack" ) )
            {
                VA_LOG_WARNING( "Cannot change name from '%s' to '%s' as the new name is reserved", m_name.c_str(), newName.c_str() );
            }
            else
            {
                m_name = newName;
                VA_LOG( "Asset pack name changed to '%s'", m_name.c_str() );
            }
        }

        ImGui::Separator();

        switch( m_storageMode )
        {
        case vaAssetPack::StorageMode::Unknown:             ImGui::Text( "Current storage mode: UNKNOWN" );     break;
        case vaAssetPack::StorageMode::Unpacked:            ImGui::Text( "Current storage mode: Unpacked" );    break;
        case vaAssetPack::StorageMode::APACK:               ImGui::Text( "Current storage mode: APACK" );       break;
        default: assert( false );  break;
        }

        ImGui::Separator();

        wstring packedStoragePath = m_assetPackManager.GetAssetFolderPath( ) + vaStringTools::SimpleWiden( m_name ) + L".apack";

        ImGui::Text( ".apack storage:" );
        ImGui::Text( vaStringTools::SimpleNarrow(packedStoragePath).c_str() );
        //ImGui::SameLine( ImGui::GetContentRegionAvailWidth() * 3 / 4 );
        ImGui::PushID( "APACK" );
        if( ImGui::Button( " Load " ) )
        {
            //wstring fileName = vaFileTools::OpenFileDialog( L"", vaCore::GetExecutableDirectory(), L".apack files\0*.apack\0\0" );
            vaFileStream fileIn;
            if( vaFileTools::FileExists( packedStoragePath ) )
            {
                if( !LoadAPACK( packedStoragePath, true, false ) )
                {
                    VA_WARN( L"LoadAPACK('%s') failed", packedStoragePath.c_str() );
                }
                else
                {
                    ImGui::PopID();
                    return;
                }

            }
            else
            {
                VA_WARN( L"File '%s' does not exist or unable to open for reading", packedStoragePath.c_str() );
            }
        }
        ImGui::SameLine( );
        if( ImGui::Button( " Save " ) )
        {
            if( !SaveAPACK( packedStoragePath, false ) )
            {
                VA_WARN( L"Unable to open file '%s' for writing", packedStoragePath.c_str() );
            }
        }
        ImGui::PopID( );

        ImGui::Separator();

        wstring unpackedStoragePath = m_assetPackManager.GetAssetFolderPath( ) + vaStringTools::SimpleWiden( m_name ) + L"\\";
        ImGui::Text( "Unpacked storage: " );
        ImGui::Text( vaStringTools::SimpleNarrow(unpackedStoragePath).c_str() );
        //ImGui::SameLine( ImGui::GetContentRegionAvailWidth() * 3 / 4 );
        ImGui::PushID( "UNPACKED" );
        if( ImGui::Button( " Load " ) )
        {
            //wstring storageDirectory = vaFileTools::SelectFolderDialog( vaCore::GetExecutableDirectory() );

            // error messages handled internally
            LoadUnpacked( unpackedStoragePath, false );
        }
        ImGui::SameLine( );
        if( ImGui::Button( " Save " ) )
        {
            //wstring storageDirectory = vaFileTools::SelectFolderDialog( vaCore::GetExecutableDirectory() );
            //if( unpackedStoragePath != L"" )

            // error messages handled internally
            SaveUnpacked( unpackedStoragePath, false );
        }
        ImGui::PopID( );

        ImGui::Separator( );

        const float indentSize = ImGui::GetFontSize( ) / 2;

        if( ImGui::CollapsingHeader( "Tools", ImGuiTreeNodeFlags_Framed /*| ImGuiTreeNodeFlags_DefaultOpen*/ ) )
        {
            ImGui::Indent( indentSize );
            if( ImGui::Button( "Compress uncompressed textures" ) )
            {
                vector< shared_ptr<vaAsset> > meshes, materials, textures;
                for( auto asset : m_assetList )
                {
                    if( asset->Type == vaAssetType::Texture )
                    {
                        shared_ptr<vaAssetTexture> assetTexture = std::dynamic_pointer_cast<vaAssetTexture>(asset);

                        {
                            vaSimpleScopeTimerLog timer( "Compressing texture '"+asset->Name()+"'" );
                            shared_ptr<vaTexture> texture = assetTexture->GetTexture();

                            shared_ptr<vaTexture> compressedTexture = shared_ptr<vaTexture>( texture->TryCompress() );
                            if( compressedTexture != nullptr )
                            {
                                VA_LOG( "Conversion successful, replacing vaAssetTexture's old resource with the newly compressed." );
                                assetTexture->ReplaceTexture( compressedTexture );
                            }
                            else
                            {
                                VA_LOG( "Conversion skipped or failed." );
                            }
                        }
                        VA_LOG( "" );
                    }
                }
            }

// not maintained, needs update
#ifdef VA_ENABLE_TEXTURE_REDUCTION_TOOL
        if( vaTextureReductionTestTool::GetSupportedByApp() )
        {
            if( ImGui::Button( "Test textures for resolution reduction impact tool" ) )
            {
                if( vaTextureReductionTestTool::GetInstancePtr( ) == nullptr )
                {
                    vector<vaTextureReductionTestTool::TestItemType> paramTextures;
                    vector<shared_ptr<vaAssetTexture>> paramTextureAssets;
                    for( auto asset : m_assetList )
                        if( asset->Type == vaAssetType::Texture )
                        {
                            paramTextures.push_back( make_pair( std::dynamic_pointer_cast<vaAssetTexture>( asset )->GetTexture(), asset->Name() ) );
                            paramTextureAssets.push_back( std::dynamic_pointer_cast<vaAssetTexture>( asset ) );
                        }

                    new vaTextureReductionTestTool( paramTextures, paramTextureAssets );
                }
            }
        }
#endif
            // remove all assets UI
            {
                const char * deleteAssetPackPopup = "Remove all assets";
                if( ImGui::Button( "Remove all assets" ) )
                {
                    ImGui::OpenPopup( deleteAssetPackPopup );
                }

                if( ImGui::BeginPopupModal( deleteAssetPackPopup ) )
                {
                    ImGui::Text( "Are you sure that you want to remove all assets?" ); 
                    if( ImGui::Button( "Yes" ) )
                    {
                        RemoveAll(false);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if( ImGui::Button( "Cancel" ) )
                    {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
            }

            if( ImGui::CollapsingHeader( "Single texture import", ImGuiTreeNodeFlags_Framed /*| ImGuiTreeNodeFlags_DefaultOpen*/ ) )
            {
                ImGui::Indent( indentSize );

                if( m_ui_teximport_lastImportedInfo != nullptr )
                {
                    ImGui::Text( "Texture imported, log:" );
                    ImGui::BeginChild( "Child1", ImVec2( -1, ImGui::GetTextLineHeight( ) * 8 ), true, ImGuiWindowFlags_HorizontalScrollbar );
                    ImGui::Text( m_ui_teximport_lastImportedInfo->c_str( ) );
                    ImGui::SetScrollHereY( 1.0f );
                    ImGui::EndChild( );
                    if( ImGui::Button( "Close import log", ImVec2( -1.0f, 0.0f ) ) )
                        m_ui_teximport_lastImportedInfo = nullptr;
                }
                else
                {
                    ImGui::InputText( "Asset name", &m_ui_teximport_assetName );

                    // int srgbHandling = 0;
                    // m_ui_teximport_textureContentsType
                    // if( m_ui_teximport_textureLoadFlags & vaTextureLoadFlags::PresumeDataIsSRGB )
                
                    std::vector<string> contentsTypes; for( int i = 0; i < (int)vaTextureContentsType::MaxValue; i++ ) contentsTypes.push_back( vaTextureContentsTypeToUIName( (vaTextureContentsType)i ) );
                    ImGuiEx_Combo( "Contents type", (int32&)m_ui_teximport_textureContentsType, contentsTypes );

                    ImGui::InputText( "Input file", &m_ui_teximport_textureFilePath ); //, ImGuiInputTextFlags_AutoSelectAll );
                    ImGui::SameLine( );
                    if( ImGui::Button( "..." ) )
                    {
                        wstring fileName = vaFileTools::OpenFileDialog( vaStringTools::SimpleWiden( m_ui_teximport_textureFilePath ), vaCore::GetExecutableDirectory( ) );
                        if( fileName != L"" )
                            m_ui_teximport_textureFilePath = vaStringTools::SimpleNarrow( fileName );
                    }

                    string suitableName = (m_ui_teximport_assetName=="")?(""):(FindSuitableAssetName( m_ui_teximport_assetName, false ));

                    m_ui_teximport_textureLoadFlags = vaTextureLoadFlags::Default;
                    switch( m_ui_teximport_textureContentsType )
                    {
                    case vaTextureContentsType::GenericColor:               m_ui_teximport_textureLoadFlags = vaTextureLoadFlags::PresumeDataIsSRGB;    break;
                    case vaTextureContentsType::GenericLinear:              m_ui_teximport_textureLoadFlags = vaTextureLoadFlags::PresumeDataIsLinear;  break;
                    case vaTextureContentsType::NormalsXYZ_UNORM:           m_ui_teximport_textureLoadFlags = vaTextureLoadFlags::PresumeDataIsLinear;  break;
                    case vaTextureContentsType::NormalsXY_UNORM:            m_ui_teximport_textureLoadFlags = vaTextureLoadFlags::PresumeDataIsLinear;  break;
                    case vaTextureContentsType::NormalsWY_UNORM:            m_ui_teximport_textureLoadFlags = vaTextureLoadFlags::PresumeDataIsLinear;  break;
                    case vaTextureContentsType::SingleChannelLinearMask:    m_ui_teximport_textureLoadFlags = vaTextureLoadFlags::PresumeDataIsLinear;  break;
                    case vaTextureContentsType::DepthBuffer:                m_ui_teximport_textureLoadFlags = vaTextureLoadFlags::PresumeDataIsLinear;  break;
                    case vaTextureContentsType::LinearDepth:                m_ui_teximport_textureLoadFlags = vaTextureLoadFlags::PresumeDataIsLinear;  break;
                    case vaTextureContentsType::NormalsXY_LAEA_ENCODED:     m_ui_teximport_textureLoadFlags = vaTextureLoadFlags::PresumeDataIsLinear;  break;
                    case vaTextureContentsType::MaxValue:
                    default:
                        assert( false ); break;
                    }

                    ImGui::Checkbox( "Generate MIPs", &m_ui_teximport_generateMIPs );

                    if( !vaFileTools::FileExists( m_ui_teximport_textureFilePath ) )
                    {
                        ImGui::ButtonEx( "File not found", ImVec2( -1.0f, 0.0f ), ImGuiButtonFlags_Disabled );
                    } 
                    else if( m_ui_teximport_assetName == "" || suitableName != m_ui_teximport_assetName )
                    {
                        ImGui::ButtonEx( "Asset name unsuitable", ImVec2( -1.0f, 0.0f ), ImGuiButtonFlags_Disabled );
                    }
                    else
                    {
                        if( ImGui::Button( "Import texture!", ImVec2( -1.0f, 0.0f ) ) )
                        {
                            SingleTextureImport( m_ui_teximport_textureFilePath, m_ui_teximport_assetName, m_ui_teximport_textureLoadFlags, m_ui_teximport_textureContentsType, m_ui_teximport_generateMIPs, m_ui_teximport_lastImportedInfo );
                        }
                    }
                }
                
                ImGui::Unindent( indentSize );
            }

            ImGui::Unindent( indentSize );
        }
    } // if( !disableEdit )

    vector< shared_ptr<vaAsset> > meshes, materials, textures;
    for( auto asset : m_assetList )
    {
        if( asset->Type == vaAssetType::RenderMesh )
            meshes.push_back(asset);
        else if( asset->Type == vaAssetType::RenderMaterial )
            materials.push_back(asset);
        else if( asset->Type == vaAssetType::Texture )
            textures.push_back(asset);
    }

    char buffer[256]; strncpy_s( buffer, sizeof(buffer), m_uiNameFilter.c_str(), m_uiNameFilter.length() );
    ImGui::InputText("filter", buffer, sizeof(buffer), ImGuiInputTextFlags_AutoSelectAll );
    m_uiNameFilter = buffer;
    m_uiNameFilter = vaStringTools::ToLower( m_uiNameFilter );

    ImGui::Checkbox( "meshes", &m_uiShowMeshes ); 
    ImGui::SameLine();
    ImGui::Checkbox( "materials", &m_uiShowMaterials ); 
    ImGui::SameLine();
    ImGui::Checkbox( "textures", &m_uiShowTextures ); 

    vector< shared_ptr<vaAsset> > filteredList;
    for( auto asset : m_assetList )
    {
        if( asset->Type == vaAssetType::RenderMesh && !m_uiShowMeshes )
            continue;
        if( asset->Type == vaAssetType::RenderMaterial && !m_uiShowMaterials )
            continue;
        if( asset->Type == vaAssetType::Texture && !m_uiShowTextures )
            continue;
        assert( asset->Name() == vaStringTools::ToLower(asset->Name()) );   // must be lowercase!
        if( m_uiNameFilter.length() > 0 && asset->Name().find( m_uiNameFilter ) == string::npos )
            continue;
        filteredList.push_back(asset);
    }

    ImGui::SameLine();
    ImGui::Text("(total: %d)", (int)filteredList.size() );

    // sort by name
    std::sort( filteredList.begin(), filteredList.end(), [](const shared_ptr<vaAsset> & a, const shared_ptr<vaAsset> & b) -> bool { return a->Name() < b->Name(); } );

    if( ImGui::BeginChild( "assetlist", {0,0}, true ) )
    {
        for( auto asset : filteredList )
        {
            if( ImGui::Selectable( asset->Name( ).c_str( ), false, ImGuiSelectableFlags_AllowDoubleClick ) )
            {
                if( ImGui::IsMouseDoubleClicked( 0 ) )
                    vaUIManager::GetInstance().SelectPropertyItem( asset );
            }

            if( ImGui::BeginDragDropSource( ImGuiDragDropFlags_None ) )
            {
                vaGUID uid = asset->GetResourceObjectUID( );
                ImGui::SetDragDropPayload( GetDNDAssetTypeName( asset->Type ), &uid, sizeof( uid ) );        // Set payload to carry the index of our item (could be anything)
                ImGui::EndDragDropSource( );
            }

            if( asset->GetResource( ) != nullptr && ImGui::IsItemHovered( ) )
                asset->GetResource( )->SetUIShowSelectedFrameIndex( GetRenderDevice().GetCurrentFrameIndex()+1 );
        }
    }
    ImGui::EndChild();

    /*
    ImGuiTabBarFlags tabBarFlags = 0;
    tabBarFlags |= ImGuiTabBarFlags_Reorderable;
    //tabBarFlags |= ImGuiTabBarFlags_AutoSelectNewTabs;
    tabBarFlags |= ImGuiTabBarFlags_NoCloseWithMiddleMouseButton;
    tabBarFlags |= ImGuiTabBarFlags_FittingPolicyScroll;    // ImGuiTabBarFlags_FittingPolicyResizeDown
    if (ImGui::BeginTabBar("MyTabBar", tabBarFlags))
    {
        if( ImGui::BeginTabItem( vaStringTools::Format("Meshes (%d)###Meshes", (int)meshes.size() ).c_str( ) ) ) //ImGuiWindowFlags_NoBackground ) )
        {
            for( auto asset : meshes )
            {
                //bool nodeOpen = ImGui::TreeNode( asset->Name().c_str() );
                if( ImGui::Selectable( asset->Name( ).c_str( ), false, ImGuiSelectableFlags_AllowDoubleClick ) )
                {
                    if( ImGui::IsMouseDoubleClicked( 0 ) )
                        vaUIManager::GetInstance().SelectPropertyItem( asset );
                }
                if( asset->GetResource( ) != nullptr )
                    asset->GetResource( )->SetUIShowSelected( ImGui::IsItemHovered( ) );
                // if( nodeOpen )
                // {
                //     if( !disableEdit )
                //         asset->UIPropertiesItemTick();
                //     ImGui::TreePop();
                // }
            }
            ImGui::EndTabItem();
        }
        if( ImGui::BeginTabItem( vaStringTools::Format("Materials (%d)###Materials", (int)materials.size() ).c_str( ) ) ) //ImGuiWindowFlags_NoBackground ) )
        {
            for( auto asset : materials )
            {
                bool nodeOpen = ImGui::TreeNode( asset->Name( ).c_str( ) );
                if( asset->GetResource( ) != nullptr )
                    asset->GetResource( )->SetUIShowSelected( ImGui::IsItemHovered( ) );
                if( nodeOpen )
                {
                    if( !disableEdit )
                        asset->UIPropertiesItemTick();
                    ImGui::TreePop();
                }
            }
            ImGui::EndTabItem();
        }
        if( ImGui::BeginTabItem( vaStringTools::Format("Textures (%d)###Textures", (int)textures.size() ).c_str( ) ) ) //ImGuiWindowFlags_NoBackground ) )
        {
            for( auto asset : textures )
            {
                bool nodeOpen = ImGui::TreeNode( asset->Name( ).c_str( ) );
                if( asset->GetResource( ) != nullptr )
                    asset->GetResource( )->SetUIShowSelected( ImGui::IsItemHovered( ) );
                if( nodeOpen )
                {
                    if( !disableEdit )
                        asset->UIPropertiesItemTick();
                    ImGui::TreePop();
                    if( asset->GetResource( ) != nullptr )
                        asset->GetResource( )->SetUIShowSelected( ImGui::IsItemHovered( ) );
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    */

    ImGui::PopID( );
    ImGui::PopItemWidth( );
#endif
}

vaRenderDevice & vaAssetPack::GetRenderDevice( )
{ 
    return m_assetPackManager.GetRenderDevice(); 
}

vaAssetPackManager::vaAssetPackManager( vaRenderDevice & renderDevice ) : m_renderDevice( renderDevice )//, vaUIPanel( "Assets" )
{
    assert( vaThreading::IsMainThread() );

#if 0 // let's not use "default" for now!
    shared_ptr<vaAssetPack> defaultPack = CreatePack( "default" );
    m_defaultPack = defaultPack;

    // some default assets - commented out slower ones to reduce startup times <-> this should be done in a better way anyway, same as (async) loading except no file access
    {
        shared_ptr<vaRenderMesh> planeMesh        = vaRenderMesh::CreatePlane( m_renderDevice, vaMatrix4x4::Identity, 1.0f, 1.0f, false                       , vaCore::GUIDFromStringA( "706f42bc-df35-4dae-b706-688c451bd181" ) );
    //    shared_ptr<vaRenderMesh> tetrahedronMesh  = vaRenderMesh::CreateTetrahedron( m_renderDevice, vaMatrix4x4::Identity, false                           , vaCore::GUIDFromStringA( "45fa748c-982b-406b-a0d0-e0c49772ded7" ) );
        shared_ptr<vaRenderMesh> cubeMesh         = vaRenderMesh::CreateCube( m_renderDevice, vaMatrix4x4::Identity, false, 0.7071067811865475f             , vaCore::GUIDFromStringA( "11ec29ea-1524-4720-9109-14aa894750b0" ) );
    //    shared_ptr<vaRenderMesh> octahedronMesh   = vaRenderMesh::CreateOctahedron( m_renderDevice, vaMatrix4x4::Identity, false                            , vaCore::GUIDFromStringA( "6d813dea-1a19-426e-97af-c3edd94d4a99" ) );
    //    shared_ptr<vaRenderMesh> icosahedronMesh  = vaRenderMesh::CreateIcosahedron( m_renderDevice, vaMatrix4x4::Identity, false                           , vaCore::GUIDFromStringA( "d2b41829-9d06-429e-bddb-46579806b77a" ) );
    //    shared_ptr<vaRenderMesh> dodecahedronMesh = vaRenderMesh::CreateDodecahedron( m_renderDevice, vaMatrix4x4::Identity, false                          , vaCore::GUIDFromStringA( "cbf5502b-97e0-49c5-8ec7-d6afa41314b8" ) );
        shared_ptr<vaRenderMesh> sphereMesh       = vaRenderMesh::CreateSphere( m_renderDevice, vaMatrix4x4::Identity, 2, true                              , vaCore::GUIDFromStringA( "089282d6-9640-4cd8-b0f5-22d026ffcdb6" ) );
        shared_ptr<vaRenderMesh> cylinderMesh     = vaRenderMesh::CreateCylinder( m_renderDevice, vaMatrix4x4::Identity, 1.0, 1.0, 1.0, 12, false, false    , vaCore::GUIDFromStringA( "aaea48bd-e48c-4d15-8c98-c5be3e0b6ef0" ) );
        //shared_ptr<vaRenderMesh> teapotMesh       = vaRenderMesh::CreateTeapot( m_renderDevice, vaMatrix4x4::Identity                                       , vaCore::GUIDFromStringA( "3bab100b-7336-429b-bb79-e4b1b25ba443" ) );
    
        defaultPack->Add( planeMesh           , "SystemMesh_Plane", true );
    //    defaultPack->Add( tetrahedronMesh     , "SystemMesh_Tetrahedron", true );
        defaultPack->Add( cubeMesh            , "SystemMesh_Cube", true );
    //    defaultPack->Add( octahedronMesh      , "SystemMesh_Octahedron", true );
    //    defaultPack->Add( icosahedronMesh     , "SystemMesh_Icosahedron", true );
    //    defaultPack->Add( dodecahedronMesh    , "SystemMesh_Dodecahedron", true );
        defaultPack->Add( sphereMesh          , "SystemMesh_Sphere", true );
        defaultPack->Add( cylinderMesh        , "SystemMesh_Cylinder", true );
        //defaultPack->Add( teapotMesh          , "SystemMesh_Teapot", true );
    }
#endif

    renderDevice.e_BeginFrame.AddWithToken( m_aliveToken, [ &hadAsyncOpLastFrame = m_hadAsyncOpLastFrame, thisPtr = this ]( float ) 
    { 
        hadAsyncOpLastFrame = std::max( 0, hadAsyncOpLastFrame - 1 );
        if( thisPtr->AnyAsyncOpExecuting( ) )
            hadAsyncOpLastFrame = 2;
    }
    );
}

vaAssetPackManager::~vaAssetPackManager( )      
{ 
    assert( vaThreading::IsMainThread() );

    UnloadAllPacks( );
}

shared_ptr<vaAssetPack> vaAssetPackManager::CreatePack( const string & _assetPackName )
{
    assert( vaThreading::IsMainThread() );
    string assetPackName = vaStringTools::ToLower( _assetPackName );

    if( FindLoadedPack( assetPackName ) != nullptr )
    {
        VA_LOG_WARNING( "vaAssetPackManager::CreatePack(%s) - pack with the same name already exists, can't create another", assetPackName.c_str() );
        return nullptr;
    }
    shared_ptr<vaAssetPack> newPack = shared_ptr<vaAssetPack>( new vaAssetPack( *this, assetPackName ) );
    m_assetPacks.push_back( newPack );
    return newPack;
}

void vaAssetPackManager::LoadPacks( const string & _nameOrWildcard, bool allowAsync )
{
    assert( vaThreading::IsMainThread() );
    string nameOrWildcard = vaStringTools::ToLower( _nameOrWildcard );

    wstring assetPackFolder = GetAssetFolderPath( );

    if( nameOrWildcard == "*" )
    {
        vaSimpleScopeTimerLog timerLog( vaStringTools::Format( L"Enumerating/loading all asset packs in '%s'", assetPackFolder.c_str( ) ) );

        // first load unpacked
        auto dirList = vaFileTools::FindDirectories( assetPackFolder );
        for( auto dir : dirList )
        {
            if( vaFileTools::FileExists( dir + L"\\AssetPack.xml" ) )
            {
                string name = vaStringTools::SimpleNarrow(dir);
                size_t lastSep = name.find_last_of( L'\\' );
                if( lastSep != wstring::npos )
                    name = name.substr( lastSep+1 );

                LoadPacks( name, allowAsync );
            }
        }

        // then load packed
        auto fileList = vaFileTools::FindFiles( assetPackFolder, L"*.apack", false );
        for( auto fileName : fileList )
        {
            wstring justName, justExt;
            vaFileTools::SplitPath( fileName, nullptr, &justName, &justExt );
            justExt = vaStringTools::ToLower( justExt );
            assert( justExt == L".apack" );

            string name = vaStringTools::SimpleNarrow( justName );

            if( FindLoadedPack(name) == nullptr )
                LoadPacks( name, allowAsync );
        }
    }
    else
    {
        if( FindLoadedPack( nameOrWildcard ) != nullptr )
        {
            VA_LOG_WARNING( "vaAssetPackManager::LoadPacks(%s) - can't load, already loaded", nameOrWildcard.c_str() );
            return;
        }

        // first try unpacked...
        bool unpackedLoaded = false;
        wstring unpackDir = assetPackFolder + vaStringTools::SimpleWiden(nameOrWildcard) + L"\\";
        if( vaFileTools::FileExists( unpackDir + L"AssetPack.xml" ) )
        {
            unpackedLoaded = true;

            shared_ptr<vaAssetPack> newPack = CreatePack( nameOrWildcard );
            if( !newPack->LoadUnpacked( unpackDir, true ) )
            {
                VA_LOG_ERROR( L"vaAssetPackManager::LoadPacks - Error while loading asset pack from '%s'", unpackDir.c_str() );
                return;
            }
        }

        // ...then try packed
        wstring apackName = assetPackFolder + vaStringTools::SimpleWiden(nameOrWildcard) + L".apack";
        if( vaFileTools::FileExists( apackName ) )
        {
            if( unpackedLoaded )
            {
                VA_LOG_WARNING( "vaAssetPackManager::LoadPacks(%s) - pack with that name exists as .apack but already loaded as unpacked - you might want to remove one or the other to avoid confusion", nameOrWildcard.c_str() );
            }
            else
            {
                shared_ptr<vaAssetPack> newPack = CreatePack( nameOrWildcard );

                if( !newPack->LoadAPACK( apackName, allowAsync, true ) )
                {
                    VA_LOG_ERROR( "vaAssetPackManager::LoadPacks(%s) - error loading .apack file", nameOrWildcard.c_str() );
                }
            }
        }
        else
        {
            if( !unpackedLoaded )
            {
                VA_LOG_ERROR( "vaAssetPackManager::LoadPacks(%s) - unable to find asset with that name in the asset folder", nameOrWildcard.c_str() );
            }
        }
    }
}

shared_ptr<vaAssetPack> vaAssetPackManager::FindOrLoadPack( const string & _assetPackName, bool allowAsync )
{
    assert( vaThreading::IsMainThread() );
    string assetPackName = vaStringTools::ToLower( _assetPackName );

    shared_ptr<vaAssetPack> found = FindLoadedPack(assetPackName);
    if( found = nullptr )
    {
        LoadPacks( assetPackName, allowAsync );
        found = FindLoadedPack(assetPackName);
    }
    return found;
}

shared_ptr<vaAssetPack> vaAssetPackManager::FindLoadedPack( const string & _assetPackName )
{
    assert( vaThreading::IsMainThread() );
    string assetPackName = vaStringTools::ToLower( _assetPackName );

    for( int i = 0; i < m_assetPacks.size(); i++ )
        if( m_assetPacks[i]->GetName() == assetPackName )
            return m_assetPacks[i];
    return nullptr;
}

void vaAssetPackManager::UnloadPack( shared_ptr<vaAssetPack> & pack )
{
    assert( vaThreading::IsMainThread() );
    assert( pack != nullptr );
    for( int i = 0; i < m_assetPacks.size(); i++ )
    {
        if( m_assetPacks[i] == pack )
        {
            pack.reset();
            // check if anyone else is holding a smart pointer to this pack - in case they are, they need to .reset() it before we get here
            assert( m_assetPacks[i].use_count() == 1 ); // this assert is not entirely correct for multithreaded scenarios so beware
            if( i != (m_assetPacks.size()-1) )
                m_assetPacks[m_assetPacks.size()-1] = m_assetPacks[i];
            m_assetPacks.pop_back();
            return;
        }
    }
}
void vaAssetPackManager::UnloadAllPacks( )
{
    assert( vaThreading::IsMainThread() );
    m_defaultPack.reset();
    for( int i = 0; i < m_assetPacks.size(); i++ )
    {
        // check if anyone else is holding a smart pointer to this pack - in case they are, they need to .reset() it before we get here
        assert( m_assetPacks[i].use_count() == 1 ); // this assert is not entirely correct for multithreaded scenarios so beware
        m_assetPacks[i] = nullptr;
    }
    m_assetPacks.clear();
}

inline vector<shared_ptr<vaAsset>> vaAssetPack::Find( std::function<bool( vaAsset& )> filter, bool lockMutex )
{
    std::unique_lock<mutex> assetStorageMutexLock( m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock( ); else m_assetStorageMutex.assert_locked_by_caller( );

    vector<shared_ptr<vaAsset>> assets;

    for( auto it : m_assetMap )
    {
        if( filter( *( it.second ) ) )
            assets.push_back( it.second );
    }
    return assets;
}

shared_ptr<vaAsset> vaAssetPackManager::FindAsset( const string & name )
{
    assert( vaThreading::IsMainThread() );

    for( int i = 0; i < m_assetPacks.size(); i++ )
    {
        shared_ptr<vaAsset> retVal = m_assetPacks[i]->Find( name );
        if( retVal != nullptr )
            return retVal;
    }
    return nullptr;
}

vector<shared_ptr<vaAsset>> vaAssetPackManager::FindAssets( std::function<bool( vaAsset& )> filter )
{
    vector<shared_ptr<vaAsset>> assets;

    for( int i = 0; i < m_assetPacks.size( ); i++ )
    {
        auto collected = m_assetPacks[i]->Find( filter );
        assets.insert( assets.end(), collected.begin(), collected.end() );
    }

    return assets;
}

bool vaAssetPackManager::AnyAsyncOpExecuting( )
{
    assert( vaThreading::IsMainThread() );
    for( int i = 0; i < m_assetPacks.size(); i++ )
        if( m_assetPacks[i]->IsBackgroundTaskActive() )
            return true;
    return false;
}

void vaAssetPackManager::OnRenderingAPIAboutToShutdown( )
{
    UnloadAllPacks();
}

shared_ptr<vaAssetResource> vaAssetPackManager::UIAssetDragAndDropTarget( vaAssetType assetType, const char * label, const vaVector2 & size )
{
    shared_ptr<vaAssetResource> retVal = nullptr;

    ImGui::Button( label, ImVec2(size.x, size.y) );

    if( ImGui::BeginDragDropTarget( ) )
    {
        const char * payloadType = GetDNDAssetTypeName( assetType );

        if( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( payloadType ) )
        {
            IM_ASSERT( payload->DataSize == sizeof( vaGUID ) );
            vaGUID payload_n = *(vaGUID*)payload->Data;
            retVal = vaUIDObjectRegistrar::GetInstance( ).Find<vaAssetResource>( payload_n );

            if( retVal != nullptr  )
            {
                assert( retVal->GetAssetType() == assetType );
            }
        }
        ImGui::EndDragDropTarget( );
    }
    return retVal;
}

// void vaAssetPackManager::UIPanelTick( )
// { 
//     assert( vaThreading::IsMainThread() );
// #ifdef VA_IMGUI_INTEGRATION_ENABLED
//     vector<string> list;
//     for( size_t i = 0; i < m_assetPacks.size(); i++ )
//         list.push_back( m_assetPacks[i]->GetName() );
//     ImGui::Text( "Asset packs in memory:" );
//     ImGui::PushItemWidth(-1);
//     ImGuiEx_ListBox( "###Asset packs in memory:", m_UIAssetPackIndex, list );
//     ImGui::PopItemWidth();
//     if( m_UIAssetPackIndex >= 0 && m_UIAssetPackIndex < m_assetPacks.size() )
//         m_assetPacks[m_UIAssetPackIndex]->UIDraw();
// #endif
// }
