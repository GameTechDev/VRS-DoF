#if defined(MATERIAL_HAS_CLEAR_COAT)
float clearCoatLobe( const ShadingParams shading, const PixelParams pixel, const vec3 h, float NoH, float LoH, out float Fcc ) {

#if defined(MATERIAL_HAS_NORMAL) || defined(MATERIAL_HAS_CLEAR_COAT_NORMAL)
    // If the material has a normal map, we want to use the geometric normal
    // instead to avoid applying the normal map details to the clear coat layer
    float clearCoatNoH = saturate(dot(shading.ClearCoatNormal, h));
#else
    float clearCoatNoH = NoH;
#endif

    // clear coat specular lobe
    float D = distributionClearCoat(pixel.ClearCoatRoughness, clearCoatNoH, h);
    float V = visibilityClearCoat(LoH);
    float F = F_Schlick(0.04, 1.0, LoH) * pixel.ClearCoat; // fix IOR to 1.5

    Fcc = F;
    return D * V * F;
}
#endif

#if defined(MATERIAL_HAS_ANISOTROPY)
vec3 anisotropicLobe(const ShadingParams shading, const PixelParams pixel, const LightParams light, const vec3 h,
        float NoV, float NoL, float NoH, float LoH) {

    vec3 l = light.L;
    vec3 t = pixel.AnisotropicT;
    vec3 b = pixel.AnisotropicB;
    vec3 v = shading.View;

    float ToV = dot(t, v);
    float BoV = dot(b, v);
    float ToL = dot(t, l);
    float BoL = dot(b, l);
    float ToH = dot(t, h);
    float BoH = dot(b, h);

    // Anisotropic parameters: at and ab are the roughness along the tangent and bitangent
    // to simplify materials, we derive them from a single roughness parameter
    // Kulla 2017, "Revisiting Physically Based Shading at Imageworks"
    float at = max(pixel.Roughness * (1.0 + pixel.Anisotropy), MIN_ROUGHNESS);
    float ab = max(pixel.Roughness * (1.0 - pixel.Anisotropy), MIN_ROUGHNESS);

    // specular anisotropic BRDF
    float D = distributionAnisotropic(at, ab, ToH, BoH, NoH);
    float V = visibilityAnisotropic(pixel.Roughness, at, ab, ToV, BoV, ToL, BoL, NoV, NoL);
    vec3  F = fresnel(pixel.F0, LoH);

    return (D * V) * F;
}
#endif

vec3 isotropicLobe(const PixelParams pixel, const LightParams light, const vec3 h,
        float NoV, float NoL, float NoH, float LoH) {

    float D = distribution(pixel.Roughness, NoH, h);
    float V = visibility(pixel.Roughness, NoV, NoL);
    vec3  F = fresnel(pixel.F0, LoH);

    return (D * V) * F;
}

vec3 specularLobe( const ShadingParams shading, const PixelParams pixel, const LightParams light, const vec3 h, float NoV, float NoL, float NoH, float LoH) 
{
#if defined(MATERIAL_HAS_ANISOTROPY)
    return anisotropicLobe(shading, pixel, light, h, NoV, NoL, NoH, LoH);
#else
    return isotropicLobe(pixel, light, h, NoV, NoL, NoH, LoH);
#endif
}

vec3 diffuseLobe(const PixelParams pixel, float NoV, float NoL, float LoH) {
    return pixel.DiffuseColor * diffuse(pixel.Roughness, NoV, NoL, LoH);
}

/**
 * Evaluates lit materials with the standard shading model. This model comprises
 * of 2 BRDFs: an optional clear coat BRDF, and a regular surface BRDF.
 *
 * Surface BRDF
 * The surface BRDF uses a diffuse lobe and a specular lobe to render both
 * dielectrics and conductors. The specular lobe is based on the Cook-Torrance
 * micro-facet model (see brdf.fs for more details). In addition, the specular
 * can be either isotropic or anisotropic.
 *
 * Clear coat BRDF
 * The clear coat BRDF simulates a transparent, absorbing dielectric layer on
 * top of the surface. Its IOR is set to 1.5 (polyutherane) to simplify
 * our computations. This BRDF only contains a specular lobe and while based
 * on the Cook-Torrance microfacet model, it uses cheaper terms than the surface
 * BRDF's specular lobe (see brdf.fs).
 */
void surfaceShading( const ShadingParams shading, const PixelParams pixel, const LightParams light, float occlusion, inout float3 inoutDiffuseColor, inout float3 inoutSpecularColor ) 
{
    vec3 h = normalize(shading.View + light.L);

    float NoV = shading.NoV;
    float NoL = saturate(light.NoL);
    float NoH = saturate(dot(shading.Normal, h));
    float LoH = saturate(dot(light.L, h));

    vec3 Fr = specularLobe( shading, pixel, light, h, NoV, NoL, NoH, LoH );
    vec3 Fd = diffuseLobe( pixel, NoV, NoL, LoH );
#if defined(HAS_REFRACTION)
    Fd *= (1.0 - pixel.Transmission);
#endif

    // TODO: attenuate the diffuse lobe to avoid energy gain

#if defined(MATERIAL_HAS_CLEAR_COAT)
    float Fcc;
    float clearCoat = clearCoatLobe(pixel, h, NoH, LoH, Fcc);
    float attenuation = 1.0 - Fcc;

#if defined(MATERIAL_HAS_NORMAL) || defined(MATERIAL_HAS_CLEAR_COAT_NORMAL)
    // vec3 color = (Fd + Fr * pixel.EnergyCompensation) * attenuation * NoL;
    float3 diffuseColor    = (Fd) * attenuation * NoL;
    float3 specularColor   = (Fr * pixel.EnergyCompensation) * attenuation * NoL;

    // If the material has a normal map, we want to use the geometric normal
    // instead to avoid applying the normal map details to the clear coat layer
    float clearCoatNoL = saturate(dot(shading.ClearCoatNormal, light.L));
    // color += clearCoat * clearCoatNoL;
    specularColor += clearCoat * clearCoatNoL;

    // Early exit to avoid the extra multiplication by NoL
//    return (color * light.ColorIntensity.rgb) * (light.ColorIntensity.w * light.Attenuation * occlusion);
    float3 lightColor = (light.ColorIntensity.rgb) * (light.ColorIntensity.w * light.Attenuation * occlusion);
    inoutDiffuseColor  += lightColor * diffuseColor;
    inoutSpecularColor += lightColor * specularColor;
    return;
#else
    //vec3 color    = (Fd + Fr * pixel.EnergyCompensation) * attenuation + clearCoat;
    float3 diffuseColor    = (Fd) * attenuation;
    float3 specularColor   = (Fr * pixel.EnergyCompensation) * attenuation + clearCoat;
#endif
#else
    // The energy compensation term is used to counteract the darkening effect
    // at high roughness
    //vec3 color = Fd + Fr * pixel.EnergyCompensation;
    float3 diffuseColor    = Fd;
    float3 specularColor   = Fr * pixel.EnergyCompensation;
#endif

    //color = (color * light.ColorIntensity.rgb) * (light.ColorIntensity.w * light.Attenuation * NoL * occlusion);
    float3 lightColor = (light.ColorIntensity.rgb) * (light.ColorIntensity.w * light.Attenuation * NoL * occlusion);
    inoutDiffuseColor   += diffuseColor  * lightColor;
    inoutSpecularColor  += specularColor * lightColor;
}
