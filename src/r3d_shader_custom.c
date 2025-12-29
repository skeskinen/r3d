/* r3d_shader_custom.c -- Custom shader implementation.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_shader.h>
#include <r3d/r3d_material.h>
#include <raylib.h>
#include <rlgl.h>
#include <glad.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Include generated shader headers
#include "shaders/geometry.vert.h"
#include "shaders/geometry.frag.h"

// Internal shader structure (opaque to users)
struct R3D_Shader {
    unsigned int program;                                   // OpenGL shader program ID
    R3D_UniformInfo customUniforms[R3D_MAX_CUSTOM_UNIFORMS];
    int customUniformCount;
    int nextTexSlot;                                        // Next available texture slot for custom samplers

    // Built-in uniform locations (same as default geometry shader)
    int locMatModel;
    int locMatNormal;
    int locAlbedoColor;
    int locEmissionEnergy;
    int locEmissionColor;
    int locTexCoordOffset;
    int locTexCoordScale;
    int locInstancing;
    int locSkinning;
    int locBillboard;
    int locTexAlbedo;
    int locTexNormal;
    int locTexEmission;
    int locTexORM;
    int locAlphaCutoff;
    int locNormalScale;
    int locOcclusion;
    int locRoughness;
    int locMetalness;
    int locTexBoneMatrices;
};

// Marker in geometry.frag that gets replaced with user code
#define USER_FRAGMENT_MARKER "#define R3D_USER_FRAGMENT_MARKER 0"

// Built-in texture slots (custom uniforms start at slot 5)
#define FIRST_CUSTOM_TEX_SLOT 5

// ============================================================================
// Helper functions
// ============================================================================

static GLuint compile_shader_source(const char* source, GLenum shaderType)
{
    GLuint shader = glCreateShader(shaderType);
    if (shader == 0) {
        TraceLog(LOG_ERROR, "R3D_CUSTOM: Failed to create shader object");
        return 0;
    }

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, sizeof(infoLog), NULL, infoLog);
        const char* type_str = (shaderType == GL_VERTEX_SHADER) ? "vertex" : "fragment";
        TraceLog(LOG_ERROR, "R3D_CUSTOM: %s shader compilation failed:\n%s", type_str, infoLog);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint link_shader_program(GLuint vertShader, GLuint fragShader)
{
    GLuint program = glCreateProgram();
    if (program == 0) {
        TraceLog(LOG_ERROR, "R3D_CUSTOM: Failed to create shader program");
        return 0;
    }

    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(program, sizeof(infoLog), NULL, infoLog);
        TraceLog(LOG_ERROR, "R3D_CUSTOM: Shader program linking failed:\n%s", infoLog);
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

// Split user code into uniforms (lines starting with "uniform") and body
// Uniforms need to be inserted after #version, body goes at the marker
static void split_user_code(const char* userCode, char** outUniforms, char** outBody)
{
    // Count total length for allocation
    size_t len = strlen(userCode);
    *outUniforms = (char*)RL_MALLOC(len + 1);
    *outBody = (char*)RL_MALLOC(len + 1);
    (*outUniforms)[0] = '\0';
    (*outBody)[0] = '\0';

    char* uniformsDst = *outUniforms;
    char* bodyDst = *outBody;

    // Process line by line
    const char* line = userCode;
    while (*line) {
        // Find end of line
        const char* eol = strchr(line, '\n');
        size_t lineLen = eol ? (size_t)(eol - line) : strlen(line);

        // Skip leading whitespace for detection
        const char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        // Check if this is a uniform declaration
        if (strncmp(trimmed, "uniform ", 8) == 0) {
            memcpy(uniformsDst, line, lineLen);
            uniformsDst += lineLen;
            *uniformsDst++ = '\n';
        } else if (lineLen > 0) {
            // Non-empty, non-uniform line goes to body
            memcpy(bodyDst, line, lineLen);
            bodyDst += lineLen;
            *bodyDst++ = '\n';
        }

        if (eol) {
            line = eol + 1;
        } else {
            break;
        }
    }

    *uniformsDst = '\0';
    *bodyDst = '\0';
}

// Compose fragment shader: insert uniforms after #version, replace marker with body
static char* compose_fragment_shader(const char* baseShader, const char* userUniforms, const char* userBody)
{
    // Find #version line end
    const char* versionStart = strstr(baseShader, "#version");
    if (!versionStart) {
        TraceLog(LOG_ERROR, "R3D_CUSTOM: Base shader missing #version");
        return NULL;
    }
    const char* versionEnd = strchr(versionStart, '\n');
    if (!versionEnd) versionEnd = versionStart + strlen(versionStart);
    else versionEnd++; // Include newline

    // Find the marker
    const char* marker = strstr(baseShader, USER_FRAGMENT_MARKER);
    if (!marker) {
        TraceLog(LOG_ERROR, "R3D_CUSTOM: Base shader missing user fragment marker");
        return NULL;
    }

    // Calculate sizes
    size_t prefixLen = versionEnd - baseShader;
    size_t uniformsLen = strlen(userUniforms);
    size_t middleStart = versionEnd - baseShader;
    size_t middleLen = marker - versionEnd;
    size_t bodyLen = strlen(userBody);
    size_t markerLen = strlen(USER_FRAGMENT_MARKER);
    size_t suffixLen = strlen(marker + markerLen);

    size_t totalLen = prefixLen + uniformsLen + middleLen + bodyLen + suffixLen + 1;

    char* composed = (char*)RL_MALLOC(totalLen);
    if (!composed) return NULL;

    char* dst = composed;

    // Copy prefix (up to and including #version line)
    memcpy(dst, baseShader, prefixLen);
    dst += prefixLen;

    // Insert user uniforms
    if (uniformsLen > 0) {
        memcpy(dst, userUniforms, uniformsLen);
        dst += uniformsLen;
    }

    // Copy middle (from after version to marker)
    memcpy(dst, versionEnd, middleLen);
    dst += middleLen;

    // Insert user body (replacing marker)
    if (bodyLen > 0) {
        memcpy(dst, userBody, bodyLen);
        dst += bodyLen;
    }

    // Copy suffix (after marker)
    memcpy(dst, marker + markerLen, suffixLen);
    dst += suffixLen;

    *dst = '\0';

    return composed;
}

// Discover custom uniforms in the compiled shader
static void discover_custom_uniforms(R3D_Shader* shader)
{
    shader->customUniformCount = 0;
    shader->nextTexSlot = FIRST_CUSTOM_TEX_SLOT;

    GLint uniformCount;
    glGetProgramiv(shader->program, GL_ACTIVE_UNIFORMS, &uniformCount);

    // List of built-in uniform names to skip
    static const char* builtinUniforms[] = {
        "uTexAlbedo", "uTexNormal", "uTexEmission", "uTexORM", "uTexBoneMatrices",
        "uAlphaCutoff", "uNormalScale", "uOcclusion", "uRoughness", "uMetalness",
        "uAlbedoColor", "uEmissionEnergy", "uEmissionColor",
        "uTexCoordOffset", "uTexCoordScale", "uInstancing", "uSkinning", "uBillboard",
        "uMatModel", "uMatNormal", "ViewBlock", NULL
    };

    char name[256];
    for (GLint i = 0; i < uniformCount && shader->customUniformCount < R3D_MAX_CUSTOM_UNIFORMS; i++) {
        GLint size;
        GLenum type;
        glGetActiveUniform(shader->program, i, sizeof(name), NULL, &size, &type, name);

        // Skip built-in uniforms
        bool isBuiltin = false;
        for (int j = 0; builtinUniforms[j] != NULL; j++) {
            if (strcmp(name, builtinUniforms[j]) == 0) {
                isBuiltin = true;
                break;
            }
        }
        if (isBuiltin) continue;

        // Skip uniforms starting with "gl_" or array indices
        if (strncmp(name, "gl_", 3) == 0) continue;
        if (strchr(name, '[')) continue;

        // Found a custom uniform
        R3D_UniformInfo* info = &shader->customUniforms[shader->customUniformCount++];
        strncpy(info->name, name, R3D_MAX_UNIFORM_NAME_LENGTH - 1);
        info->name[R3D_MAX_UNIFORM_NAME_LENGTH - 1] = '\0';
        info->location = glGetUniformLocation(shader->program, name);

        // Map GL type to our enum
        switch (type) {
            case GL_FLOAT:
                info->type = R3D_PARAM_FLOAT;
                info->texSlot = -1;
                break;
            case GL_FLOAT_VEC2:
                info->type = R3D_PARAM_VEC2;
                info->texSlot = -1;
                break;
            case GL_FLOAT_VEC3:
                info->type = R3D_PARAM_VEC3;
                info->texSlot = -1;
                break;
            case GL_FLOAT_VEC4:
                info->type = R3D_PARAM_VEC4;
                info->texSlot = -1;
                break;
            case GL_SAMPLER_2D:
                info->type = R3D_PARAM_TEX2D;
                info->texSlot = shader->nextTexSlot++;
                // Set the sampler uniform to its texture slot
                glUseProgram(shader->program);
                glUniform1i(info->location, info->texSlot);
                break;
            default:
                // Unknown type, skip
                shader->customUniformCount--;
                break;
        }
    }

    TraceLog(LOG_INFO, "R3D_CUSTOM: Discovered %d custom uniform(s)", shader->customUniformCount);
    for (int i = 0; i < shader->customUniformCount; i++) {
        TraceLog(LOG_DEBUG, "  - %s (type=%d, loc=%d, slot=%d)",
            shader->customUniforms[i].name,
            shader->customUniforms[i].type,
            shader->customUniforms[i].location,
            shader->customUniforms[i].texSlot);
    }
}

// Cache built-in uniform locations
static void cache_builtin_uniforms(R3D_Shader* shader)
{
    shader->locMatModel = glGetUniformLocation(shader->program, "uMatModel");
    shader->locMatNormal = glGetUniformLocation(shader->program, "uMatNormal");
    shader->locAlbedoColor = glGetUniformLocation(shader->program, "uAlbedoColor");
    shader->locEmissionEnergy = glGetUniformLocation(shader->program, "uEmissionEnergy");
    shader->locEmissionColor = glGetUniformLocation(shader->program, "uEmissionColor");
    shader->locTexCoordOffset = glGetUniformLocation(shader->program, "uTexCoordOffset");
    shader->locTexCoordScale = glGetUniformLocation(shader->program, "uTexCoordScale");
    shader->locInstancing = glGetUniformLocation(shader->program, "uInstancing");
    shader->locSkinning = glGetUniformLocation(shader->program, "uSkinning");
    shader->locBillboard = glGetUniformLocation(shader->program, "uBillboard");
    shader->locTexAlbedo = glGetUniformLocation(shader->program, "uTexAlbedo");
    shader->locTexNormal = glGetUniformLocation(shader->program, "uTexNormal");
    shader->locTexEmission = glGetUniformLocation(shader->program, "uTexEmission");
    shader->locTexORM = glGetUniformLocation(shader->program, "uTexORM");
    shader->locAlphaCutoff = glGetUniformLocation(shader->program, "uAlphaCutoff");
    shader->locNormalScale = glGetUniformLocation(shader->program, "uNormalScale");
    shader->locOcclusion = glGetUniformLocation(shader->program, "uOcclusion");
    shader->locRoughness = glGetUniformLocation(shader->program, "uRoughness");
    shader->locMetalness = glGetUniformLocation(shader->program, "uMetalness");
    shader->locTexBoneMatrices = glGetUniformLocation(shader->program, "uTexBoneMatrices");
}

// ============================================================================
// Public API
// ============================================================================

R3D_Shader* R3D_CreateCustomShader(const char* fragmentCode)
{
    if (!fragmentCode || fragmentCode[0] == '\0') {
        TraceLog(LOG_ERROR, "R3D_CreateCustomShader: Empty fragment code");
        return NULL;
    }

    // Split user code into uniforms and body
    char* userUniforms = NULL;
    char* userBody = NULL;
    split_user_code(fragmentCode, &userUniforms, &userBody);

    // Compose the final fragment shader
    char* composedFrag = compose_fragment_shader(GEOMETRY_FRAG, userUniforms, userBody);

    RL_FREE(userUniforms);
    RL_FREE(userBody);

    if (!composedFrag) {
        return NULL;
    }

    // Compile vertex shader (unchanged)
    GLuint vs = compile_shader_source(GEOMETRY_VERT, GL_VERTEX_SHADER);
    if (vs == 0) {
        RL_FREE(composedFrag);
        return NULL;
    }

    // Compile composed fragment shader
    GLuint fs = compile_shader_source(composedFrag, GL_FRAGMENT_SHADER);
    RL_FREE(composedFrag);

    if (fs == 0) {
        glDeleteShader(vs);
        return NULL;
    }

    // Link program
    GLuint program = link_shader_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    if (program == 0) {
        return NULL;
    }

    // Allocate shader struct
    R3D_Shader* shader = (R3D_Shader*)RL_CALLOC(1, sizeof(R3D_Shader));
    if (!shader) {
        glDeleteProgram(program);
        return NULL;
    }

    shader->program = program;

    // Cache built-in uniform locations
    cache_builtin_uniforms(shader);

    // Discover custom uniforms
    discover_custom_uniforms(shader);

    // Bind ViewBlock UBO if present
    GLuint viewBlockIndex = glGetUniformBlockIndex(program, "ViewBlock");
    if (viewBlockIndex != GL_INVALID_INDEX) {
        glUniformBlockBinding(program, viewBlockIndex, 0); // R3D_SHADER_UBO_VIEW_SLOT = 0
    }

    // Set built-in sampler uniforms to their texture slots (same as default geometry shader)
    glUseProgram(program);
    if (shader->locTexBoneMatrices >= 0) glUniform1i(shader->locTexBoneMatrices, 0);
    if (shader->locTexAlbedo >= 0) glUniform1i(shader->locTexAlbedo, 1);
    if (shader->locTexNormal >= 0) glUniform1i(shader->locTexNormal, 2);
    if (shader->locTexEmission >= 0) glUniform1i(shader->locTexEmission, 3);
    if (shader->locTexORM >= 0) glUniform1i(shader->locTexORM, 4);
    glUseProgram(0);

    TraceLog(LOG_INFO, "R3D_CreateCustomShader: Created custom shader (program=%u)", program);

    return shader;
}

void R3D_DestroyCustomShader(R3D_Shader* shader)
{
    if (shader == NULL) return;

    if (shader->program != 0) {
        glDeleteProgram(shader->program);
    }

    RL_FREE(shader);
}

// Helper: Find or create a parameter slot in a material
static R3D_MaterialParam* FindOrCreateParam(R3D_Material* material, const char* name, R3D_ParamType type)
{
    // Search for existing param
    for (int i = 0; i < material->paramCount; i++) {
        if (strcmp(material->params[i].name, name) == 0) {
            return &material->params[i];
        }
    }

    // Need to add new param - ensure capacity
    if (material->params == NULL) {
        material->paramCapacity = 4;
        material->params = (R3D_MaterialParam*)RL_MALLOC(material->paramCapacity * sizeof(R3D_MaterialParam));
        material->paramCount = 0;
    } else if (material->paramCount >= material->paramCapacity) {
        material->paramCapacity *= 2;
        material->params = (R3D_MaterialParam*)RL_REALLOC(material->params, material->paramCapacity * sizeof(R3D_MaterialParam));
    }

    // Add new param
    R3D_MaterialParam* param = &material->params[material->paramCount++];
    memset(param, 0, sizeof(R3D_MaterialParam));
    strncpy(param->name, name, R3D_MAX_UNIFORM_NAME_LENGTH - 1);
    param->type = type;

    return param;
}

void R3D_SetMaterialFloat(R3D_Material* material, const char* name, float value)
{
    if (material == NULL || name == NULL) return;

    R3D_MaterialParam* param = FindOrCreateParam(material, name, R3D_PARAM_FLOAT);
    param->value.f = value;
}

void R3D_SetMaterialVec2(R3D_Material* material, const char* name, Vector2 value)
{
    if (material == NULL || name == NULL) return;

    R3D_MaterialParam* param = FindOrCreateParam(material, name, R3D_PARAM_VEC2);
    param->value.v2[0] = value.x;
    param->value.v2[1] = value.y;
}

void R3D_SetMaterialVec3(R3D_Material* material, const char* name, Vector3 value)
{
    if (material == NULL || name == NULL) return;

    R3D_MaterialParam* param = FindOrCreateParam(material, name, R3D_PARAM_VEC3);
    param->value.v3[0] = value.x;
    param->value.v3[1] = value.y;
    param->value.v3[2] = value.z;
}

void R3D_SetMaterialVec4(R3D_Material* material, const char* name, Vector4 value)
{
    if (material == NULL || name == NULL) return;

    R3D_MaterialParam* param = FindOrCreateParam(material, name, R3D_PARAM_VEC4);
    param->value.v4[0] = value.x;
    param->value.v4[1] = value.y;
    param->value.v4[2] = value.z;
    param->value.v4[3] = value.w;
}

void R3D_SetMaterialTexture(R3D_Material* material, const char* name, Texture2D texture)
{
    if (material == NULL || name == NULL) return;

    R3D_MaterialParam* param = FindOrCreateParam(material, name, R3D_PARAM_TEX2D);
    param->value.tex = texture;
}

// ============================================================================
// Internal API for r3d_draw.c
// ============================================================================

// Get shader program ID
unsigned int R3D_GetCustomShaderProgram(const R3D_Shader* shader)
{
    return shader ? shader->program : 0;
}

// Bind custom uniforms from material params to shader
void R3D_BindCustomUniforms(const R3D_Shader* shader, const R3D_Material* material)
{
    if (!shader || !material) return;

    for (int i = 0; i < shader->customUniformCount; i++) {
        const R3D_UniformInfo* info = &shader->customUniforms[i];

        // Find matching param in material
        const R3D_MaterialParam* param = NULL;
        for (int j = 0; j < material->paramCount; j++) {
            if (strcmp(material->params[j].name, info->name) == 0) {
                param = &material->params[j];
                break;
            }
        }

        if (!param) continue; // Param not set on material

        // Bind based on type
        switch (info->type) {
            case R3D_PARAM_FLOAT:
                glUniform1f(info->location, param->value.f);
                break;
            case R3D_PARAM_VEC2:
                glUniform2fv(info->location, 1, param->value.v2);
                break;
            case R3D_PARAM_VEC3:
                glUniform3fv(info->location, 1, param->value.v3);
                break;
            case R3D_PARAM_VEC4:
                glUniform4fv(info->location, 1, param->value.v4);
                break;
            case R3D_PARAM_TEX2D:
                glActiveTexture(GL_TEXTURE0 + info->texSlot);
                glBindTexture(GL_TEXTURE_2D, param->value.tex.id);
                break;
        }
    }
}

// Built-in uniform setters for custom shaders
void R3D_CustomShaderSetMatModel(const R3D_Shader* shader, const float* matrix)
{
    if (shader && shader->locMatModel >= 0)
        glUniformMatrix4fv(shader->locMatModel, 1, GL_TRUE, matrix);
}

void R3D_CustomShaderSetMatNormal(const R3D_Shader* shader, const float* matrix)
{
    if (shader && shader->locMatNormal >= 0)
        glUniformMatrix4fv(shader->locMatNormal, 1, GL_TRUE, matrix);
}

void R3D_CustomShaderSetAlbedoColor(const R3D_Shader* shader, float r, float g, float b, float a)
{
    if (shader && shader->locAlbedoColor >= 0)
        glUniform4f(shader->locAlbedoColor, r, g, b, a);
}

void R3D_CustomShaderSetEmissionEnergy(const R3D_Shader* shader, float value)
{
    if (shader && shader->locEmissionEnergy >= 0)
        glUniform1f(shader->locEmissionEnergy, value);
}

void R3D_CustomShaderSetEmissionColor(const R3D_Shader* shader, float r, float g, float b)
{
    if (shader && shader->locEmissionColor >= 0)
        glUniform3f(shader->locEmissionColor, r, g, b);
}

void R3D_CustomShaderSetTexCoordOffset(const R3D_Shader* shader, float x, float y)
{
    if (shader && shader->locTexCoordOffset >= 0)
        glUniform2f(shader->locTexCoordOffset, x, y);
}

void R3D_CustomShaderSetTexCoordScale(const R3D_Shader* shader, float x, float y)
{
    if (shader && shader->locTexCoordScale >= 0)
        glUniform2f(shader->locTexCoordScale, x, y);
}

void R3D_CustomShaderSetInstancing(const R3D_Shader* shader, int value)
{
    if (shader && shader->locInstancing >= 0)
        glUniform1i(shader->locInstancing, value);
}

void R3D_CustomShaderSetSkinning(const R3D_Shader* shader, int value)
{
    if (shader && shader->locSkinning >= 0)
        glUniform1i(shader->locSkinning, value);
}

void R3D_CustomShaderSetBillboard(const R3D_Shader* shader, int value)
{
    if (shader && shader->locBillboard >= 0)
        glUniform1i(shader->locBillboard, value);
}

void R3D_CustomShaderSetAlphaCutoff(const R3D_Shader* shader, float value)
{
    if (shader && shader->locAlphaCutoff >= 0)
        glUniform1f(shader->locAlphaCutoff, value);
}

void R3D_CustomShaderSetNormalScale(const R3D_Shader* shader, float value)
{
    if (shader && shader->locNormalScale >= 0)
        glUniform1f(shader->locNormalScale, value);
}

void R3D_CustomShaderSetOcclusion(const R3D_Shader* shader, float value)
{
    if (shader && shader->locOcclusion >= 0)
        glUniform1f(shader->locOcclusion, value);
}

void R3D_CustomShaderSetRoughness(const R3D_Shader* shader, float value)
{
    if (shader && shader->locRoughness >= 0)
        glUniform1f(shader->locRoughness, value);
}

void R3D_CustomShaderSetMetalness(const R3D_Shader* shader, float value)
{
    if (shader && shader->locMetalness >= 0)
        glUniform1f(shader->locMetalness, value);
}

int R3D_CustomShaderGetTexBoneMatricesLoc(const R3D_Shader* shader)
{
    return shader ? shader->locTexBoneMatrices : -1;
}

int R3D_CustomShaderGetTexAlbedoLoc(const R3D_Shader* shader)
{
    return shader ? shader->locTexAlbedo : -1;
}

int R3D_CustomShaderGetTexNormalLoc(const R3D_Shader* shader)
{
    return shader ? shader->locTexNormal : -1;
}

int R3D_CustomShaderGetTexEmissionLoc(const R3D_Shader* shader)
{
    return shader ? shader->locTexEmission : -1;
}

int R3D_CustomShaderGetTexORMLoc(const R3D_Shader* shader)
{
    return shader ? shader->locTexORM : -1;
}
