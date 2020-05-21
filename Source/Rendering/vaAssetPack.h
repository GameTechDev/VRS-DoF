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
#include "Core/vaUI.h"

#include "vaRendering.h"

#include "vaTriangleMesh.h"
#include "vaTexture.h"

#include "Rendering/Shaders/vaSharedTypes.h"

// vaRenderMesh and vaRenderMeshManager are a generic render mesh implementation

namespace Vanilla
{
    class vaTexture;
    class vaRenderMesh;
    class vaRenderMaterial;
    class vaAssetPack;
    class vaAssetPackManager;

    // this needs to be converted to a class, along with type names and other stuff (it started as a simple struct)
    struct vaAsset : public vaUIPropertiesItem
    {
    protected:
        friend class vaAssetPack;
        shared_ptr<vaAssetResource>                     m_resource;
        string                                          m_name;                     // warning, never change this except by using Rename
        vaAssetPack &                                   m_parentPack;
        int                                             m_parentPackStorageIndex;   // referring to vaAssetPack::m_assetList

    protected:
        vaAsset( vaAssetPack & pack, const vaAssetType type, const string & name, const shared_ptr<vaAssetResource> & resourceBasePtr ) : m_parentPack( pack ), Type( type ), m_name( name ), m_resource( resourceBasePtr ), m_parentPackStorageIndex( -1 )
                                                        { assert( m_resource != nullptr ); m_resource->SetParentAsset( this ); assert( m_resource->GetAssetType() == type ); }
        virtual ~vaAsset( )                             { assert( m_resource != nullptr ); m_resource->SetParentAsset( nullptr ); }

    public:
        void                                            ReplaceAssetResource( const shared_ptr<vaAssetResource> & newResourceBasePtr );

    public:
        const vaAssetType                               Type;
//        const wstring                                   StoragePath;

        vaAssetPack &                                   GetAssetPack( ) const                   { return m_parentPack; }

        virtual const vaGUID &                          GetResourceObjectUID( )                 { assert( m_resource != nullptr ); return m_resource->UIDObject_GetUID(); }
        
         shared_ptr<vaAssetResource> &                  GetResource( )                          { return m_resource; }

        template<typename ResourceType>
        shared_ptr<ResourceType>                        GetResource( ) const                    { return std::dynamic_pointer_cast<ResourceType>(m_resource); }

        const string &                                  Name( ) const                           { return m_name; }

        bool                                            Rename( const string & newName );
        //bool                                            Delete( );

        virtual bool                                    SaveAPACK( vaStream & outStream );
        virtual bool                                    SerializeUnpacked( vaXMLSerializer & serializer, const wstring & assetFolder );

        // this returns the shared pointer to this object kept by the parent asset pack
        shared_ptr<vaAsset>                             GetSharedPtr( ) const;

        //void                                            ReconnectDependencies( )                { m_resourceBasePtr->ReconnectDependencies(); }

        static string                                   GetTypeNameString( vaAssetType );

    public:
        virtual string                                  UIPropertiesItemGetDisplayName( ) const override { return vaStringTools::Format( "%s: %s", GetTypeNameString(Type).c_str(), m_name.c_str() ); }
        virtual void                                    UIPropertiesItemTick( vaApplicationBase & application ) override;
    };

    // TODO: all these should be removed, there's really no need to have type-specific vaAsset-s, everything specific should be handled by the vaAssetResource 
    struct vaAssetTexture : public vaAsset
    {
    private:
        friend class vaAssetPack;
        vaAssetTexture( vaAssetPack & pack, const shared_ptr<vaTexture> & texture, const string & name );
    
    public:
        shared_ptr<vaTexture>                           GetTexture( ) const                                                     { return std::dynamic_pointer_cast<vaTexture>(m_resource); }
        void                                            ReplaceTexture( const shared_ptr<vaTexture> & newTexture );

