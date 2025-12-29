/* geometry.frag -- Fragment shader used for rendering in G-buffers
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/math.glsl"

/* === Varyings === */

flat in vec3 vEmission;
in vec2 vTexCoord;
in vec4 vColor;
in mat3 vTBN;

/* === Uniforms === */

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexNormal;
uniform sampler2D uTexEmission;
uniform sampler2D uTexORM;

uniform float uAlphaCutoff;
uniform float uNormalScale;
uniform float uOcclusion;
uniform float uRoughness;
uniform float uMetalness;

/* === Fragments === */

layout(location = 0) out vec3 FragAlbedo;
layout(location = 1) out vec3 FragEmission;
layout(location = 2) out vec2 FragNormal;
layout(location = 3) out vec3 FragORM;

/* === Main function === */

void main()
{
    // Sample material properties into globals that custom shaders can modify
    vec4 ALBEDO = vColor * texture(uTexAlbedo, vTexCoord);
    if (ALBEDO.a < uAlphaCutoff) discard;

    vec3 NORMAL = normalize(vTBN * M_NormalScale(texture(uTexNormal, vTexCoord).rgb * 2.0 - 1.0, uNormalScale));
    if (!gl_FrontFacing) NORMAL = -NORMAL; // Flip for back facing triangles with double sided meshes

    vec3 ORM = texture(uTexORM, vTexCoord).rgb;
    vec3 EMISSION = vEmission * texture(uTexEmission, vTexCoord).rgb;

#define R3D_USER_FRAGMENT_MARKER 0

    // Write to G-buffer
    FragAlbedo = ALBEDO.rgb;
    FragEmission = EMISSION;
    FragNormal = M_EncodeOctahedral(NORMAL);
    FragORM.r = uOcclusion * ORM.x;
    FragORM.g = uRoughness * ORM.y;
    FragORM.b = uMetalness * ORM.z;
}
