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
#include "Core/Containers/vaTrackerTrackee.h"

#include "vaRendering.h"

#include "vaTriangleMesh.h"
#include "vaTexture.h"

#include "Rendering/Shaders/vaSharedTypes.h"


// vaRenderMesh and vaRenderMeshManager are a generic render mesh implementation


// if materialSettings.Transparent and materialSettings.FaceCull is not vaFaceCull::None, draw transparencies twice - first back and then front faces
// (this actually isn't the best way to do this - I think it would be much better to draw all back faces with depth write on and then front faces after as a two pass solution)
// #define VA_AUTO_TWO_PASS_TRANSPARENCIES_ENABLED 

namespace Vanilla
{
    class vaRenderMeshManager;
    class vaRenderMaterial;

    class vaRenderMesh : public vaAssetResource
    {
    public:

        // struct StandardVertexOld
        // {
        //     // first 16 bytes
        //     vaVector3   Position;
        //     uint32_t    Color;
        // 
        //     // next 16 bytes (.w not encoded - can be used for skinning indices for example; should probably be compressed to 16bit floats on the rendering side)
        //     vaVector4   Normal;
        // 
        //     // next 16 bytes (.w stores -1/1 handedness for determining bitangent)
        //     vaVector4   Tangent;
        // 
        //     // next 8 bytes (first UVs; should probably be compressed to 16bit floats on the rendering side)
        //     vaVector2   TexCoord0;
        // 
        //     // next 8 bytes (second UVs; should probably be compressed to 16bit floats on the rendering side)
        //     vaVector2   TexCoord1;
        // };

        // only standard mesh storage supported at the moment
        struct StandardVertex
        {
            // first 16 bytes
            vaVector3   Position;
            uint32_t    Color;

            // next 16 bytes, .w not encoded - can be reused for something else. Should probably be compressed to 16bit floats on the rendering side.
            vaVector4   Normal;

            // next 8 bytes, first set of UVs; could maybe be compressed to 16bit floats, not sure
            vaVector2   TexCoord0;

            // next 8 bytes, second set of UVs; could maybe be compressed to 16bit floats, not sure
            vaVector2   TexCoord1;

            StandardVertex( ) { }
            explicit StandardVertex( const vaVector3 & position ) : Position( position ), Normal( vaVector4( 0, 1, 0, 0 ) ), Color( 0xFF808080 ), TexCoord0( 0, 0 ), TexCoord1( 0, 0 ) {}
            StandardVertex( const vaVector3 & position, const uint32_t & color ) : Position( position ), Normal( vaVector4( 0, 1, 0, 0 ) ), TexCoord0( 0, 0 ), TexCoord1( 0, 0 ), Color( color ) { }
            StandardVertex( const vaVector3 & position, const vaVector4 & normal, const uint32_t & color ) : Position( position ), Normal( normal ), Color( color ), TexCoord0( 0, 0 ), TexCoord1( 0, 0 ) { }
            StandardVertex( const vaVector3 & position, const vaVector4 & normal, const vaVector2 & texCoord0, const uint32_t & color ) : Position( position ), Normal( normal ), TexCoord0( texCoord0 ), TexCoord1( 0, 0 ), Color( color ) { }
            StandardVertex( const vaVector3 & position, const vaVector4 & normal, const vaVector2 & texCoord0, const vaVector2 & texCoord1, const uint32_t & color ) : Position( position ), Normal( normal ), TexCoord0( texCoord0 ), TexCoord1( texCoord1 ), Color( color ) { }

            // StandardVertex( const StandardVertexOld & old ) : Position( old.Position ), Color( old.Color ), Normal( old.Normal ), TexCoord0( old.TexCoord0 ), TexCoord1( old.TexCoord1 ) { }

            bool operator ==( const StandardVertex & cmpAgainst ) const
            {
                return      ( Position == cmpAgainst.Position ) && ( Normal == cmpAgainst.Normal ) && ( TexCoord0 == cmpAgainst.TexCoord0 ) && ( TexCoord1 == cmpAgainst.TexCoord1 ) && ( Color == cmpAgainst.Color );
            }
            
            static bool IsDuplicate( const StandardVertex & left, const StandardVertex & right ) { return left == right; }
        };


        struct StandardVertexAnimationPart
        {
            uint32      Indices;    // (8888_UINT)
            uint32      Weights;    // (8888_UNORM)
        };

