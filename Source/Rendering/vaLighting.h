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

#include "vaRendering.h"

#include "Rendering/vaRenderingIncludes.h"

#include "vaTexture.h"

#include "Rendering/Shaders/vaLightingShared.h"

#include "Core/vaUI.h"

#include "Rendering/vaRenderBuffers.h"

#include "Core/vaXMLSerialization.h"

#include "vaIBL.h"

namespace Vanilla
{
    class vaGBuffer;

    class vaShadowmap;

    // this isn't optimized for size/efficiency
    struct vaLight : public vaXMLSerializable, public vaUIPropertiesItem
    {
        // Following assimp light definitions (which seem to follow blender standards).
        // See aiLightSourceType
        enum class Type : int32
        {
            // If no other indirect lighting is used this can be a basic way to add some amount of always present generic light.
            // This light type doesn't have a valid position, direction, or other properties, just a color.
            // In practice there's no sense having more than one but if there's multiple ambient lights then they are just summed up together.
            // Unit is 'illuminance' in lux (lx) or lm/m^2 (I think? or is it just Luminance - cd/m^2? not sure)
            Ambient,

            // A directional light source has a well-defined direction but is infinitely far away. That's quite a good approximation for sun light.
            // Unit is 'illuminance' in lux (lx) or lm/m^2.
            Directional,

            // A point light source has a well-defined position in space but no direction - it emits light in all directions. A normal bulb is a point light.
            // Unit is luminous power (lm)
            Point,

            // A spot light source emits light in a specific angle. It has a position and a direction it is pointing to. A good example for a spot light is a light spot in sport arenas.
            // Treated exactly like 'Point' except for the falloff.
            // Unit is luminous power (lm)
            Spot,

            // [not actually implemented]
            // Same as spot but using photometric profile as a texture or similar.
            Photometric,

            // [not actually implemented]
            // An area light is a rectangle with predefined size that uniformly emits light from one of its sides. The position is center of the rectangle and direction is its normal vector.
            Area,

            MaxValue
        };


        Type                                            Type            = Type::Ambient;

        string                                          Name;

        vaVector3                                       Color           = vaVector3(0,0,0); // color of emitted light, as a linear RGB color. UI should expose it as an sRGB color or a color temperature
        float                                           Intensity       = 0.0f;             // brightness, with the actual unit depending on the type of light (see types above)

        vaVector3                                       Position        = vaVector3(0,0,0); // only used for point/spot lights during lighting, but used in all types for displaying UI icons
        vaVector3                                       Direction       = vaVector3(0,0,0); // only used for spot/directional lights
        vaVector3                                       Up              = vaVector3(0,0,0); // more of a placeholder for now (Up direction of the light source in space. Relative to the transformation of the node corresponding to the light.)

        float                                           Size            = 0;                // only for point/spot lights: in some ways these are considered spherical lights: this is the distance from which to start attenuating or compute umbra/penumbra/antumbra / compute specular (making this into a 'sphere' light) - useful to avoid near-infinities for when close-to-point lights
        float                                           Range           = 200.0f;           // warning: don't set to too high or shadow maps will not work; only for point/spot lights: max range at which they are effective regardless of other parameters; influences performance and shadow quality

        // appliedIntensity = light.Intensity * saturate( 1.0 - (angle - SpotInnerAngle) / (SpotOuterAngle - SpotInnerAngle) ) 
        float                                           SpotInnerAngle  = 0;                // angle from Direction below which the spot light has the full intensity (a.k.a. inner cone angle)
        float                                           SpotOuterAngle  = 0;                // angle from Direction below which the spot light intensity starts dropping (a.k.a. outer cone angle)

        // area light stuff only used for directional sunlight case at the moment - based on Filament implementation
        float                                           AngularRadius   = 0;                // angular radius of the sun in degrees; defaults to 0deg (disabled) or 0.545deg when enabled. The Sun as seen from Earth has an angular size of 0.526deg to 0.545deg
        float                                           HaloSize        = 10.0f;            // angular radius of the sun halo defined in multiples 
        float                                           HaloFalloff     = 80.0f;            // exponent used for the falloff of the sun halo (default is 80)

        bool                                            CastShadows     = false;
        bool                                            Enabled         = true;

        vaLight( ) {  }