        static vaAssetTexture *                         CreateAndLoadAPACK( vaAssetPack & pack, const string & name, vaStream & inStream );
        static vaAssetTexture *                         CreateAndLoadUnpacked( vaAssetPack & pack, const string & name, const vaGUID & uid, vaXMLSerializer & serializer, const wstring & assetFolder );

        static shared_ptr<vaAssetTexture>               SafeCast( const shared_ptr<vaAsset> & asset ) { assert( asset->Type == vaAssetType::Texture ); return std::dynamic_pointer_cast<vaAssetTexture, vaAsset>( asset ); }
    };

    struct vaAssetRenderMesh : public vaAsset
    {
    private:
        friend class vaAssetPack;
        vaAssetRenderMesh( vaAssetPack & pack, const shared_ptr<vaRenderMesh> & mesh, const string & name );

    public:
        shared_ptr<vaRenderMesh>                        GetRenderMesh( ) const                                                     { return std::dynamic_pointer_cast<vaRenderMesh>(m_resource); }
        void                                            ReplaceRenderMesh( const shared_ptr<vaRenderMesh> & newRenderMesh );

        static vaAssetRenderMesh *                      CreateAndLoadAPACK( vaAssetPack & pack, const string & name, vaStream & inStream );
        static vaAssetRenderMesh *                      CreateAndLoadUnpacked( vaAssetPack & pack, const string & name, const vaGUID & uid, vaXMLSerializer & serializer, const wstring & assetFolder );

        static shared_ptr<vaAssetRenderMesh>            SafeCast( const shared_ptr<vaAsset> & asset ) { assert( asset->Type == vaAssetType::RenderMesh ); return std::dynamic_pointer_cast<vaAssetRenderMesh, vaAsset>( asset ); }
    };

    struct vaAssetRenderMaterial : public vaAsset
    {
    private:
        friend class vaAssetPack;
        vaAssetRenderMaterial( vaAssetPack & pack, const shared_ptr<vaRenderMaterial> & material, const string & name );

    public:
        shared_ptr<vaRenderMaterial>                    GetRenderMaterial( ) const { return std::dynamic_pointer_cast<vaRenderMaterial>( m_resource ); }
        void                                            ReplaceRenderMaterial( const shared_ptr<vaRenderMaterial> & newRenderMaterial );

        static vaAssetRenderMaterial *                  CreateAndLoadAPACK( vaAssetPack & pack, const string & name, vaStream & inStream );
        static vaAssetRenderMaterial *                  CreateAndLoadUnpacked( vaAssetPack & pack, const string & name, const vaGUID & uid, vaXMLSerializer & serializer, const wstring & assetFolder );

        static shared_ptr<vaAssetRenderMaterial>        SafeCast( const shared_ptr<vaAsset> & asset ) { assert( asset->Type == vaAssetType::RenderMaterial ); return std::dynamic_pointer_cast<vaAssetRenderMaterial, vaAsset>( asset ); }
    };

    class vaAssetPack : public vaUIPanel//, public std::enable_shared_from_this<vaAssetPack>
    {
        enum class StorageMode
        {
            Unknown,
            Unpacked,
            APACK,
            //APACKStreamable,
        };

    protected:
        string                                              m_name;                 // warning - not protected by the mutex and can only be accessed by the main thread
        std::map< string, shared_ptr<vaAsset> >             m_assetMap;
        vector< shared_ptr<vaAsset> >                       m_assetList;
        mutable mutex                                       m_assetStorageMutex;

        vaAssetPackManager &                                m_assetPackManager;

        // changes on every load/save
        StorageMode                                         m_storageMode           = StorageMode::Unknown;
        bool                                                m_dirty                 = false;
        vaFileStream                                        m_apackStorage;
        mutex                                               m_apackStorageMutex;

        shared_ptr<vaBackgroundTaskManager::Task>           m_ioTask;

        string                                              m_uiNameFilter          = "";
        bool                                                m_uiShowMeshes          = true;
        bool                                                m_uiShowMaterials       = true;
        bool                                                m_uiShowTextures        = true;