        // Legacy from when we had the multiple part option - no longer the case, but this struct seems useful so let's just keep it!
        // Note: if ever desperately needing vertex/index buffer reuse (the main reason for multiple parts), add "alternate vertex buffer source" reference mesh ID and simply reuse that way
        struct SubPart
        {
            int                                         IndexStart;
            int                                         IndexCount;
            vaGUID                                      MaterialID;     // used during loading - could be moved into a separate structure and disposed of after loading

            mutable weak_ptr<vaRenderMaterial>          CachedMaterialRef;

            SubPart( ) : IndexStart( 0 ) , IndexCount ( 0 ), MaterialID ( vaCore::GUIDNull() ) { }
            SubPart( int indexStart, int indexCount, const weak_ptr<vaRenderMaterial> & material );
            SubPart( int indexStart, int indexCount, const vaGUID & materialID ) : IndexStart( indexStart ), IndexCount( indexCount ), CachedMaterialRef(), MaterialID( materialID ) { }
        };

        typedef vaTriangleMesh<StandardVertex>          StandardTriangleMesh;


    private:
        // wstring const                                   m_name;                 // unique (within renderMeshManager) name
        vaTT_Trackee< vaRenderMesh * >                  m_trackee;
        vaRenderMeshManager &                           m_renderMeshManager;

        vaWindingOrder                                  m_frontFaceWinding;
        //bool                                            m_tangentBitangentValid;

        shared_ptr<StandardTriangleMesh>                m_triangleMesh;

        // Legacy from when we had the multiple part option - no longer the case, but this struct seems useful so let's just keep it!
        // Note: if ever desperately needing vertex/index buffer reuse (the main reason for multiple parts), add "alternate vertex buffer source" reference mesh ID and simply reuse that way
        SubPart                                         m_part;

        vaBoundingBox                                   m_boundingBox;      // local bounding box around the mesh

    protected:
        friend class vaRenderMeshManager;
        vaRenderMesh( vaRenderMeshManager & renderMeshManager, const vaGUID & uid );
    public:
        virtual ~vaRenderMesh( )                        { }

    public:
        //const wstring &                                 GetName( ) const                                    { return m_name; };

        vaRenderMeshManager &                           GetManager( ) const                                 { return m_renderMeshManager; }
        int                                             GetListIndex( ) const                               { return m_trackee.GetIndex( ); }

        const vaBoundingBox &                           GetAABB( ) const                                    { return m_boundingBox; }

        // Legacy from when we had the multiple part option - no longer the case, but this struct seems useful so let's just keep it!
        // Note: if ever desperately needing vertex/index buffer reuse (the main reason for multiple parts), add "alternate vertex buffer source" reference mesh ID and simply reuse that way
        const SubPart &                                 GetPart( ) const                                    { return m_part; }
        void                                            SetPart( const SubPart & subPart )                  { m_part = subPart; }

        shared_ptr<vaRenderMaterial>                    GetMaterial( ) const;
        void                                            SetMaterial( const shared_ptr<vaRenderMaterial> & m);

        const shared_ptr<StandardTriangleMesh> &        GetTriangleMesh(  ) const                           { return m_triangleMesh; }
        void                                            SetTriangleMesh( const shared_ptr<StandardTriangleMesh> & mesh );
        void                                            CreateTriangleMesh( vaRenderDevice & device, const vector<StandardVertex> & vertices, const vector<uint32> & indices );

        vaWindingOrder                                  GetFrontFaceWindingOrder( ) const                   { return m_frontFaceWinding; }
        void                                            SetFrontFaceWindingOrder( vaWindingOrder winding )  { m_frontFaceWinding = winding; }

        //bool                                            GetTangentBitangentValid( ) const                   { return m_tangentBitangentValid; }
        //void                                            SetTangentBitangentValid( bool value )              { m_tangentBitangentValid = value; }

        void                                            UpdateAABB( );
        void                                            RebuildNormals( );

        bool                                            SaveAPACK( vaStream & outStream ) override;
        bool                                            LoadAPACK( vaStream & inStream ) override;
        bool                                            SerializeUnpacked( vaXMLSerializer & serializer, const wstring & assetFolder ) override;

        //virtual void                                    ReconnectDependencies( );
        virtual void                                    RegisterUsedAssetPacks( std::function<void( const vaAssetPack & )> registerFunction ) override;
        void                                            EnumerateUsedAssets( const std::function<void(vaAsset * asset)> & callback );

        vaRenderDevice &                                GetRenderDevice() const;