        static vaLight                                  MakeAmbient( const string & name, const vaVector3 & color, const float intensity )     { vaLight ret; ret.Type = Type::Ambient;        ret.Name = name; ret.Color = color; ret.Intensity = intensity; return ret; }
        static vaLight                                  MakeDirectional( const string & name, const vaVector3 & color, const float intensity, const vaVector3 & direction )    
                                                                                                                            { vaLight ret; ret.Type = Type::Directional;    ret.Name = name; ret.Color = color; ret.Intensity = intensity; ret.Direction = direction; return ret; }
        static vaLight                                  MakeDirectionalSun( const string & name, const vaVector3 & color, const float intensity, const vaVector3 & direction, const float angularRadius = vaMath::DegreeToRadian(0.545f), float haloSize = 10.0f, float haloFalloff = 80.0f )
                                                                                                                            { vaLight ret; ret.Type = Type::Directional;    ret.Name = name; ret.Color = color; ret.Intensity = intensity; ret.Direction = direction; ret.AngularRadius = angularRadius; ret.HaloSize = haloSize; ret.HaloFalloff = haloFalloff; return ret; }
        static vaLight                                  MakePoint( const string & name, float size, const vaVector3 & color, const float intensity, const vaVector3 & position, const float range = 200.0f )
                                                                                                                            { vaLight ret; ret.Type = Type::Point;          ret.Size = size; ret.Name = name; ret.Color = color; ret.Intensity = intensity; ret.Position = position; ret.Range = range; return ret; }
        static vaLight                                  MakeSpot( const string & name, float size, const vaVector3 & color, const float intensity, const vaVector3 & position, const vaVector3 & direction, float innerAngle, float outerAngle, const float range = 200.0f )
                                                                                                                            { vaLight ret; ret.Type = Type::Spot;           ret.Size = size; ret.Name = name; ret.Color = color; ret.Intensity = intensity; ret.Direction = direction; ret.Position = position; ret.SpotInnerAngle = innerAngle; ret.SpotOuterAngle = outerAngle; ret.Range = range; return ret; }

        // // [TODO: add pre-exposure to this...] this actually depends on tonemapping exposure setting and will be different for diffuse & specular, but keep it simple for now for simplicity reasons
        // float                                           EffectiveRadius( float intensityThreshold = 1e-4f ) const           { float totalLight = Intensity; return vaMath::Sqrt( totalLight / intensityThreshold ); }

        void                                            CorrectLimits( );
        string                                          UIPropertiesItemGetDisplayName( ) const override    { return Name; }
        void                                            UIPropertiesItemTick( vaApplicationBase & application ) override;

        bool                                            Perceptible( ) const                                { return Enabled && (Intensity >= VA_EPSf) && !vaGeometry::NearEqual(Color, vaVector3::Zero); }

        bool                                            NearEqual( const vaLight & other );
        void                                            Reset( );

    protected:
        bool                                            Serialize( vaXMLSerializer & serializer ) override;
    };

    // Simple easy to use and control fog
    struct vaFogSphere : public vaXMLSerializable, public vaUIPropertiesItem
    {
        bool        Enabled             = false;
        bool        UseCustomCenter     = false;
        vaVector3   Center              = {0, 0, 0};    // set to camera position for regular fog
        vaVector3   Color               = {0, 0, 0.2f}; // fog color
        float       RadiusInner         = 1.0f;         // distance at which to start blending towards FogColor
        float       RadiusOuter         = 100.0f;       // distance at which to fully blend to FogColor
        float       BlendCurvePow       = 0.5f;         // fogK = pow( (distance - RadiusInner) / (RadiusOuter - RadiusInner), BlendCurvePow ) * BlendMultiplier
        float       BlendMultiplier     = 0.1f;         // fogK = pow( (distance - RadiusInner) / (RadiusOuter - RadiusInner), BlendCurvePow ) * BlendMultiplier

    public:
        void                                            CorrectLimits( );
        string                                          UIPropertiesItemGetDisplayName( ) const override { return "Fog Settings"; }
        void                                            UIPropertiesItemTick( vaApplicationBase & application ) override;

    protected:
        bool                                            Serialize( vaXMLSerializer & serializer ) override;
    };


    // A helper for GBuffer rendering - does creation and updating of render target textures and provides debug views of them.
    class vaLighting : public vaRenderingModule, public vaUIPanel, public std::enable_shared_from_this<vaLighting>
    {
    public:

