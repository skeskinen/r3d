/* r3d_shader.h -- R3D Custom Shader Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_SHADER_H
#define R3D_SHADER_H

#include "./r3d_platform.h"
#include <raylib.h>
#include <stdint.h>

// Forward declaration to avoid circular include
typedef struct R3D_Material R3D_Material;

/**
 * @defgroup CustomShader Custom Shaders
 * @{
 */

// ========================================
// DEFINES
// ========================================

#define R3D_MAX_CUSTOM_UNIFORMS     16
#define R3D_MAX_UNIFORM_NAME_LENGTH 64

// ========================================
// ENUMS
// ========================================

/**
 * @brief Type of a custom shader uniform parameter.
 */
typedef enum R3D_ParamType {
    R3D_PARAM_FLOAT,    ///< Single float value
    R3D_PARAM_VEC2,     ///< 2-component vector
    R3D_PARAM_VEC3,     ///< 3-component vector
    R3D_PARAM_VEC4,     ///< 4-component vector
    R3D_PARAM_TEX2D     ///< 2D texture sampler
} R3D_ParamType;

// ========================================
// STRUCTS
// ========================================

/**
 * @brief Information about a custom uniform in a shader.
 */
typedef struct R3D_UniformInfo {
    char name[R3D_MAX_UNIFORM_NAME_LENGTH];  ///< Uniform name
    int location;                             ///< OpenGL uniform location
    R3D_ParamType type;                       ///< Uniform type
    int texSlot;                              ///< For samplers: assigned texture slot (5+)
} R3D_UniformInfo;

/**
 * @brief Opaque handle to a custom shader.
 *
 * Created via R3D_CreateCustomShader() and destroyed via R3D_DestroyCustomShader().
 * Can be assigned to R3D_Material.shader to use custom fragment shading.
 */
typedef struct R3D_Shader R3D_Shader;

/**
 * @brief Storage for a custom parameter value on a material.
 */
typedef struct R3D_MaterialParam {
    char name[R3D_MAX_UNIFORM_NAME_LENGTH];  ///< Uniform name to bind to
    R3D_ParamType type;                       ///< Value type
    union {
        float f;            ///< R3D_PARAM_FLOAT
        float v2[2];        ///< R3D_PARAM_VEC2
        float v3[3];        ///< R3D_PARAM_VEC3
        float v4[4];        ///< R3D_PARAM_VEC4
        Texture2D tex;      ///< R3D_PARAM_TEX2D
    } value;
} R3D_MaterialParam;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a custom fragment shader for materials.
 *
 * The provided code is injected into the deferred geometry shader after
 * the default material properties are sampled. The following globals are
 * available for reading and writing:
 *
 * - ALBEDO (vec4): Base color and alpha
 * - NORMAL (vec3): World-space normal
 * - ORM (vec3): Occlusion, Roughness, Metalness
 * - EMISSION (vec3): Emission color
 *
 * Read-only inputs:
 * - vTexCoord (vec2): UV coordinates
 * - vColor (vec4): Vertex color
 * - vTBN (mat3): Tangent-Bitangent-Normal matrix
 *
 * Custom uniforms can be declared at the top of the code:
 * @code
 * uniform sampler2D uMyTexture;
 * uniform float uMyValue;
 *
 * // Your code here, modifying ALBEDO, NORMAL, etc.
 * ALBEDO.rgb = mix(texture(uMyTexture, vTexCoord).rgb, ALBEDO.rgb, uMyValue);
 * @endcode
 *
 * @param fragmentCode GLSL code to inject (uniforms + statements).
 * @return Pointer to shader on success, NULL on compile error (error logged).
 */
R3DAPI R3D_Shader* R3D_CreateCustomShader(const char* fragmentCode);

/**
 * @brief Destroy a custom shader and free its resources.
 *
 * @param shader Shader to destroy (can be NULL).
 */
R3DAPI void R3D_DestroyCustomShader(R3D_Shader* shader);

/**
 * @brief Set a float parameter on a material for its custom shader.
 *
 * @param material Material to modify.
 * @param name Uniform name (must match declaration in shader code).
 * @param value Float value to set.
 */
R3DAPI void R3D_SetMaterialFloat(R3D_Material* material, const char* name, float value);

/**
 * @brief Set a vec2 parameter on a material for its custom shader.
 *
 * @param material Material to modify.
 * @param name Uniform name.
 * @param value Vector2 value to set.
 */
R3DAPI void R3D_SetMaterialVec2(R3D_Material* material, const char* name, Vector2 value);

/**
 * @brief Set a vec3 parameter on a material for its custom shader.
 *
 * @param material Material to modify.
 * @param name Uniform name.
 * @param value Vector3 value to set.
 */
R3DAPI void R3D_SetMaterialVec3(R3D_Material* material, const char* name, Vector3 value);

/**
 * @brief Set a vec4 parameter on a material for its custom shader.
 *
 * @param material Material to modify.
 * @param name Uniform name.
 * @param value Vector4 value to set.
 */
R3DAPI void R3D_SetMaterialVec4(R3D_Material* material, const char* name, Vector4 value);

/**
 * @brief Set a texture parameter on a material for its custom shader.
 *
 * @param material Material to modify.
 * @param name Uniform name.
 * @param texture Texture to bind.
 */
R3DAPI void R3D_SetMaterialTexture(R3D_Material* material, const char* name, Texture2D texture);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of CustomShader

#endif // R3D_SHADER_H