        // startTrackingUIDObject parameter: if creating from non-main thread, do not automatically start tracking; rather finish all loading/creation and then manually register with UIDOBject database (ptr->UIDObject_Track())
        //
        // create mesh with normals, with provided vertices & indices
        static shared_ptr<vaRenderMesh>                 Create( vaRenderDevice& device, const vaMatrix4x4& transform, const std::vector<vaVector3>& vertices, const std::vector<vaVector3>& normals, const std::vector<vaVector2>& texcoords0, const std::vector<vaVector2>& texcoords1, const std::vector<uint32>& indices, vaWindingOrder frontFaceWinding /*= vaWindingOrder::CounterClockwise*/, const vaGUID& uid = vaCore::GUIDCreate( ), bool startTrackingUIDObject = true );
        // create mesh with provided triangle mesh, winding order and material
        static shared_ptr<vaRenderMesh>                 Create( shared_ptr<StandardTriangleMesh> & triMesh, vaWindingOrder frontFaceWinding = vaWindingOrder::CounterClockwise, const vaGUID & materialID = vaGUID::Null, const vaGUID & uid = vaCore::GUIDCreate(), bool startTrackingUIDObject = true );
        // create mesh based on provided vaRenderMesh but without creating new triMesh or material or etc.
        static shared_ptr<vaRenderMesh>                 CreateShallowCopy( const vaRenderMesh & copy, const vaGUID & uid = vaCore::GUIDCreate( ), bool startTrackingUIDObject = true );

        // these use vaStandardShapes::Create* functions and create shapes with center in (0, 0, 0) and each vertex magnitude of 1 (normalized), except where specified otherwise, and then transformed by the provided transform
        static shared_ptr<vaRenderMesh>                 CreatePlane( vaRenderDevice & device, const vaMatrix4x4 & transform, float sizeX = 1.0f, float sizeY = 1.0f, bool doubleSided = false, const vaGUID & uid = vaCore::GUIDCreate() );
        static shared_ptr<vaRenderMesh>                 CreateGrid( vaRenderDevice & device, const vaMatrix4x4 & transform, int dimX, int dimY, float sizeX = 1.0f, float sizeY = 1.0f, const vaVector2 & uvOffsetMul = vaVector2( 1, 1 ), const vaVector2 & uvOffsetAdd = vaVector2( 0, 0 ), const vaGUID & uid = vaCore::GUIDCreate() );
        static shared_ptr<vaRenderMesh>                 CreateTetrahedron( vaRenderDevice & device, const vaMatrix4x4 & transform, bool shareVertices, const vaGUID & uid = vaCore::GUIDCreate() );
        static shared_ptr<vaRenderMesh>                 CreateCube( vaRenderDevice & device, const vaMatrix4x4 & transform, bool shareVertices, float edgeHalfLength = 0.7071067811865475f, const vaGUID & uid = vaCore::GUIDCreate() );
        static shared_ptr<vaRenderMesh>                 CreateOctahedron( vaRenderDevice & device, const vaMatrix4x4 & transform, bool shareVertices, const vaGUID & uid = vaCore::GUIDCreate() );
        static shared_ptr<vaRenderMesh>                 CreateIcosahedron( vaRenderDevice & device, const vaMatrix4x4 & transform, bool shareVertices, const vaGUID & uid = vaCore::GUIDCreate() );
        static shared_ptr<vaRenderMesh>                 CreateDodecahedron( vaRenderDevice & device, const vaMatrix4x4 & transform, bool shareVertices, const vaGUID & uid = vaCore::GUIDCreate() );
        static shared_ptr<vaRenderMesh>                 CreateSphere( vaRenderDevice & device, const vaMatrix4x4 & transform, int tessellationLevel, bool shareVertices, const vaGUID & uid = vaCore::GUIDCreate() );
        static shared_ptr<vaRenderMesh>                 CreateCylinder( vaRenderDevice & device, const vaMatrix4x4 & transform, float height, float radiusBottom, float radiusTop, int tessellation, bool openTopBottom, bool shareVertices, const vaGUID & uid = vaCore::GUIDCreate() );
        static shared_ptr<vaRenderMesh>                 CreateTeapot( vaRenderDevice & device, const vaMatrix4x4 & transform, const vaGUID & uid = vaCore::GUIDCreate() );

    public:
        virtual bool                                    UIPropertiesDraw( vaApplicationBase& application ) override;

        vaAssetType                                     GetAssetType( ) const override                      { return vaAssetType::RenderMesh; }
    };

    struct vaRenderMeshCustomHandler;