    protected:
        string                                          m_debugInfo;

        // vaVector3                                       m_directionalLightDirection;
        // vaVector3                                       m_directionalLightIntensity;
        // vaVector3                                       m_ambientLightIntensity;

        vaFogSphere                                     m_fogSettings;

        shared_ptr<vaIBLProbe>                          m_localIBL                  = nullptr;
        shared_ptr<vaIBLProbe>                          m_distantIBL                = nullptr;

        //vaMatrix3x3                                     m_envmapRotation            = vaMatrix3x3::Identity;
        //float                                           m_envmapColorMultiplier     = 0.0f;
        //shared_ptr<vaTexture>                           m_envmapTexture;

        shared_ptr<vaTexture>                           m_AOTexture;

        vector<shared_ptr<vaLight>>                     m_lights;
        vector<shared_ptr<vaShadowmap>>                 m_shadowmaps;

        weak_ptr<vaLight>                               m_UI_SelectedLight;
        weak_ptr<vaShadowmap>                           m_UI_SelectedShadow;

        bool                                            m_shadowmapTexturesCreated  = false;
        static const int                                m_shadowCubeMapCount        = LightingShaderConstants::MaxShadowCubes;    // total supported at one time
        const int                                       m_shadowCubeResolution      = 3072; // one face resolution
        //vaResourceFormat                                m_shadowCubeDepthFormat     = vaResourceFormat::D16_UNORM;  // D32_FLOAT for more precision
        //vaResourceFormat                                m_shadowCubeFormat          = vaResourceFormat::R16_UNORM;  // R16_FLOAT not supported for compare sampler!!! not sure about R32_FLOAT?
        shared_ptr<vaTexture>                           m_shadowCubeArrayTexture;
        weak_ptr<vaShadowmap>                           m_shadowCubeArrayCurrentUsers[m_shadowCubeMapCount];
        //shared_ptr<vaTexture>                           m_shadowCubeDepthTexture;

        float                                           m_shadowCubeDepthBiasScale  = 1.2f;
        float                                           m_shadowCubeFilterKernelSize= 1.5f;

        vaTypedConstantBufferWrapper< LightingShaderConstants >
                                                        m_constantsBuffer;

//        shared_ptr<vaLight>

        // vaAutoRMI<vaPixelShader>                        m_applyDirectionalAmbientPS;
        // vaAutoRMI<vaPixelShader>                        m_applyDirectionalAmbientShadowedPS;
        // bool                                            m_shadersDirty              = true;
        // vector< pair< string, string > >                m_staticShaderMacros;
        // wstring                                         m_shaderFileToUse           = L"vaLighting.hlsl";

    protected:
        vaLighting( const vaRenderingModuleParams & params );
    public:
        virtual ~vaLighting( );

    private:
        friend class vaLightingDX11;

    protected:
        void                                            UpdateShaderConstants( vaSceneDrawContext & drawContext );
        //const shared_ptr<vaConstantBuffer> &            GetConstantsBuffer( ) const                                                             { return m_constantsBuffer.GetBuffer(); };
        //const shared_ptr<vaTexture> &                   GetEnvmapTexture( ) const                                                               { return m_envmapTexture; }
        //const shared_ptr<vaTexture> &                   GetShadowCubeArrayTexture( ) const                                                      { return m_shadowCubeArrayTexture; }

    public:
        void                                            UpdateAndSetToGlobals( vaSceneDrawContext & drawContext, vaShaderItemGlobals & shaderItemGlobals );

    public:
        vaFogSphere &                                   FogSettings( )                                                                          { return m_fogSettings; }

        // at the moment, update all lights every frame - this needs to be rethinked maybe
        void                                            SetLights( const vector<shared_ptr<vaLight>> & lights );
        const vector<shared_ptr<vaLight>> &             GetLights( )                                                                            { return m_lights; }

        void                                            SetLocalIBL( const shared_ptr<vaIBLProbe> & localIBL )                                  { m_localIBL = localIBL; }
        shared_ptr<vaIBLProbe>                          GetLocalIBL( ) const                                                                    { return m_localIBL; }
        void                                            SetDistantIBL( const shared_ptr<vaIBLProbe> & distantIBL )                              { m_distantIBL = distantIBL; }
        shared_ptr<vaIBLProbe>                          GetDistantIBL( ) const                                                                  { return m_distantIBL; }