        string                                              m_ui_teximport_assetName            = "";
        string                                              m_ui_teximport_textureFilePath      = "";
        vaTextureLoadFlags                                  m_ui_teximport_textureLoadFlags     = vaTextureLoadFlags::Default;
        vaTextureContentsType                               m_ui_teximport_textureContentsType  = vaTextureContentsType::GenericColor;
        bool                                                m_ui_teximport_generateMIPs         = true;
        shared_ptr<string>                                  m_ui_teximport_lastImportedInfo;

    private:
        friend class vaAssetPackManager;
        explicit vaAssetPack( vaAssetPackManager & assetPackManager, const string & name );
        vaAssetPack( const vaAssetPack & copy ) = delete;
    
    public:
        virtual ~vaAssetPack( );

    public:
        mutex &                                             GetAssetStorageMutex( ) { return m_assetStorageMutex; }

        string                                              FindSuitableAssetName( const string & nameSuggestion, bool lockMutex );

        shared_ptr<vaAsset>                                 Find( const string & _name, bool lockMutex = true );
        vector<shared_ptr<vaAsset>>                         Find( std::function<bool( vaAsset& )> filter, bool lockMutex = true );

        //shared_ptr<vaAsset>                                 FindByStoragePath( const wstring & _storagePath );
        void                                                Remove( const shared_ptr<vaAsset> & asset, bool lockMutex );
        void                                                Remove( vaAsset * asset, bool lockMutex );
        void                                                RemoveAll( bool lockMutex );
        
        // save current contents
        bool                                                SaveAPACK( const wstring & fileName, bool lockMutex );
        // load contents (current contents are not deleted)
        bool                                                LoadAPACK( const wstring & fileName, bool async, bool lockMutex );

        // save current contents as XML & folder structure
        bool                                                SaveUnpacked( const wstring & folderRoot, bool lockMutex );
        // load contents as XML & folder structure (current contents are not deleted)
        bool                                                LoadUnpacked( const wstring & folderRoot, bool lockMutex );

        vaRenderDevice &                                    GetRenderDevice( );

    public:
        shared_ptr<vaAssetTexture>                          Add( const shared_ptr<vaTexture> & texture, const string & name, bool lockMutex );
        shared_ptr<vaAssetRenderMesh>                       Add( const shared_ptr<vaRenderMesh> & mesh, const string & name, bool lockMutex );
        shared_ptr<vaAssetRenderMaterial>                   Add( const shared_ptr<vaRenderMaterial> & material, const string & name, bool lockMutex );

        string                                              GetName( ) const                            { assert(vaThreading::IsMainThread()); return m_name; }

        bool                                                RenameAsset( vaAsset & asset, const string & newName, bool lockMutex );
        bool                                                DeleteAsset( vaAsset & asset, bool lockMutex );

        size_t                                              Count( bool lockMutex ) const               { std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock ); if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller(); assert( m_assetList.size() == m_assetMap.size() ); return m_assetList.size(); }