    class vaRenderMeshDrawList
    {
    public:
        struct Entry
        {
            shared_ptr<vaRenderMesh>                    Mesh;
            shared_ptr<vaRenderMaterial>                Material;                   // while this is actually part of material, we resolve the reference during insertion, also allowing for it to be overridden
            vaMatrix4x4                                 Transform;

            // TODO: change to std::function
            // per-instance data for custom rendering - could be something simple like custom per-instance coloring or mesh
            // (can be modified before each call to vaREnderMeshManager::Draw)
            vaRenderMeshCustomHandler *                 CustomHandler   = nullptr;  // per-entry or global handler  (and can be used as a type identifier through RTTI or similar)
            uint64                                      CustomPayload   = 0;        // per-entry payload

            // per-draw-call shading rate
            // (can be modified before each call to vaREnderMeshManager::Draw)
            vaShadingRate                               ShadingRate     = vaShadingRate::ShadingRate1X1;
            vaVector4                                   CustomColor     = vaVector4( 0.0f, 0.0f, 0.0f, 0.0f );  // for debugging visualization (default means "do not override")

            Entry( const std::shared_ptr<vaRenderMesh> & mesh, const std::shared_ptr<vaRenderMaterial> & material, const vaMatrix4x4 & transform, vaShadingRate shadingRate = vaShadingRate::ShadingRate1X1, const vaVector4 & customColor = vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) );

            Entry( const Entry & copy ) : Mesh( copy.Mesh ), Material( copy.Material ), Transform( copy.Transform ), CustomHandler( copy.CustomHandler ), CustomPayload( copy.CustomPayload ), ShadingRate( copy.ShadingRate ), CustomColor( copy.CustomColor ) { }
        };

    private:
        vector< Entry >                                 m_drawList;
        //vaRenderSelectionCullFlags                      m_usedFilter;

        struct SortState
        {
            vaRenderSelection::SortSettings             SortSettings;
            bool                                        Enabled             = false;
            bool                                        Sorted              = false;
            vector<pair<int,float>>                     SortDistances;      // first is a sort group - items first get sorted by it and then by distance
            vector<int>                                 SortedIndices;
        } mutable                                       m_sortState;

    public:
        // can be useful to deallocate / dispose of any per-list temporary storage, for ex. anything pointed to by the CustomPayload-s
        // only called once before m_drawList.clear() and then cleared as well!
        vaEvent<void(vaRenderMeshDrawList& list)>       Event_PreReset;

    public:
        vaRenderMeshDrawList( )                         { }
        vaRenderMeshDrawList( const vaRenderMeshDrawList & src ) : m_drawList( src.m_drawList ) { }
        vaRenderMeshDrawList & operator = ( const vaRenderMeshDrawList & src ) { Reset(); m_drawList = src.m_drawList; return *this; }
        ~vaRenderMeshDrawList( )                        { Reset( ); }

    public:
        void                                            Reset( )                            { Event_PreReset.Invoke( *this ); Event_PreReset.RemoveAll(); m_drawList.clear(); }
        int                                             Count( ) const                      { return (int)m_drawList.size(); }
        
        // shadingRateOffset gets combined with material shading rate offset and, based on material horizontal/vertical preference converted into actual shading rate 
        void                                            Insert( const std::shared_ptr<vaRenderMesh> & mesh, const std::shared_ptr<vaRenderMaterial> & material, const vaMatrix4x4 & transform, vaShadingRate shadingRate, const vaVector4 & customColor );

        // version that takes the material off the mesh, if possible, and has defaults for everything else - super-simple
        void                                            Insert( const std::shared_ptr<vaRenderMesh> & mesh, const vaMatrix4x4 & transform, vaShadingRate shadingRate = vaShadingRate::ShadingRate1X1, const vaVector4 & customColor = vaVector4( 0.0f, 0.0f, 0.0f, 0.0f ) );
        
        //void                                            SetShadingRate( int index, vaShadingRate shadingRate )  { m_drawList[index].ShadingRate = shadingRate; }
        //void                                            SetColorOverride( int index, vaVector4 & colorOverride) { m_drawList[index].ColorOverride = colorOverride; }

        const Entry &                                   operator[] ( int index ) const      { return m_drawList[index]; }