        // void                                            SetEnvmap( const shared_ptr<vaTexture> & texture, const vaMatrix3x3 & rotation, float colorMultiplier ) { m_envmapTexture = texture; m_envmapRotation = rotation; m_envmapColorMultiplier = colorMultiplier; }
        // void                                            GetEnvmap( shared_ptr<vaTexture> & outTexture, vaMatrix3x3 & outRotation, float & outColorMultiplier )  { outTexture = m_envmapTexture; outRotation = m_envmapRotation; outColorMultiplier = m_envmapColorMultiplier; }

        void                                            SetAOMap( const shared_ptr<vaTexture> & texture )                                       { m_AOTexture = texture; }

        void                                            Tick( float deltaTime );

        // call vaShadowmap::SetUpToDate() to make 'fresh' - 'fresh' ones will not get returned by this function, and if there's no dirty ones left it will return nullptr
        shared_ptr<vaShadowmap>                         GetNextHighestPriorityShadowmapForRendering( );

        // vaVector4                                       GetShadowCubeViewspaceDepthOffsets( ) const                                             { return vaVector4( m_shadowCubeFlatOffsetAdd / (float)m_shadowCubeResolution, m_shadowCubeFlatOffsetScale / (float)m_shadowCubeResolution, m_shadowCubeSlopeOffsetAdd / (float)m_shadowCubeResolution, m_shadowCubeSlopeOffsetScale / (float)m_shadowCubeResolution ); }

    protected:
        void                                            DestroyShadowmapTextures( );
        void                                            CreateShadowmapTextures( );
        shared_ptr<vaShadowmap>                         FindShadowmapForLight( const shared_ptr<vaLight> & light );

    protected:
        // virtual void                                    UpdateResourcesIfNeeded( vaSceneDrawContext & drawContext );
        // virtual void                                    ApplyDirectionalAmbientLighting( vaSceneDrawContext & drawContext, vaGBuffer & GBuffer ) = 0;
        // virtual void                                    ApplyDynamicLighting( vaSceneDrawContext & drawContext, vaGBuffer & GBuffer ) = 0;

    public:
        // could add a 'deallocate' if vaShadowmap wants to detach for any reason, but not sure that's needed - they get destroyed anyway when not needed and that removes them from this list
        bool                                            AllocateShadowStorageTextureIndex( const shared_ptr<vaShadowmap> & shadowmap, int & outTextureIndex, shared_ptr<vaTexture> & outTextureArray );

        // // if using cubemap shadows this is one depth buffer used for rendering all of them
        // const shared_ptr<vaTexture> &                   GetCubemapDepthTexture( )                                              { return m_shadowCubeDepthTexture; }

    private:
        virtual void                                    UIPanelTick( vaApplicationBase & application ) override;
    };

    class vaShadowmap : public vaRenderingModule, public vaUIPanel, public vaUIPropertiesItem, public std::enable_shared_from_this<vaShadowmap>
    {
    protected:
        const weak_ptr<vaLighting>                      m_lightingSystem;
        const weak_ptr<vaLight>                         m_light;
        int                                             m_storageTextureIndex = -1;  // index to vaLighting's texture storage index (into cubemap array for point lights, csm array for directional, etc.)

        // state of the light when the shadowmap was drawn last time; updated on Tick
        vaLight                                         m_lastLightState;

        // if covering dynamic objects then use age to determine which shadow map to update first; for static-only shadows there's no need to update except if light parameters changed
        bool                                            m_includeDynamicObjects = false;
        float                                           m_dataAge = VA_FLOAT_HIGHEST;

    protected:
        vaShadowmap( ) = delete;
        vaShadowmap( vaRenderDevice & device, const shared_ptr<vaLighting> & lightingSystem, const shared_ptr<vaLight> & light ) : vaRenderingModule( vaRenderingModuleParams(device) ), vaUIPanel("SM", 0, false, vaUIPanel::DockLocation::DockedLeftBottom, "ShadowMaps" ), m_lightingSystem( lightingSystem ), m_light( light ) { }

    public:
        virtual ~vaShadowmap( ) { }

