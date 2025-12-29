/* pbr_blend.c -- PBR material blending example
 *
 * Demonstrates blending two tiling PBR materials on a sphere using vertex colors.
 * Uses CC0 textures from ambientcg.com (Metal009 + Metal025).
 *
 * Bottom of sphere = material A (rusty metal)
 * Top of sphere = material B (clean brushed metal)
 */

#include <r3d/r3d.h>
#include <raymath.h>
#include <stddef.h>

#ifndef RESOURCES_PATH
#   define RESOURCES_PATH "./"
#endif

// Create a sphere mesh with vertex colors based on Y position
// Blue channel: 0 at bottom, 1 at top
static R3D_Mesh CreateBlendSphere(float radius, int rings, int slices)
{
    R3D_MeshData data = R3D_GenMeshDataSphere(radius, rings, slices);

    for (int i = 0; i < data.vertexCount; i++) {
        float y = data.vertices[i].position.y;
        float t = (y / radius + 1.0f) * 0.5f;  // Map [-radius, radius] to [0, 1]

        data.vertices[i].color = (Color){
            255,                        // R - unused
            255,                        // G - unused
            (unsigned char)(t * 255),   // B - blend factor
            255                         // A
        };
    }

    R3D_Mesh mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, &data, NULL, R3D_STATIC_MESH);
    R3D_UnloadMeshData(&data);
    return mesh;
}

// Create ORM texture from separate roughness and metalness images
// ORM = Occlusion (R), Roughness (G), Metalness (B)
static Texture2D CreateORMTexture(const char* roughnessPath, const char* metalnessPath)
{
    Image roughImg = LoadImage(roughnessPath);
    Image metalImg = LoadImage(metalnessPath);

    // Create ORM image (white occlusion, roughness in G, metalness in B)
    Image ormImg = GenImageColor(roughImg.width, roughImg.height, WHITE);

    // Copy roughness to green channel, metalness to blue channel
    Color* ormPixels = LoadImageColors(ormImg);
    Color* roughPixels = LoadImageColors(roughImg);
    Color* metalPixels = LoadImageColors(metalImg);

    for (int i = 0; i < roughImg.width * roughImg.height; i++) {
        ormPixels[i].r = 255;                // Occlusion = 1.0 (no AO)
        ormPixels[i].g = roughPixels[i].r;   // Roughness from red channel
        ormPixels[i].b = metalPixels[i].r;   // Metalness from red channel
        ormPixels[i].a = 255;
    }

    // Update the ORM image with modified pixels
    for (int y = 0; y < ormImg.height; y++) {
        for (int x = 0; x < ormImg.width; x++) {
            ImageDrawPixel(&ormImg, x, y, ormPixels[y * ormImg.width + x]);
        }
    }

    UnloadImageColors(ormPixels);
    UnloadImageColors(roughPixels);
    UnloadImageColors(metalPixels);
    UnloadImage(roughImg);
    UnloadImage(metalImg);

    Texture2D tex = LoadTextureFromImage(ormImg);
    UnloadImage(ormImg);

    return tex;
}

