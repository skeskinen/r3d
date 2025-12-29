/* r3d_shader_custom.h -- Internal header for custom shader support.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODULE_SHADER_CUSTOM_H
#define R3D_MODULE_SHADER_CUSTOM_H

#include <r3d/r3d_shader.h>
#include <r3d/r3d_material.h>

// Get the OpenGL program ID from a custom shader
unsigned int R3D_GetCustomShaderProgram(const R3D_Shader* shader);

// Bind custom uniforms from material params to the currently bound shader
void R3D_BindCustomUniforms(const R3D_Shader* shader, const R3D_Material* material);

// Set built-in uniform values on a custom shader
// These use the cached locations from shader creation
void R3D_CustomShaderSetMatModel(const R3D_Shader* shader, const float* matrix);
void R3D_CustomShaderSetMatNormal(const R3D_Shader* shader, const float* matrix);
void R3D_CustomShaderSetAlbedoColor(const R3D_Shader* shader, float r, float g, float b, float a);
void R3D_CustomShaderSetEmissionEnergy(const R3D_Shader* shader, float value);
void R3D_CustomShaderSetEmissionColor(const R3D_Shader* shader, float r, float g, float b);
void R3D_CustomShaderSetTexCoordOffset(const R3D_Shader* shader, float x, float y);
void R3D_CustomShaderSetTexCoordScale(const R3D_Shader* shader, float x, float y);
void R3D_CustomShaderSetInstancing(const R3D_Shader* shader, int value);
void R3D_CustomShaderSetSkinning(const R3D_Shader* shader, int value);
void R3D_CustomShaderSetBillboard(const R3D_Shader* shader, int value);
void R3D_CustomShaderSetAlphaCutoff(const R3D_Shader* shader, float value);
void R3D_CustomShaderSetNormalScale(const R3D_Shader* shader, float value);
void R3D_CustomShaderSetOcclusion(const R3D_Shader* shader, float value);
void R3D_CustomShaderSetRoughness(const R3D_Shader* shader, float value);
void R3D_CustomShaderSetMetalness(const R3D_Shader* shader, float value);

// Get texture uniform locations (slots are fixed)
int R3D_CustomShaderGetTexBoneMatricesLoc(const R3D_Shader* shader);
int R3D_CustomShaderGetTexAlbedoLoc(const R3D_Shader* shader);
int R3D_CustomShaderGetTexNormalLoc(const R3D_Shader* shader);
int R3D_CustomShaderGetTexEmissionLoc(const R3D_Shader* shader);
int R3D_CustomShaderGetTexORMLoc(const R3D_Shader* shader);

#endif // R3D_MODULE_SHADER_CUSTOM_H