        shared_ptr<vaAsset>                                 AssetAt( size_t index, bool lockMutex )     { std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock ); if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller(); if( index >= m_assetList.size() ) return nullptr; else return m_assetList[index]; }

        const shared_ptr<vaBackgroundTaskManager::Task> &   GetCurrentIOTask( )                         { return m_ioTask; }
        void                                                WaitUntilIOTaskFinished( bool breakIfSafe = false );
        bool                                                IsBackgroundTaskActive( ) const;

        // this returns the shared pointer to this object kept by the parent asset manager
        shared_ptr<vaAssetPack>                             GetSharedPtr( ) const;


    protected:
        // these are leftovers - need to be removed 
        // virtual string                                      IHO_GetInstanceName( ) const                { return vaStringTools::Format("Asset Pack '%s'", Name().c_str() ); }
        //virtual void                                        IHO_Draw( );
        virtual string                                      UIPanelGetDisplayName( ) const override     { return m_name; }
        virtual void                                        UIPanelTick( vaApplicationBase & application ) override;

    private:
        void                                                InsertAndTrackMe( shared_ptr<vaAsset> newAsset, bool lockMutex );

        bool                                                LoadAPACKInner( vaStream & inStream, vector< shared_ptr<vaAsset> > & loadedAssets, vaBackgroundTaskManager::TaskContext & taskContext );

    protected:
        void                                                SingleTextureImport( string _filePath, string assetName, vaTextureLoadFlags textureLoadFlags, vaTextureContentsType textureContentsType, bool generateMIPs, shared_ptr<string> & outImportedInfo );
    };

    class vaAssetImporter;

    class vaAssetPackManager// : public vaUIPanel
    {
        // contains some standard meshes, etc.
        weak_ptr<vaAssetPack>                               m_defaultPack;

        vaRenderDevice &                                    m_renderDevice;

    protected:
        vector<shared_ptr<vaAssetPack>>                     m_assetPacks;           // tracks all vaAssetPacks that belong to this vaAssetPackManager
        int                                                 m_UIAssetPackIndex      = 0;

        int                                                 m_hadAsyncOpLastFrame   = 0;
         shared_ptr<int>                                    m_aliveToken      = std::make_shared<int>(42);

    public:
                                                            vaAssetPackManager( vaRenderDevice & renderDevice );
                                                            ~vaAssetPackManager( );

    public:
        shared_ptr<vaAssetPack>                             GetDefaultPack( )                                       { return m_defaultPack.lock(); }
        
        // wildcard '*' or name supported (name with wildcard not yet supported but feel free to add!)
        shared_ptr<vaAssetPack>                             CreatePack( const string & assetPackName );
        void                                                LoadPacks( const string & nameOrWildcard, bool allowAsync = false );
        shared_ptr<vaAssetPack>                             FindLoadedPack( const string & assetPackName );
        shared_ptr<vaAssetPack>                             FindOrLoadPack( const string & assetPackName, bool allowAsync = true );
        void                                                UnloadPack( shared_ptr<vaAssetPack> & pack );
        void                                                UnloadAllPacks( );

        const vector<shared_ptr<vaAssetPack>> &             GetAllAssetPacks() const                                { return m_assetPacks; }

        shared_ptr<vaAsset>                                 FindAsset( const string & name );
        vector<shared_ptr<vaAsset>>                         FindAssets( std::function<bool( vaAsset & )> filter );

        bool                                                AnyAsyncOpExecuting( );
        bool                                                HadAnyAsyncOpExecutingLastFrame( )                      { return m_hadAsyncOpLastFrame > 0; }

        vaRenderDevice &                                    GetRenderDevice( )                                      { return m_renderDevice; }

        wstring                                             GetAssetFolderPath( )                                   { return vaCore::GetExecutableDirectory() + L"Media\\AssetPacks\\"; }

    protected:

        // Many assets have DirectX/etc. resource locks so make sure we're not holding any references 
        friend class vaDirectXCore; // <- these should be reorganized so that this is not called from anything that is API-specific
        void                                                OnRenderingAPIAboutToShutdown( );

    public:
        shared_ptr<vaAssetResource>                         UIAssetDragAndDropTarget( vaAssetType assetType, const char* label, const vaVector2 & size = vaVector2( 0, 0 ) );

    //    virtual void                                        UIPanelTick( ) override;
    };


    //////////////////////////////////////////////////////////////////////////

    inline shared_ptr<vaAsset> vaAssetPack::Find( const string & _name, bool lockMutex ) 
    { 
        std::unique_lock<mutex> assetStorageMutexLock(m_assetStorageMutex, std::defer_lock );    if( lockMutex ) assetStorageMutexLock.lock(); else m_assetStorageMutex.assert_locked_by_caller();
        auto it = m_assetMap.find( vaStringTools::ToLower( _name ) );                        
        if( it == m_assetMap.end( ) ) 
            return nullptr; 
        else 
            return it->second; 
    }
}

#include "Scene/vaAssetImporter.h"