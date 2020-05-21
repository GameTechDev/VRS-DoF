//------------------------------------------------------------------------------
// Punctual lights evaluation
//------------------------------------------------------------------------------

#if 0
// Make sure this matches the same constants in Froxel.cpp
#define FROXEL_BUFFER_WIDTH_SHIFT   6u
#define FROXEL_BUFFER_WIDTH         (1u << FROXEL_BUFFER_WIDTH_SHIFT)
#define FROXEL_BUFFER_WIDTH_MASK    (FROXEL_BUFFER_WIDTH - 1u)

#define RECORD_BUFFER_WIDTH_SHIFT   5u
#define RECORD_BUFFER_WIDTH         (1u << RECORD_BUFFER_WIDTH_SHIFT)
#define RECORD_BUFFER_WIDTH_MASK    (RECORD_BUFFER_WIDTH - 1u)

struct FroxelParams {
    uint recordOffset; // offset at which the list of lights for this froxel starts
    uint pointCount;   // number of point lights in this froxel
    uint spotCount;    // number of spot lights in this froxel
};

/**
 * Returns the coordinates of the froxel at the specified fragment coordinates.
 * The coordinates are a 3D position in the froxel grid.
 */
uvec3 getFroxelCoords(const vec3 fragCoords) {
    uvec3 froxelCoord;

    vec3 adjustedFragCoords = fragCoords;
// In Vulkan and Metal, texture coords are Y-down. In OpenGL, texture coords are Y-up.
#if defined(TARGET_METAL_ENVIRONMENT) || defined(TARGET_VULKAN_ENVIRONMENT)
    adjustedFragCoords.y = frameUniforms.resolution.y - adjustedFragCoords.y;
#endif

    froxelCoord.xy = uvec2((adjustedFragCoords.xy - frameUniforms.origin.xy) *
            vec2(frameUniforms.oneOverFroxelDimension, frameUniforms.oneOverFroxelDimensionY));

    froxelCoord.z = uint(max(0.0,
            log2(frameUniforms.zParams.x * adjustedFragCoords.z + frameUniforms.zParams.y) *
                    frameUniforms.zParams.z + frameUniforms.zParams.w));

    return froxelCoord;
}

/**
 * Computes the froxel index of the fragment at the specified coordinates.
 * The froxel index is computed from the 3D coordinates of the froxel in the
 * froxel grid and later used to fetch from the froxel data texture
 * (light_froxels).
 */
uint getFroxelIndex(const vec3 fragCoords) {
    uvec3 froxelCoord = getFroxelCoords(fragCoords);
    return froxelCoord.x * frameUniforms.fParamsX +
           froxelCoord.y * frameUniforms.fParams.x +
           froxelCoord.z * frameUniforms.fParams.y;
}

/**
 * Computes the texture coordinates of the froxel data given a froxel index.
 */
ivec2 getFroxelTexCoord(uint froxelIndex) {
    return ivec2(froxelIndex & FROXEL_BUFFER_WIDTH_MASK, froxelIndex >> FROXEL_BUFFER_WIDTH_SHIFT);
}

/**
 * Returns the froxel data for the given froxel index. The data is fetched
 * from the light_froxels texture.
 */
FroxelParams getFroxelParams(uint froxelIndex) {
    ivec2 texCoord = getFroxelTexCoord(froxelIndex);
    uvec2 entry = texelFetch(light_froxels, texCoord, 0).rg;

    FroxelParams froxel;
    froxel.recordOffset = entry.r;
    froxel.pointCount = entry.g & 0xFFu;
    froxel.spotCount = entry.g >> 8u;
    return froxel;
}

/**
 * Returns the coordinates of the light record in the light_records texture
 * given the specified index. A light record is a single uint index into the
 * lights data buffer (lightsUniforms UBO).
 */
ivec2 getRecordTexCoord(uint index) {
    return ivec2(index & RECORD_BUFFER_WIDTH_MASK, index >> RECORD_BUFFER_WIDTH_SHIFT);
}
#endif


