// moved to vaLighting.hlsl
//
// struct LightParams 
// {
//     float4  ColorIntensity;     // rgb, pre-exposed intensity
//     float3  L;                  // light direction (viewspace)
//     float   Attenuation;
//     float   NoL;
// #if VA_RM_SPECIAL_EMISSIVE_LIGHT
//     bool    AddToEmissive;
// #endif
// };

struct PixelParams 
{
    float3  DiffuseColor;
    float   PerceptualRoughness;
    float   PerceptualRoughnessUnclamped;
    float3  F0;                         // Reflectance at normal incidence
    float   Roughness;
    float3  DFG;
    float3  EnergyCompensation;

#if defined(MATERIAL_HAS_CLEAR_COAT)
    float   ClearCoat;
    float   ClearCoatPerceptualRoughness;
    float   ClearCoatRoughness;
#endif

#if defined(MATERIAL_HAS_ANISOTROPY)
    float3  AnisotropicT;
    float3  AnisotropicB;
    float   Anisotropy;
#endif

#if defined(SHADING_MODEL_SUBSURFACE) || defined(HAS_REFRACTION)
    float   Thickness;
#endif
#if defined(SHADING_MODEL_SUBSURFACE)
    float3  SubsurfaceColor;
    float   SubsurfacePower;
#endif

#if defined(SHADING_MODEL_CLOTH) && defined(MATERIAL_HAS_SUBSURFACE_COLOR)
    float3  SubsurfaceColor;
#endif

#if defined(HAS_REFRACTION)
    float EtaRI;
    float EtaIR;
    float Transmission;
    float uThickness;
    vec3  Absorption;
#endif
};

float computeMicroShadowing(float NoL, float visibility) {
    // Chan 2018, "Material Advances in Call of Duty: WWII" -> http://advances.realtimerendering.com/other/2016/naughty_dog/NaughtyDog_TechArt_Final.pdf (microshadowing)
    float aperture = inversesqrt(1.0000001 - visibility);
    float microShadow = saturate(NoL * aperture);
    return microShadow * microShadow;
}
