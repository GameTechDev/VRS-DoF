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

#include "Rendering/vaShader.h"

#include "Core/vaXMLSerialization.h"

#include <optional>

// for PBR, reading material:
// - http://blog.selfshadow.com/publications/s2015-shading-course/
// - https://www.allegorithmic.com/pbr-guide
// - http://www.marmoset.co/toolbag/learn/pbr-practice
// - http://www.marmoset.co/toolbag/learn/pbr-theory
// - http://www.marmoset.co/toolbag/learn/pbr-conversion
// - http://docs.cryengine.com/display/SDKDOC2/Creating+Textures+for+Physical+Based+Shading
// - 


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// this is all super-simplified - real generic node-based system would be very nice; in that case I'd organize it something like
// (just writing it down to avoid the temptation of doing any of it now :) )
//  struct NodeConnection
//    * typed (bool, int, scalar, vec4) input or output; can be implemented with std::variant?
//  struct NodeBase
//    * has a GUID? or maybe better just unique name?
//    * has a list of (uniquely) named inputs & outputs, NodeConnection types; inputs can have defaults and UI parameters
//    * connections are identified with the node name + input/output name pair 
//  struct ShaderNode : NodeBase
//    * has only inputs, outputs hardcoded
//  struct TextureNode : NodeBase
//    * has only output(s) based on texture type
//  struct ConversionNode : NodeBase
//    * one input, one output; can convert from any NodeConnection to any ConversionNode type and also do any swizzling operation
//  struct MathNode : NodeBase
//    * various math operations
// etc etc...
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Vanilla
{
    class vaRenderMaterialManager;

    struct vaRenderMaterialConstructorParams : vaRenderingModuleParams
    {
        vaRenderMaterialManager &   RenderMaterialManager;
        const vaGUID &          UID;
        vaRenderMaterialConstructorParams( vaRenderDevice & device, vaRenderMaterialManager & renderMaterialManager, const vaGUID & uid ) : vaRenderingModuleParams(device), RenderMaterialManager( renderMaterialManager ), UID( uid ) { }
    };

    enum class vaRenderMaterialShaderType
    {
        DepthOnly       = 0,            // Z-pre-pass; no pixels are shaded unless alpha tested
        CustomShadow    = 1,            // kept potentially for reflective shadow maps or similar
        Forward         = 2,
        //Deferred        = 2,          // removed for now
    };

    //
    // // similar to BLEND_MODE_* from Filament - it's more complex than just the blend mode (which is encapsulated in vaBlendMode)
    // enum class vaRenderMaterialBlendType
    // {
    //     Opaque          = 0,
    //     Masked          = 1,
    //     Transparent     = 2,
    //     Additive        = 3,
    //     Multiply        = 4,
    // };

    struct vaRenderMaterialCachedShaders;

    class vaRenderMaterial : public vaAssetResource, public vaRenderingModule
    {
    public:

        // main shader input type
        typedef std::variant<bool, int32, float, vaVector3, vaVector4>  ValueType;

        // list of ValueType indices for easier use
        enum class ValueTypeIndex : int32
        {
            Bool        = 0,                    // one bool value; if used as a static input (IsValueDynamic == false) then it will be compiled out
            Integer     = 1,                    // one int32 value
            Scalar      = 2,                    // one float value
            Vector3     = 3,                    // 3 float values
            Vector4     = 4,                    // 4 float values
            MaxValue,
            Undefined   = -1                    // hmm should be https://en.cppreference.com/w/cpp/utility/variant/variant_npos but don't want to use size_t
        };

        class Node : public vaXMLSerializableObject
        {
        protected:
            friend class vaRenderMaterial;
            string                      Name;
            ValueTypeIndex              Type;
            
            mutable bool                InUse = false;      // whether the input is connected to any input slots! updated at runtime; can also indicate that the resource is not available at the moment but will be after loading or etc.

        protected:
            Node( ) = delete;
            Node( const string & name, const ValueTypeIndex & type );

        protected:
            virtual bool                Serialize( vaXMLSerializer & ) override;
            virtual bool                UIDraw( vaApplicationBase &, vaRenderMaterial & ownerMaterial )                    = 0;

        protected:
            virtual string              GetShaderMaterialInputsType( ) const;
            //virtual string              GetShaderMaterialInputsVariableName( ) const    { return Name; }
            virtual string              GetShaderMaterialInputLoader( ) const           = 0;

            virtual void                ResetTemps( ) const                             = 0;
            // This will be occasionally called to account for external changes (such as the texture getting deleted/loaded) without setting the m_inputsDirty dirty flag;
            // Returning 'true' will set the flag and thus update all connections & dependencies.
            virtual bool                RequiresReUpdate( ) const                       { return false; }

            virtual string              GetUIShortInfo( ) const                         { return Name; }

        public:
            virtual ~Node( ) { }

            const string &              GetName( ) const                                { return Name; }
            ValueTypeIndex              GetType( ) const                                { return Type; }
        };

        //just not implemented yet
        //struct ValueNode : public vaXMLSerializable
        //{
        //}

        class TextureNode : public Node
        {
        protected:
            friend class vaRenderMaterial;

            vaGUID                      UID             = vaCore::GUIDNull( );                  // If vaCore::GUIDNull(), value defaults to ValueProperties::Default
            vaStandardSamplerType       SamplerType     = vaStandardSamplerType::PointClamp;    // Which standard sampler to use
            int                         UVIndex         = -1;                                   // Which vertex (interpolant) UV index to use

            // temporary thingies
            mutable weak_ptr<vaTexture> CachedTexture;
            mutable int                 ComputedShaderTextureSlot          = -1;

        protected:
            virtual bool                Serialize( vaXMLSerializer& ) override;
            virtual const char *        GetSerializableTypeName( ) const                { return "TextureNode"; }

            virtual bool                UIDraw( vaApplicationBase& , vaRenderMaterial & ownerMaterial ) override;
            virtual string              GetShaderMaterialInputLoader( ) const override;
            virtual void                ResetTemps( ) const override                    { CachedTexture.reset(); ComputedShaderTextureSlot = -1; }
            virtual bool                RequiresReUpdate( ) const override              { shared_ptr<vaTexture> prevTexture = CachedTexture.lock(); return prevTexture != GetTexture(); }

        public:
            TextureNode( ) : Node( "", ValueTypeIndex::Undefined ) { };
            TextureNode( const string& name, const vaGUID& textureUID, vaStandardSamplerType samplerType = vaStandardSamplerType::PointClamp, int uvIndex = 0 );
            TextureNode( const string& name, const vaTexture& texture, vaStandardSamplerType samplerType = vaStandardSamplerType::PointClamp, int uvIndex = 0 );
            TextureNode( const TextureNode & copy );

            shared_ptr<vaTexture>       GetTexture( ) const;
            const vaGUID &              GetTextureUID( ) const                          { return UID; };
        };

        // for editing defaults to an input slot a constant value node
        struct ValueProperties : public vaXMLSerializable
        {
            ValueType                   Default;                                        // todo: rename to VALUE
            ValueType                   Min;
            ValueType                   Max;
            bool                        IsColor = false;                                // only influences editing UI
            bool                        IsMultiplier = false;                           // if true and there is a node connected to the slot, the Default/Value value will premultiply the value coming from the node

            ValueProperties( ) { }
            ValueProperties( const ValueType& default, const ValueType& min, const ValueType& max, bool isMultiplier, bool isColor ) : Default( default ), Min( min ), Max( max ), IsColor( isColor ), IsMultiplier( isMultiplier ) { }
            ValueProperties( const ValueType& default, bool isMultiplier, bool isColor );

            virtual bool                Serialize( vaXMLSerializer& serializer ) override;
            bool                        DrawUI( );

            bool                        ClampMinMax( ValueType& value );

            string                      GetUIShortInfo( ) const;

            ValueTypeIndex              GetType( ) const                                { assert( Default.index() == Min.index() && Default.index() == Max.index() ); return (ValueTypeIndex)Default.index(); }

            // a.k.a. GetChannelCount - how many components does the type hold (1 - .x, 2 - .xy, 3 - .xyz, 4. - .xyzw)
            int                         GetComponentCount( ) const;
        };

        class InputSlot : public vaXMLSerializable
        {
        private:
            friend class vaRenderMaterial;

            // this defines how the input is visible to the shader (VA_RM_INPUT_'Name' - i.e. VA_RM_INPUT_BaseColor)
            /*const */string            Name                    = "";

            // this defines the type and also holds the 'default' value which is used if no input node is connected (or cannot be found)
            ValueProperties             Properties;

            // at the moment one node is one value; in the future there might be input node name and slot name
            string                      ConnectedInput          = "";   // name of the connected input node - so far just keep it simple like that

            // how to read from the connected node
            char                        InputSwizzle[5]         = { 'x', 'y', 'z', 'w', '\0' };

            mutable weak_ptr<const Node> CachedConnectedInput;

        protected:

            virtual bool                Serialize( vaXMLSerializer& serializer ) override;
            void                        ResetTemps( )                                   { CachedConnectedInput.reset(); }

            bool                        UIDraw( vaApplicationBase &, vaRenderMaterial & ownerMaterial );

        public:
            InputSlot( ) { };
            InputSlot( const InputSlot & copy );
            InputSlot( const string & name, const ValueProperties & properties );
            InputSlot( const string & name, const ValueType & default, bool defaultIsMultiplier, bool isColor ) : InputSlot( name, ValueProperties( default, defaultIsMultiplier, isColor ) ) { }

            const string &              GetName( ) const                                { return Name; }
            ValueTypeIndex              GetType( ) const                                { return (ValueTypeIndex)Properties.Default.index(); }
            
            // a.k.a. GetChannelCount - how many components does the type hold (1 - .x, 2 - .xy, 3 - .xyz, 4. - .xyzw)
            int                         GetComponentCount( ) const                      { return Properties.GetComponentCount(); }
            const ValueType &           GetDefaultValue( ) const                        { return Properties.Default; }

            string                      GetShaderMaterialInputsType( ) const;
        };

        struct MaterialSettings
        {
            vaFaceCull                  FaceCull                = vaFaceCull::Back;
            vaLayerMode                 LayerMode               = vaLayerMode::Opaque;
            int32                       DecalSortOrder          = 0;
            float                       AlphaTestThreshold      = 0.5f;             // to be moved to Inputs if needed
            bool                        ReceiveShadows          = true;
            bool                        CastShadows             = true;
            bool                        Wireframe               = false;
            bool                        AdvancedSpecularShader  = true;
            bool                        SpecialEmissiveLight    = false;            // only add emissive within light sphere, and then scale with light itself; this is to allow emissive materials to be 'controlled' by the light - useful for models that represent light emitters (lamps, etc.)
            float                       LocalIBLNormalBasedBias = 0;                // see vaLighting.hlsl transitionNormalBias - it's super-hacky and temporary
            float                       LocalIBLBasedBias       = 0;
            bool                        VRSPreferHorizontal     = true;             // whether to prefer 2x1/4x2 over 1x2/2x4 rates
            int                         VRSRateOffset           = 0;                // from -4 (no VRS, ever) to 4 (4x4 VRS)

            MaterialSettings( ) { }

            inline bool operator == ( const MaterialSettings & mat ) const
            {
                return 0 == memcmp( this, &mat, sizeof( MaterialSettings ) );
            }

            inline bool operator != ( const MaterialSettings & mat ) const
            {
                return 0 != memcmp( this, &mat, sizeof( MaterialSettings ) );
            }
        };

        struct ShaderSettings
        {
            // file / entry point
            pair< string, string >              VS_Standard;
            pair< string, string >              GS_Standard;
            pair< string, string >              PS_DepthOnly;
            pair< string, string >              PS_Forward;
            //pair< string, string >              PS_Deferred;            // more of a placeholder, not really used anymore, waiting for future need
            pair< string, string >              PS_CustomShadow;

            vector< pair< string, string > >    BaseMacros;

            inline bool operator == ( const ShaderSettings & other ) const
            {
                return VS_Standard      == other.VS_Standard
                    && GS_Standard      == other.GS_Standard
                    && PS_DepthOnly     == other.PS_DepthOnly
                    && PS_Forward       == other.PS_Forward
                //    && PS_Deferred      == other.PS_Deferred
                    && PS_CustomShadow  == other.PS_CustomShadow
                    && BaseMacros       == other.BaseMacros;
            }

            inline bool operator != ( const ShaderSettings & other ) const  { return !( (*this)==other ); }
        };

    private:
        //wstring const                                   m_name;                 // unique (within renderMeshManager) name
        vaTT_Trackee< vaRenderMaterial * >              m_trackee;

    protected:
        vaRenderMaterialManager &                       m_renderMaterialManager;

        // Primary material data
        MaterialSettings                                m_materialSettings;
        ShaderSettings                                  m_shaderSettings;

        std::vector<shared_ptr<Node>>                   m_nodes;
        std::vector<InputSlot>                          m_inputSlots;

        vector< pair< string, string > >                m_shaderMacros;
        bool                                            m_shaderMacrosDirty;
        bool                                            m_shadersDirty;

        int                                             m_inputsDirtyThoroughTextureCheckCounter = vaRandom::Singleton.NextIntRange(0, 4);

        int64                                           m_lastUpdateFrame = 0;

        shared_ptr<vaRenderMaterialCachedShaders>       m_shaders;

        // vaTypedConstantBufferWrapper< RenderMeshMaterialConstants >
        //                                                 m_constantsBuffer;

        bool                                            m_immutable = false;            // for default or protected materials used by multiple systems that should not be changed - will assert on any attempt to change

        bool                                            m_inputsDirty               = true;     // set after any changes to m_nodes or m_inputSlots
        int                                             m_computedTextureSlotCount  = 0;
        double                                          m_delayedInputsSetDirty     = std::numeric_limits<double>::max();

    protected:
        friend class vaRenderMeshManager;
        vaRenderMaterial( const vaRenderingModuleParams & params );
    public:
        virtual ~vaRenderMaterial( )                { }

        // copying by value not supported - use 'SetupFromOther'
        vaRenderMaterial( const vaRenderMaterial& ) = delete;
        vaRenderMaterial& operator = ( const vaRenderMaterial& ) = delete;

    public:
        //const wstring &                                 GetName( ) const                                                { return m_name; };

        const MaterialSettings &                        GetMaterialSettings( ) const                                    { return m_materialSettings; }
        void                                            SetMaterialSettings( const MaterialSettings & settings )        { assert( !m_immutable ); if( m_materialSettings != settings ) m_shaderMacrosDirty = true; m_materialSettings = settings; }

        const ShaderSettings &                          GetShaderSettings( ) const                                      { return m_shaderSettings; }
        void                                            SetShaderSettings( const ShaderSettings & settings )            { assert( !m_immutable ); if( m_shaderSettings != settings ) m_shadersDirty = true; m_shaderSettings = settings; }

        void                                            SetSettingsDirty( )                                             { m_shaderMacrosDirty = true; }
        void                                            SetShadersDirty( )                                              { m_shaderMacrosDirty = true; m_shadersDirty = true; }
        void                                            SetInputsDirty( )                                               { m_inputsDirty = true; m_shaderMacrosDirty = true; m_shadersDirty = true; }
        void                                            SetDelayedDirty( double delayTime );

        bool                                            IsDirty( ) const                                                { return m_inputsDirty || m_shaderMacrosDirty || m_shadersDirty || (m_delayedInputsSetDirty != std::numeric_limits<double>::max()); }

        vaRenderMaterialManager &                       GetManager( ) const                                             { return m_renderMaterialManager; }
        int                                             GetListIndex( ) const                                           { return m_trackee.GetIndex( ); }

        void                                            RemoveAllNodes( );
        void                                            RemoveAllInputSlots( );

        // few helpers (depend only on MaterialSettings)
        bool                                            IsTransparent( ) const                                          { return m_materialSettings.LayerMode == vaLayerMode::Decal || m_materialSettings.LayerMode == vaLayerMode::Transparent; }
        bool                                            IsAlphaTested( ) const                                          { return m_materialSettings.LayerMode == vaLayerMode::AlphaTest; }
        bool                                            IsDecal( ) const                                                { return m_materialSettings.LayerMode == vaLayerMode::Decal; }
        vaShadingRate                                   ComputeShadingRate( int baseShadingRate ) const;

        template< typename NodeType = Node >
        shared_ptr<const NodeType>                      FindNode( const string & name )                             { for( int i = 0; i < m_nodes.size(); i++ ) if( vaStringTools::ToLower( m_nodes[i]->Name ) == vaStringTools::ToLower( name ) ) return std::dynamic_pointer_cast<NodeType, Node>(m_nodes[i]); return nullptr; }
        bool                                            RemoveNode( const string & name, bool assertIfNotFound );
        string                                          FindAvailableNodeName( const string & name );
        bool                                            SetNode( const shared_ptr<Node> & node );
        bool                                            SetTextureNode( const string& name, const vaGUID& textureUID, vaStandardSamplerType samplerType = vaStandardSamplerType::PointClamp, int uvIndex = 0 );
        bool                                            SetTextureNode( const string& name, const vaTexture& texture, vaStandardSamplerType samplerType = vaStandardSamplerType::PointClamp, int uvIndex = 0 )
                                                                                                                        { return SetTextureNode( name, texture.UIDObject_GetUID(), samplerType, uvIndex ); }
        bool                                            SetTextureNode( const string& name, const shared_ptr<vaTexture>& texture, vaStandardSamplerType samplerType = vaStandardSamplerType::PointClamp, int uvIndex = 0 )
                                                                                                                        { return SetTextureNode( name, texture->UIDObject_GetUID(), samplerType, uvIndex ); }
        // similar to SetTextureNode except it preserves existing samplerType and uvIndex (and other if any) info
        bool                                            ReplaceTextureOnNode( const string & name, const vaGUID& textureUID );
        bool                                            ReplaceTextureOnNode( const string & name, const vaTexture & texture )
                                                                                                                        { return ReplaceTextureOnNode( name, texture.UIDObject_GetUID() ); }
        bool                                            ReplaceTextureOnNode( const string & name, const shared_ptr<vaTexture> & texture )
                                                                                                                        { return ReplaceTextureOnNode( name, texture->UIDObject_GetUID() ); }

        std::optional<const InputSlot>                  FindInputSlot( const string & name )                        { for( int i = 0; i < m_inputSlots.size(); i++ ) if( vaStringTools::ToLower( m_inputSlots[i].Name ) == vaStringTools::ToLower( name ) ) return m_inputSlots[i]; return std::nullopt; }
        bool                                            RemoveInputSlot( const string & name, bool assertIfNotFound );
        bool                                            SetInputSlot( const InputSlot & newValue );
        bool                                            SetInputSlot( const string & name, const ValueType & default, bool defaultIsMultiplier, bool isColor );
        // same as SetInputSlot() except it doesn't add new input slot if it doesn't exist, and it doesn't change the default value if the type is different
        bool                                            SetInputSlotDefaultValue( const string & name, const ValueType & default );

        // to disconnect just use ConnectInputSlotWithNode( "InputSlotName", "" );
        bool                                            ConnectInputSlotWithNode( const string & inputSlotName, const string & nodeName, const string & inputSwizzle = "xyzw", bool assertIfFailed = true );

//        bool                                            GetNeedsPSForShadowGenerate( ) const                            { return false; }

        vaFaceCull                                      GetFaceCull( ) const                                            { return m_materialSettings.FaceCull; }

        // disabled for now (for simplicity reasons - and it's small anyway) but the vaAssetRenderMesh will use SerializeUnpacked to/from memory stream to support APACK! :)
        bool                                            SaveAPACK( vaStream & outStream ) override;
        bool                                            LoadAPACK( vaStream & inStream ) override;
        bool                                            SerializeUnpacked( vaXMLSerializer & serializer, const wstring & assetFolder ) override;

        //virtual void                                    ReconnectDependencies( );
        virtual void                                    RegisterUsedAssetPacks( std::function<void( const vaAssetPack & )> registerFunction ) override;

        // for default or protected materials used by multiple systems that should not be changed - will assert on any attempt to change
        void                                            SetImmutable( bool immutable )          { m_immutable = immutable; }

        bool                                            SetToRenderItem( vaGraphicsItem & renderItem, vaRenderMaterialShaderType shaderType, vaDrawResultFlags & inoutDrawResults );

        // Maybe it's time to refactor this to a big switch & enum? the enum is different from vaRenderMaterialShaderType though and requires indication on whether it's a VS/GS/PS too
        void                                            GetShaderState_VS_Standard    ( vaShader::State & outState, string & outErrorString );
        void                                            GetShaderState_GS_Standard    ( vaShader::State & outState, string & outErrorString );
        void                                            GetShaderState_PS_DepthOnly   ( vaShader::State & outState, string & outErrorString );
        void                                            GetShaderState_PS_Forward     ( vaShader::State & outState, string & outErrorString );
        //void                                            GetShaderState_PS_Deferred    ( vaShader::State & outState, string & outErrorString );
        void                                            GetShaderState_PS_CustomShadow( vaShader::State & outState, string & outErrorString );

    protected:
        shared_ptr<vaVertexShader>                      GetVS( vaRenderMaterialShaderType shaderType );
        shared_ptr<vaGeometryShader>                    GetGS( vaRenderMaterialShaderType shaderType );
        shared_ptr<vaPixelShader>                       GetPS( vaRenderMaterialShaderType shaderType );

        void                                            VerifyNames( );

        // this will update shader macros based on material inputs but will not recreate shaders
        void                                            UpdateShaderMacros( );
        
        // this will call UpdateShaderMacros and recreate shaders accordingly
        bool                                            Update( );

        // called after m_inputs are loaded
        void                                            UpdateInputsDependencies( );

    private:
        const std::vector<InputSlot> &                  GetInputSlots( ) const                                      { return m_inputSlots; }
        int                                             FindInputSlotIndex( const string & name ) const             { for( int i = 0; i < m_inputSlots.size(); i++ ) if( vaStringTools::ToLower( m_inputSlots[i].Name ) == vaStringTools::ToLower( name ) ) return i; return -1; }
        const std::vector<shared_ptr<Node>> &           GetNodes( ) const                                           { return m_nodes; }
        int                                             FindNodeIndex( const string & name ) const                  { for( int i = 0; i < m_nodes.size(); i++ ) if( vaStringTools::ToLower( m_nodes[i]->Name ) == vaStringTools::ToLower( name ) ) return i; return -1; }

    public:
        static vector<string>                           GetPresetMaterials( );
        bool                                            SetupFromPreset( const string & presetName, bool removeNodes = false );
        bool                                            SetupFromOther( const vaRenderMaterial & other );

        void                                            EnumerateUsedAssets( const std::function<void(vaAsset * asset)> & callback );

    public:
        bool                                            UIPropertiesDraw( vaApplicationBase & application ) override;

        vaAssetType                                     GetAssetType( ) const override                              { return vaAssetType::RenderMaterial; }
    };

    struct vaRenderMaterialCachedShaders
    {
        struct Key
        {
            //wstring                     WStringPart;
            string                      AStringPart;

            bool                        operator == ( const Key & cmp ) const { return this->AStringPart == cmp.AStringPart; }
            bool                        operator >( const Key & cmp ) const { return this->AStringPart > cmp.AStringPart; }
            bool                        operator < ( const Key & cmp ) const { return this->AStringPart < cmp.AStringPart; }

            // alphaTest is part of the key because it determines whether PS_DepthOnly is needed at all; all other shader parameters are contained in shaderMacros
            Key( bool alphaTest, const vaRenderMaterial::ShaderSettings & shaderSettings, const vector< pair< string, string > > & shaderMacros )
            {
                //WStringPart = fileName;
                AStringPart = "";
                AStringPart += "&" + shaderSettings.VS_Standard.first       + "&" + shaderSettings.VS_Standard.second;
                AStringPart += "&" + shaderSettings.PS_DepthOnly.first      + "&" + shaderSettings.PS_DepthOnly.second;
                AStringPart += "&" + shaderSettings.PS_Forward.first        + "&" + shaderSettings.PS_Forward.second;
                // AStringPart += "&" + shaderSettings.PS_Deferred.first       + "&" + shaderSettings.PS_Deferred.second;
                AStringPart += "&" + shaderSettings.PS_CustomShadow.first   + "&" + shaderSettings.PS_CustomShadow.second;
                AStringPart += "&" + shaderSettings.GS_Standard.first       + "&" + shaderSettings.GS_Standard.second;

                AStringPart += ((alphaTest)?("a&"):("b&"));
                for( int i = 0; i < shaderMacros.size(); i++ )
                    AStringPart += shaderMacros[i].first + "&" + shaderMacros[i].second + "&";
            }
        };

        vaRenderMaterialCachedShaders( vaRenderDevice & device ) : VS_Standard( device ), GS_Standard( device ), PS_DepthOnly( device ), PS_Forward( device ), /*PS_Deferred( device ),*/ PS_CustomShadow( device ) { }

        vaAutoRMI<vaVertexShader>          VS_Standard;
        vaAutoRMI<vaGeometryShader>        GS_Standard;

        vaAutoRMI<vaPixelShader>           PS_DepthOnly;
        vaAutoRMI<vaPixelShader>           PS_Forward;
        //vaAutoRMI<vaPixelShader>           PS_Deferred;
        vaAutoRMI<vaPixelShader>           PS_CustomShadow;
    };

    class vaRenderMaterialManager : public vaRenderingModule, public vaUIPanel
    {
        //VA_RENDERING_MODULE_MAKE_FRIENDS( );
    protected:

        vaTT_Tracker< vaRenderMaterial * >              m_renderMaterials;
        // map<wstring, shared_ptr<vaRenderMaterial>>  m_renderMaterialsMap;

        shared_ptr< vaRenderMaterial >                  m_defaultMaterial;
        bool                                            m_isDestructing;

        bool                                            m_texturingDisabled;

        map< vaRenderMaterialCachedShaders::Key, weak_ptr<vaRenderMaterialCachedShaders> >
                                                        m_cachedShaders;

        vector< pair< string, string > >                m_globalShaderMacros;

        bool                                            m_globalGSOverrideEnabled       = false ;

        shared_ptr<vaTexture>                           m_DFG_LUT;                                      // dfg term lookup table, see https://google.github.io/filament/Filament.html#lighting/imagebasedlights/distantlightprobes

    public:
        vaRenderMaterialManager( const vaRenderingModuleParams & params );
        virtual ~vaRenderMaterialManager( );

    private:
        void                                            RenderMaterialsTrackeeAddedCallback( int newTrackeeIndex );
        void                                            RenderMaterialsTrackeeBeforeRemovedCallback( int removedTrackeeIndex, int replacedByTrackeeIndex );

    public:
        shared_ptr<vaRenderMaterial>                    GetDefaultMaterial( ) const                             { return m_defaultMaterial; }

        // warning: changing global shader macros will force recompile of all shaders; this is mostly useful for debugging and similar purposes
        const vector< pair< string, string > > &        GetGlobalShaderMacros( ) const                          { return m_globalShaderMacros; }
        void                                            SetGlobalShaderMacros( const vector< pair< string, string > > & globalShaderMacros = vector< pair< string, string > >() );

        // sets GS file to the same as VS and entry point to GS_Standard
        bool                                            GetGlobalGSOverride( )                                  { return m_globalGSOverrideEnabled; }
        void                                            SetGlobalGSOverride( bool enabled );

    public:
        // if creating from non-main thread, do not automatically start tracking; rather finish all loading/creation and then manually register with UIDOBject database (ptr->UIDObject_Track())
        shared_ptr<vaRenderMaterial>                    CreateRenderMaterial( const vaGUID & uid = vaCore::GUIDCreate( ), bool startTrackingUIDObject = true );
        vaTT_Tracker< vaRenderMaterial * > *            GetRenderMaterialTracker( ) { return &m_renderMaterials; }

        bool                                            GetTexturingDisabled( ) const   { return m_texturingDisabled; }
        void                                            SetTexturingDisabled( bool texturingDisabled );

    public:
        void                                            UpdateAndSetToGlobals( vaSceneDrawContext & drawContext, vaShaderItemGlobals & shaderItemGlobals );

    public:
        // alphaTest is part of the key because it determines whether PS_DepthOnly is needed at all; all other shader parameters are contained in shaderMacros
        shared_ptr<vaRenderMaterialCachedShaders>       FindOrCreateShaders( bool alphaTest, const vaRenderMaterial::ShaderSettings & shaderSettings, const vector< pair< string, string > > & shaderMacros );

    protected:
        virtual string                                  UIPanelGetDisplayName( ) const override { return "Materials"; } //vaStringTools::Format( "vaRenderMaterialManager (%d meshes)", m_renderMaterials.size( ) ); }
        virtual void                                    UIPanelTick( vaApplicationBase & application ) override;

    public:
        void                                            RegisterSerializationTypeConstructors( vaXMLSerializer & serializer );
    };
}