int main(void)
{
    // Initialize window
    InitWindow(800, 600, "[r3d] - PBR material blend example");
    SetTargetFPS(60);

    // Initialize R3D
    R3D_Init(GetScreenWidth(), GetScreenHeight(), R3D_FLAG_FXAA);

    // Tonemapping
    R3D_ENVIRONMENT_SET(tonemap.mode, R3D_TONEMAP_ACES);
    R3D_ENVIRONMENT_SET(tonemap.exposure, 0.75f);
    R3D_ENVIRONMENT_SET(tonemap.white, 1.25f);

    // Set texture filter for mipmaps
    R3D_SetTextureFilter(TEXTURE_FILTER_TRILINEAR);

    // Load PBR textures (CC0 from ambientcg.com)
    // Material A: Rusty metal (Metal025)
    Texture2D albedoA = LoadTexture(RESOURCES_PATH "pbr/textures/metal_rust_albedo.jpg");
    Texture2D normalA = LoadTexture(RESOURCES_PATH "pbr/textures/metal_rust_normal.jpg");
    Texture2D ormA = CreateORMTexture(
        RESOURCES_PATH "pbr/textures/metal_rust_roughness.jpg",
        RESOURCES_PATH "pbr/textures/metal_rust_metalness.jpg"
    );

    // Material B: Clean brushed metal (Metal009)
    Texture2D albedoB = LoadTexture(RESOURCES_PATH "pbr/textures/metal_clean_albedo.jpg");
    Texture2D normalB = LoadTexture(RESOURCES_PATH "pbr/textures/metal_clean_normal.jpg");
    Texture2D ormB = CreateORMTexture(
        RESOURCES_PATH "pbr/textures/metal_clean_roughness.jpg",
        RESOURCES_PATH "pbr/textures/metal_clean_metalness.jpg"
    );

    // Enable texture wrapping for tiling
    SetTextureWrap(albedoA, TEXTURE_WRAP_REPEAT);
    SetTextureWrap(normalA, TEXTURE_WRAP_REPEAT);
    SetTextureWrap(ormA, TEXTURE_WRAP_REPEAT);
    SetTextureWrap(albedoB, TEXTURE_WRAP_REPEAT);
    SetTextureWrap(normalB, TEXTURE_WRAP_REPEAT);
    SetTextureWrap(ormB, TEXTURE_WRAP_REPEAT);

    TraceLog(LOG_INFO, "Loaded PBR textures from ambientcg.com (CC0)");

    // Create sphere mesh with vertex colors for blending
    R3D_Mesh sphere = CreateBlendSphere(1.0f, 32, 32);

    // Create custom shader for PBR material blending
    // Blends all PBR channels: albedo, normal, ORM
    R3D_Shader* blendShader = R3D_CreateCustomShader(
        // Custom uniforms for material B textures
        "uniform sampler2D uAlbedoB;\n"
        "uniform sampler2D uNormalB;\n"
        "uniform sampler2D uOrmB;\n"
        "uniform float uThreshold;\n"
        "uniform float uSharpness;\n"
        "uniform float uUVScale;\n"
        "\n"
        // Scale UVs for tiling
        "vec2 uv = vTexCoord * uUVScale;\n"
        "\n"
        // Blend based on vertex color blue channel with adjustable threshold/sharpness
        "float t = vColor.b;\n"
        "float edge0 = uThreshold - uSharpness;\n"
        "float edge1 = uThreshold + uSharpness;\n"
        "float blend = smoothstep(edge0, edge1, t);\n"
        "\n"
        // Sample material B textures
        "vec4 albedoB = texture(uAlbedoB, uv);\n"
        "vec3 normalB = texture(uNormalB, uv).rgb * 2.0 - 1.0;\n"
        "vec3 ormB = texture(uOrmB, uv).rgb;\n"
        "\n"
        // Blend albedo
        "ALBEDO = mix(albedoB, ALBEDO, blend);\n"
        "\n"
        // Blend normals (lerp in tangent space)
        "vec3 normalA = NORMAL;\n"
        "NORMAL = normalize(mix(normalB, normalA, blend));\n"
        "\n"
        // Blend ORM
        "ORM = mix(ormB, ORM, blend);\n"
    );

    if (!blendShader) {
        TraceLog(LOG_ERROR, "Failed to create blend shader");
        return 1;
    }

    // Create material A (rusty - shown at bottom)
    R3D_Material blendMaterial = R3D_GetDefaultMaterial();
    blendMaterial.albedo.texture = albedoA;
    blendMaterial.albedo.color = WHITE;
    blendMaterial.normal.texture = normalA;
    blendMaterial.normal.scale = 1.0f;
    blendMaterial.orm.texture = ormA;
    blendMaterial.orm.occlusion = 1.0f;
    blendMaterial.orm.roughness = 1.0f;
    blendMaterial.orm.metalness = 1.0f;
    blendMaterial.shader = blendShader;

    // Set material B textures as custom uniforms (clean - shown at top)
    R3D_SetMaterialTexture(&blendMaterial, "uAlbedoB", albedoB);
    R3D_SetMaterialTexture(&blendMaterial, "uNormalB", normalB);
    R3D_SetMaterialTexture(&blendMaterial, "uOrmB", ormB);

    // Shader parameters
    float threshold = 0.5f;   // Where the blend transition happens (0-1)
    float sharpness = 0.25f;  // Width of transition (smaller = sharper)
    float uvScale = 2.0f;
    R3D_SetMaterialFloat(&blendMaterial, "uThreshold", threshold);
    R3D_SetMaterialFloat(&blendMaterial, "uSharpness", sharpness);
    R3D_SetMaterialFloat(&blendMaterial, "uUVScale", uvScale);

    // Load skybox
    R3D_Skybox skybox = R3D_LoadSkybox(RESOURCES_PATH "sky/skybox2.png", CUBEMAP_LAYOUT_AUTO_DETECT);
    R3D_ENVIRONMENT_SET(background.sky, skybox);

    // Setup directional light
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    R3D_SetLightDirection(light, (Vector3){-1, -1, -1});
    R3D_SetLightActive(light, true);

    // Floor material (using rust texture)
    R3D_Material floorMaterial = R3D_GetDefaultMaterial();
    floorMaterial.albedo.texture = albedoA;
    floorMaterial.normal.texture = normalA;
    floorMaterial.orm.texture = ormA;
    floorMaterial.uvScale = (Vector2){4.0f, 4.0f};
    R3D_Mesh plane = R3D_GenMeshPlane(10.0f, 10.0f, 1, 1);

    // Setup camera
    Camera3D camera = {
        .position = {3, 2, 3},
        .target = {0, 0, 0},
        .up = {0, 1, 0},
        .fovy = 60,
        .projection = CAMERA_PERSPECTIVE
    };

    // Main loop
    while (!WindowShouldClose())
    {
        // Adjust threshold (where the blend happens)
        if (IsKeyDown(KEY_UP)) threshold += 0.01f;
        if (IsKeyDown(KEY_DOWN)) threshold -= 0.01f;
        threshold = Clamp(threshold, 0.0f, 1.0f);
        R3D_SetMaterialFloat(&blendMaterial, "uThreshold", threshold);

        // Adjust sharpness (width of transition)
        if (IsKeyDown(KEY_RIGHT)) sharpness -= 0.005f;
        if (IsKeyDown(KEY_LEFT)) sharpness += 0.005f;
        sharpness = Clamp(sharpness, 0.01f, 0.5f);
        R3D_SetMaterialFloat(&blendMaterial, "uSharpness", sharpness);

        // Adjust UV scale
        if (IsKeyDown(KEY_W)) uvScale += 0.02f;
        if (IsKeyDown(KEY_S)) uvScale -= 0.02f;
        uvScale = Clamp(uvScale, 0.5f, 8.0f);
        R3D_SetMaterialFloat(&blendMaterial, "uUVScale", uvScale);

        // Update camera
        UpdateCamera(&camera, CAMERA_ORBITAL);

        BeginDrawing();
            ClearBackground(DARKGRAY);

            R3D_Begin(camera);
                R3D_DrawMesh(&sphere, &blendMaterial, MatrixIdentity());
                R3D_DrawMesh(&plane, &floorMaterial, MatrixTranslate(0, -1.5f, 0));
            R3D_End();

            // UI
            DrawText("PBR Material Blend Example", 10, 10, 20, WHITE);
            DrawText(TextFormat("Threshold: %.2f (UP/DOWN)", threshold), 10, 40, 16, WHITE);
            DrawText(TextFormat("Sharpness: %.2f (LEFT=soft, RIGHT=sharp)", sharpness), 10, 60, 16, WHITE);
            DrawText(TextFormat("UV scale: %.1f (W/S)", uvScale), 10, 80, 16, WHITE);
            DrawText("Bottom = clean brushed metal", 10, 120, 16, LIGHTGRAY);
            DrawText("Top = rusty metal", 10, 140, 16, ORANGE);
            DrawText("Textures: ambientcg.com (CC0)", 10, GetScreenHeight()-26, 14, GRAY);
            DrawFPS(10, GetScreenHeight() - 50);

        EndDrawing();
    }

    // Cleanup
    R3D_DestroyCustomShader(blendShader);
    R3D_UnloadMesh(&sphere);
    R3D_UnloadMesh(&plane);
    UnloadTexture(albedoA);
    UnloadTexture(normalA);
    UnloadTexture(ormA);
    UnloadTexture(albedoB);
    UnloadTexture(normalB);
    UnloadTexture(ormB);
    R3D_UnloadSkybox(skybox);
    R3D_Close();

    CloseWindow();

    return 0;
}
