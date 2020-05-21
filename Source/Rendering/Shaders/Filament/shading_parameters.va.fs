
// This provides functionality of Filament 'computeShadingParams' and 'prepareMaterial' in Vanilla; see shading_parameters.fs for 
// original code & comments

ShadingParams ComputeShadingParams( const RenderMaterialInterpolants vertex, const MaterialInputs material, const bool isFrontFace )
{
    ShadingParams shading;

    shading.Position        = vertex.WorldspacePos.xyz;

    Noise3D( vertex.WorldspacePos.xyz, shading.Noise, shading.NoiseAttenuation );

    float3 worldspaceNormal = normalize( vertex.WorldspaceNormal.xyz );

    // if viewing back face, invert the normal to match
    float frontFaceSign = (isFrontFace)?(1.0):(-1.0);

    worldspaceNormal     *= frontFaceSign;

    shading.GeometricNormal = worldspaceNormal;

#if defined(MATERIAL_HAS_ANISOTROPY) || defined(MATERIAL_HAS_NORMAL) || defined(MATERIAL_HAS_CLEAR_COAT_NORMAL)
    // original code:
    // // We use unnormalized post-interpolation values, assuming mikktspace tangents
    // shading_tangentToWorld = mat3(t, b, n);
    // compute (co)tangent frame (since we don't have a good path for vertex cotangent space)
    shading.TangentToWorld = ComputeCotangentFrame( worldspaceNormal, vertex.WorldspacePos.xyz, frontFaceSign * vertex.Texcoord01.xy );
    // this should not be needed but the above step does not "just work" on our broken assets so use this...
    ReOrthogonalizeFrame( shading.TangentToWorld, true );
#else
    // Leave the tangent and bitangent uninitialized, we won't use them
    shading.TangentToWorld[2] = worldspaceNormal;
#endif

#if 0
    if( saturate(abs(length(shading.TangentToWorld[0]) - 1)-0.1) != 0 )
        ComputeOrthonormalBasis( worldspaceNormal, shading.TangentToWorld[0], shading.TangentToWorld[1] );
#endif

    // shading_position = vertex_worldPosition;
    shading.View = normalize( g_Global.CameraWorldPosition.xyz - vertex.WorldspacePos.xyz );   // normalized vector from the fragment to the eye


//#if defined(HAS_ATTRIBUTE_TANGENTS)
#if defined(MATERIAL_HAS_NORMAL)
    shading.Normal = normalize( mul( material.Normal, shading.TangentToWorld ) );// normalize(shading_tangentToWorld * material.normal);
#else
    shading.Normal = shading.GetWorldGeometricNormalVector();
#endif
    shading.NoV         = clampNoV( dot( shading.Normal, shading.View ) );
    shading.Reflected   = reflect( -shading.View, shading.Normal );

#if defined(MATERIAL_HAS_CLEAR_COAT)
#if defined(MATERIAL_HAS_CLEAR_COAT_NORMAL)
    shading.ClearCoatNormal = normalize( mul( material.ClearCoatNormal, shading.TangentToWorld ) ); //normalize(shading_tangentToWorld * material.clearCoatNormal);
#else
    shading.ClearCoatNormal = shading.GetWorldGeometricNormalVector();
#endif
#endif
//#endif

#if defined(MATERIAL_HAS_EMISSIVE)
    // the way filament does it:
    // float emissiveIntensity = pow( 2.0, g_Global.EV100 + material.EmissiveIntensity - 3.0) * g_Global.PreExposureMultiplier;
    
    // vanilla does it a bit simpler - also clamp values below 0 and don't do anything in case of VA_RM_SPECIAL_EMISSIVE_LIGHT
    float emissiveIntensity = max( 0, material.EmissiveIntensity ) * g_Global.PreExposureMultiplier; // pow( 2.0, g_Global.EV100 + material.EmissiveIntensity - 3.0) * g_Global.PreExposureMultiplier;
    shading.PrecomputedEmissive = material.EmissiveColor * material.EmissiveIntensity.xxx;
#endif

#if VA_RM_ACCEPTSHADOWS
    shading.CubeShadows = ComputeCubeShadowsParams( shading.Noise, shading.NoiseAttenuation, vertex.WorldspacePos.xyz );
#endif

    shading.IBL         = ComputeIBLParams( shading.Noise, shading.NoiseAttenuation, vertex.WorldspacePos.xyz, shading.GeometricNormal );

    shading.CombinedAmbientOcclusion = material.AmbientOcclusion;

#if !VA_RM_TRANSPARENT || VA_RM_DECAL   // only sample SSAO if we're not transparent, unless we're a decal (in which case we lay on opaque surface by definition, so SSAO is correct)
    shading.CombinedAmbientOcclusion = min( shading.CombinedAmbientOcclusion, SampleAO( vertex.Position.xy ) );
#endif

    // // if we wanted to update the TangentToWorld (cotangent frame) with the normal loaded from normalmap, we can do this:
    // shading.TangentToWorld[2] = shading.Normal;
    // ReOrthogonalizeFrame( cotangentFrame, false );

    return shading;
}