    public:
        const weak_ptr<vaLighting> &                    GetLighting( ) const { return m_lightingSystem; }
        const weak_ptr<vaLight> &                       GetLight( ) const { return m_light; }
        int                                             GetStorageTextureIndex( ) const { return m_storageTextureIndex; }
        const float                                     GetDataAge( ) const { return m_dataAge; }

        const vaLight &                                 GetLastLightState( ) const { return m_lastLightState; }

        static shared_ptr<vaShadowmap>                  Create( vaRenderDevice & device, const shared_ptr<vaLight> & light, const shared_ptr<vaLighting> & lightingSystem );

        virtual void                                    Tick( float deltaTime );

        void                                            Invalidate( )                                       { m_lastLightState.Reset( ); }
        void                                            SetUpToDate( )                                      { m_dataAge = 0; }
        void                                            SetIncludeDynamicObjects( bool includeDynamic )     { m_includeDynamicObjects = includeDynamic; }

        // create draw filter
        virtual void                                    SetToRenderSelectionFilter( vaRenderSelection::FilterSettings & filter ) const = 0 ;
        // draw
        virtual vaDrawResultFlags                       Draw( vaRenderDeviceContext & renderContext, vaRenderSelection & renderSelection ) = 0;

        virtual string                                  UIPropertiesItemGetDisplayName( ) const override    { return UIPanelGetDisplayName(); }
        virtual void                                    UIPropertiesItemTick( vaApplicationBase & ) override                    { assert( false ); } //return UIPanelTick(); }
    };

    // for point/spot lights
    class vaCubeShadowmap : public vaShadowmap
    {
    private:
        std::shared_ptr<vaTexture>                      m_cubemapArraySRV;      // array SRV into the big cube texture pointing to the beginning and of size of 6
        //std::shared_ptr<vaTexture>                      m_cubemapArrayRTVs[6];  // RTVs each pointing at the beginning + n (n goes from 0 to 5)
        std::shared_ptr<vaTexture>                      m_cubemapSliceDSVs[6];  // temp DSVs used to render the cubemap

    protected:
        friend class vaShadowmap;
        vaCubeShadowmap( ) = delete;
        vaCubeShadowmap( vaRenderDevice & device, const shared_ptr<vaLighting> & lightingSystem, const shared_ptr<vaLight> & light );

    public:
        virtual ~vaCubeShadowmap( ) { }

    protected:
        virtual void                                    SetToRenderSelectionFilter( vaRenderSelection::FilterSettings & filter ) const;
        virtual vaDrawResultFlags                       Draw( vaRenderDeviceContext & renderContext, vaRenderSelection & renderSelection );
        virtual void                                    Tick( float deltaTime );

    protected:
        virtual string                                  UIPanelGetDisplayName( ) const override { return vaStringTools::Format( "Cubemap [%s]", m_lastLightState.Name.c_str() ); }
        virtual void                                    UIPanelTick( vaApplicationBase & application ) override;
    };

    
    inline bool vaLight::NearEqual( const vaLight & other )
    {
        // ignores the type when doing data comparisons, so make sure to copy all contents if you want this to ever return positive (or add branching below)

        if( Type != other.Type ) return false;
        if( !vaGeometry::NearEqual( Color, other.Color ) )                      return false;
        if( !vaGeometry::NearEqual( Intensity, other.Intensity ) )              return false;
        if( !vaGeometry::NearEqual( Position, other.Position ) )                return false;
        if( !vaGeometry::NearEqual( Direction, other.Direction ) )              return false;
        if( !vaGeometry::NearEqual( Up, other.Up ) )                            return false;
        if( !vaGeometry::NearEqual( Size, other.Size ) )                        return false;
        if( !vaGeometry::NearEqual( Range, other.Range ) )                      return false;
        if( !vaGeometry::NearEqual( SpotInnerAngle, other.SpotInnerAngle ) )    return false;
        if( !vaGeometry::NearEqual( SpotOuterAngle, other.SpotOuterAngle ) )    return false;
        if( !vaGeometry::NearEqual( AngularRadius, other.AngularRadius ) )      return false;
        if( !vaGeometry::NearEqual( HaloSize     , other.HaloSize      ) )      return false;
        if( !vaGeometry::NearEqual( HaloFalloff  , other.HaloFalloff   ) )      return false;
        if( CastShadows != other.CastShadows )                                  return false;
        if( Enabled != other.Enabled )                                          return false;

        return true;
    }

}