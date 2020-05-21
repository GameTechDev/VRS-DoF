struct ShadingParams
{
    highp float3x3  TangentToWorld; // TBN matrix
    vec3        Position;           // position of the fragment in world space - same as vertex.WorldspacePos
    float3      View;               // normalized vector from the fragment to the eye
    float3      Normal;             // normalized transformed normal, in world space (if normalmap exists, this is a perturbed normal)
    float3      GeometricNormal;    // normalized geometric normal, in world space
    float3      Reflected;          // reflection of view about normal
    float       NoV;                // dot(normal, view), always strictly >= MIN_N_DOT_V

#if defined(MATERIAL_HAS_CLEAR_COAT)
    float3      ClearCoatNormal;    // normalized clear coat layer normal, in world space
#endif

#if defined(MATERIAL_HAS_EMISSIVE)
    float3      PrecomputedEmissive;
#endif

    float       Noise;
    float       NoiseAttenuation;   // noise function is not that great at handling anisotropy / glancing angles so it provides this value to attenuate anything using it if needed

#if VA_RM_ACCEPTSHADOWS
    CubeShadowsParams
                CubeShadows;
#endif

    IBLParams   IBL;

    float       CombinedAmbientOcclusion;       // min( material.AmbientOcclusion, SSAO )

    //float3      GetWorldGeometricNormalVector( ) { return TangentToWorld[2]; }
    float3      GetWorldGeometricNormalVector( ) { return GeometricNormal; }
};