    private:
        friend class vaRenderMeshManager;
        const SortState &                               SortState( ) const                  { return m_sortState; }
        void                                            StartSort( const vaRenderSelection::SortSettings & sortSettings ) const;
        void                                            FinalizeSort( const vaRenderSelection::SortSettings & sortSettings ) const;
    };

    struct vaRenderMeshCustomHandler
    {
        virtual void    ApplyMeshCustomization( vaSceneDrawContext & drawContext, const vaRenderMeshDrawList::Entry & entry, const vaRenderMaterial & material, vaGraphicsItem & renderItem )    = 0;
    };

    enum class vaRenderMeshDrawFlags : uint32
    {
        None                    = 0,
        // SkipTransparencies      = ( 1 << 0 ),           //                                      should really go to vaRenderSelectionCullFlags instead of here
        // SkipNonTransparencies   = ( 1 << 1 ),           // a.k.a only draw shadow casters       should really go to vaRenderSelectionCullFlags instead of here
        EnableDepthTest         = ( 1 << 2 ),
        InvertDepthTest         = ( 1 << 3 ),
        EnableDepthWrite        = ( 1 << 4 ),
        DepthTestIncludesEqual  = ( 1 << 5 ),
        SkipNonShadowCasters    = ( 1 << 6 ),           //                                      should really go to vaRenderSelectionCullFlags instead of here
        DepthTestEqualOnly      = ( 1 << 7 ),           // only draw if exact value already in the depth buffer
        // SkipNoDepthPrePass      = ( 1 << 7 ),           // skip all meshes/materials flagged with NoDepthPrePass
        // ReverseDrawOrder        = ( 1 << 8 ),           // reverse mesh list draw order - if sorted; by default sorting is front to back so reversing means draw back to front
        DisableVRS              = ( 1 << 9 ),           // disable variable rate shading - always use 1x1 (useful for depth prepass)
    };

    BITFLAG_ENUM_CLASS_HELPER( vaRenderMeshDrawFlags );

    class vaRenderMeshManager : public vaRenderingModule, public vaUIPanel
    {
        VA_RENDERING_MODULE_MAKE_FRIENDS( );

    public:

    protected:
        vaTT_Tracker< vaRenderMesh * >                  m_renderMeshes;

        bool                                            m_isDestructing;

        vaTypedConstantBufferWrapper< ShaderInstanceConstants, true >
                                                        m_constantsBuffer;

    public:
//        friend class vaRenderingCore;
        vaRenderMeshManager( const vaRenderingModuleParams & params );
        virtual ~vaRenderMeshManager( );

    private:
        friend class MVMSimpleMeshManagerDX11;

    private:
        void                                            RenderMeshesTrackeeAddedCallback( int newTrackeeIndex );
        void                                            RenderMeshesTrackeeBeforeRemovedCallback( int removedTrackeeIndex, int replacedByTrackeeIndex );

    public:
        virtual vaDrawResultFlags                       Draw( vaSceneDrawContext & drawContext, const vaRenderMeshDrawList & list, vaBlendMode blendMode, vaRenderMeshDrawFlags drawFlags, const vaRenderSelection::SortSettings & sortSettings = vaRenderSelection::SortSettings(),
                                                                std::function< void( const vaRenderMeshDrawList::Entry & entry, const vaRenderMaterial & material, vaGraphicsItem & renderItem ) > globalCustomizer = nullptr );

        vaTT_Tracker< vaRenderMesh * > *                GetRenderMeshTracker( )                                                     { return &m_renderMeshes; }

        shared_ptr<vaRenderMesh>                        CreateRenderMesh( const vaGUID & uid = vaCore::GUIDCreate(), bool startTrackingUIDObject = true );

    protected:
        virtual string                                  UIPanelGetDisplayName( ) const override { return "Meshes"; } //vaStringTools::Format( "vaRenderMaterialManager (%d meshes)", m_renderMaterials.size( ) ); }
        virtual void                                    UIPanelTick( vaApplicationBase & application ) override;
    };

    inline void vaRenderMeshDrawList::Insert( const std::shared_ptr<vaRenderMesh> & mesh, const std::shared_ptr<vaRenderMaterial> & material, const vaMatrix4x4 & transform, vaShadingRate shadingRate, const vaVector4 & customColor )
    {
        if( mesh == nullptr )
        {
            VA_WARN( "vaRenderMeshDrawList::Insert - trying to add nullptr mesh, ignoring" );
            return;
        }
        if( material == nullptr )
        {
            VA_WARN( "vaRenderMeshDrawList::Insert - trying to add nullptr material, ignoring" );
            return;
        }
        m_sortState.Sorted = false;
        
        m_drawList.push_back( Entry( mesh, material, transform, shadingRate, customColor ) );
    }

}