float getSquareFalloffAttenuation( float distanceSquare, float range )
{
    // falloff looks like this: https://www.desmos.com/calculator/ej4tpxmaco (for factor^2) - if link doesn't work set "k" to factor and graph to "\max\left(0,\ \min\left(1,\ 1-\left(x^2\cdot k\right)^3\right)\right)^2" 
    float falloff = 1.0 / (range * range);
    float factor = distanceSquare * falloff;
    float smoothFactor = saturate(1.0 - factor * factor * factor); // modded to factor^3 instead of factor^2 for a bit sharper falloff
    // We would normally divide by the square distance here
    // but we do it at the call site
    return smoothFactor * smoothFactor;
}

float getDistanceAttenuation( const float distanceSquare, const float range, const float size )
{
    float attenuation = getSquareFalloffAttenuation( distanceSquare, range );
    // Assume a punctual light occupies a volume of 1cm to avoid a division by 0
    return attenuation * 1.0 / max( distanceSquare, size*size );
}

float getAngleAttenuation( const vec3 lightDir, const vec3 l, const vec2 scaleOffset )
{
    float cd = dot(lightDir, l);
    float attenuation  = saturate(cd * scaleOffset.x + scaleOffset.y);
    return attenuation * attenuation;
}

/**
 * Light setup common to point and spot light. This function sets the light vector
 * "l" and the attenuation factor in the Light structure. The attenuation factor
 * can be partial: it only takes distance attenuation into account. Spot lights
 * must compute an additional angle attenuation.
 */
LightParams setupPunctualLight( const RenderMaterialInterpolants vertex, const ShadingParams shading, const ShaderLightSpot lightIn )//, const highp vec4 positionFalloff )
{
    LightParams light;

    highp float3 worldPosition = vertex.WorldspacePos.xyz;
    highp float3 posToLight = lightIn.Position - worldPosition;
    float distanceSquare = dot( posToLight, posToLight );
    light.Dist = sqrt( distanceSquare );
    light.L = posToLight / light.Dist; // normalize(posToLight); // TODO: just use rsq(distanceSquare)*posToLight
    light.Attenuation = getDistanceAttenuation( distanceSquare, lightIn.Range, lightIn.Size );
    light.NoL = saturate( dot( shading.Normal, light.L ) );

#if VA_RM_SPECIAL_EMISSIVE_LIGHT
    // Only add emissive within light sphere, and then scale with light itself; this is to allow emissive materials to be 'controlled' by
    // the light - useful for models that represent light emitters (lamps, etc.)
    light.AddToEmissive = light.Dist < (lightIn.Size);
#else
    light.AddToEmissive = false;
#endif
    light.ColorIntensity    = float4( lightIn.Color, lightIn.Intensity );
//    light.colorIntensity.rgb = colorIntensity.rgb;
//    light.colorIntensity.w = computePreExposedIntensity( colorIntensity.w, frameUniforms.exposure );

    return light;
}

/**
 * Returns a Light structure (see common_lighting.fs) describing a spot light.
 * The colorIntensity field will store the *pre-exposed* intensity of the light
 * in the w component.
 *
 * The light parameters used to compute the Light structure are fetched from the
 * lightsUniforms uniform buffer.
 */
LightParams getSpotLight( const RenderMaterialInterpolants vertex, const ShadingParams shading, const uint lightIndex )//uint index ) 
{
    const ShaderLightSpot lightIn = g_Lighting.LightsSpotAndPoint[lightIndex];

    LightParams light = setupPunctualLight( vertex, shading, lightIn );

    // ivec2 texCoord = getRecordTexCoord(index);
    // uint lightIndex = texelFetch(light_records, texCoord, 0).r;
    // 
    // highp vec4 positionFalloff = lightsUniforms.lights[lightIndex][0];
    // highp vec4 colorIntensity  = lightsUniforms.lights[lightIndex][1];
    //       vec4 directionIES    = lightsUniforms.lights[lightIndex][2];
    //       vec2 scaleOffset     = lightsUniforms.lights[lightIndex][3].xy;

#if defined(MATERIAL_CAN_SKIP_LIGHTING)
    [branch]
    if (light.NoL <= 0.0) 
    {
        light.Attenuation = 0;
        return light;
    }
    else
#endif
    {

    // light.attenuation *= getAngleAttenuation( -directionIES.xyz, light.l, scaleOffset );
    // use Vanilla attenuation approach for now: 
    //[branch]
    //if( i < g_Lighting.LightCountSpotOnly )
    {
        // TODO: remove acos, switch to dot-product based one like filament? this must be slow.
        float angle = acos( dot( lightIn.Direction, -light.L ) );
        float spotAttenuation = saturate( (lightIn.SpotOuterAngle - angle) / (lightIn.SpotOuterAngle - lightIn.SpotInnerAngle) );
        // squaring of spot attenuation is just for a softer outer->inner curve that I like more visually
        light.Attenuation *= spotAttenuation*spotAttenuation;
    }

#if VA_RM_ACCEPTSHADOWS
    [branch]
    if( light.Attenuation > 0 && lightIn.CubeShadowIndex >= 0 )
        light.Attenuation *= ComputeCubeShadow( shading.CubeShadows, shading.GetWorldGeometricNormalVector(), lightIn.CubeShadowIndex, light.L, light.Dist, lightIn.Size, lightIn.Range );
#endif

    return light;
    }
}

/**
 * Evaluates all punctual lights that my affect the current fragment.
 * The result of the lighting computations is accumulated in the color
 * parameter, as linear HDR RGB.
 */
void evaluatePunctualLights( const RenderMaterialInterpolants vertex, const ShadingParams shading, const PixelParams pixel, inout float3 diffuseColor, inout float3 specularColor )
{
    // Fetch the light information stored in the froxel that contains the
    // current fragment
    // FroxelParams froxel = getFroxelParams(getFroxelIndex(gl_FragCoord.xyz));

    // Each froxel contains how many point and spot lights can influence
    // the current fragment. A froxel also contains a record offset that
    // tells us where the indices of those lights are in the records
    // texture. The records texture contains the indices of the actual
    // light data in the lightsUniforms uniform buffer
    // 
    // uint index = froxel.recordOffset;
    // uint end = index + froxel.pointCount;

    // Iterate point lights
    // for ( ; index < end; index++) 
    // {
    for( uint i = 0; i < g_Lighting.LightCountSpotAndPoint; i++ )
    {
        // Light light = getPointLight(index);
        LightParams light = getSpotLight( vertex, shading, i );

        // this is only a property of punctual lights: add it directly to diffuse here!
        SpecialEmissiveLight( shading, light, diffuseColor );

        [branch]
        if( light.Attenuation > 0 )
        {
            // maybe not the best place to apply this         
            light.Attenuation *= computeMicroShadowing( light.NoL, shading.CombinedAmbientOcclusion );
            
            surfaceShading( shading, pixel, light, 1.0, diffuseColor, specularColor );
        }

// #if defined(MATERIAL_CAN_SKIP_LIGHTING)
//         if (light.NoL > 0.0) 
//         {
//             surfaceShading( shading, pixel, light, 1.0, diffuseColor, specularColor );
//         }
// #else
//         surfaceShading( shading, pixel, light, 1.0, diffuseColor, specularColor );
// #endif
    }

//    end += froxel.spotCount;
//
//     // Iterate spotlights
//     for ( ; index < end; index++) {
//         Light light = getSpotLight(index);
// #if defined(MATERIAL_CAN_SKIP_LIGHTING)
//         if (light.NoL > 0.0) {
//             color.rgb += surfaceShading(pixel, light, 1.0);
//         }
// #else
//         color.rgb += surfaceShading(pixel, light, 1.0);
// #endif
//     }
}
