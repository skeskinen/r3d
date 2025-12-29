/* custom_shader.c -- Example demonstrating custom material shaders
 *
 * This example shows how to use R3D_CreateCustomShader() to create
 * custom fragment shaders that modify material properties.
 *
 * Test 1: Simple color blend based on vertex color (no custom uniforms)
 * Test 2: Texture blend with custom uniforms (sampler + float)
 */

#include <r3d/r3d.h>
#include <raymath.h>
#include <stdlib.h>
#include <stdio.h>

// Helper: Create a sphere mesh with vertex colors based on Y position
// Bottom = blue channel 0, Top = blue channel 1
static R3D_Mesh CreateBlendSphere(float radius, int rings, int slices)
{
    R3D_MeshData data = R3D_GenMeshDataSphere(radius, rings, slices);

    // Paint vertex colors: blue channel = normalized height (0 at bottom, 1 at top)
    for (int i = 0; i < data.vertexCount; i++) {
        float y = data.vertices[i].position.y;
        float t = (y / radius + 1.0f) * 0.5f;  // Map [-radius, radius] to [0, 1]

        data.vertices[i].color = (Color){
            255,                    // R - unused
            255,                    // G - unused
            (unsigned char)(t * 255), // B - blend factor
            255                     // A
        };
    }

    R3D_Mesh mesh = R3D_LoadMesh(R3D_PRIMITIVE_TRIANGLES, &data, NULL, R3D_STATIC_MESH);
    R3D_UnloadMeshData(&data);
    return mesh;
}

int main(void)
{
    // Initialize
    InitWindow(800, 600, "[r3d] - Custom shader example");
    SetTargetFPS(60);
    R3D_Init(800, 600, 0);

    // Create mesh with vertex colors for blending
    R3D_Mesh sphere = CreateBlendSphere(1.0f, 32, 32);
    R3D_Mesh plane = R3D_GenMeshPlane(10.0f, 10.0f, 1, 1);

    // =========================================
    // Test 1: Simple color blend (no custom uniforms)
    // Blends from red (bottom) to blue (top) based on vertex color
    // =========================================
    R3D_Shader* colorBlendShader = R3D_CreateCustomShader(
        // User fragment code - modifies ALBEDO based on vColor.b
        "float blend = vColor.b;\n"
        "ALBEDO.rgb = mix(vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0), blend);\n"
    );

    if (!colorBlendShader) {
        TraceLog(LOG_ERROR, "Failed to create color blend shader");
        // Continue anyway to test error handling
    }

    // =========================================
    // Test 2: Texture blend with custom uniforms
    // Blends between base albedo and a second texture
    // =========================================
    R3D_Shader* textureBlendShader = R3D_CreateCustomShader(
        // Custom uniform declarations
        "uniform sampler2D uTexB;\n"
        "uniform float uBlendPower;\n"
        "uniform vec3 uTintColor;\n"
        "\n"
        // User fragment code
        "float blend = pow(vColor.b, uBlendPower);\n"
        "vec3 colorB = texture(uTexB, vTexCoord).rgb * uTintColor;\n"
        "ALBEDO.rgb = mix(colorB, ALBEDO.rgb, blend);\n"
    );

    if (!textureBlendShader) {
        TraceLog(LOG_ERROR, "Failed to create texture blend shader");
    }

    // Setup materials
    R3D_Material materialTest1 = R3D_GetDefaultMaterial();
    materialTest1.shader = colorBlendShader;

    R3D_Material materialTest2 = R3D_GetDefaultMaterial();
    materialTest2.shader = textureBlendShader;

    // Set custom uniforms for Test 2
    if (textureBlendShader) {
        // Load a simple checkerboard texture for testing
        Image checkerImg = GenImageChecked(64, 64, 8, 8, DARKGREEN, LIME);
        Texture2D checkerTex = LoadTextureFromImage(checkerImg);
        UnloadImage(checkerImg);

        R3D_SetMaterialTexture(&materialTest2, "uTexB", checkerTex);
        R3D_SetMaterialFloat(&materialTest2, "uBlendPower", 2.0f);
        R3D_SetMaterialVec3(&materialTest2, "uTintColor", (Vector3){1.0f, 1.0f, 1.0f});
    }

    R3D_Material floorMaterial = R3D_GetDefaultMaterial();
    floorMaterial.albedo.color = GRAY;

    // Setup lighting
    R3D_Light light = R3D_CreateLight(R3D_LIGHT_DIR);
    R3D_SetLightDirection(light, (Vector3){-1, -1, -1});
    R3D_SetLightActive(light, true);

    R3D_ENVIRONMENT_SET(ambient.color, (Color){30, 30, 40, 255});

    // Setup camera
    Camera3D camera = {
        .position = {4, 3, 4},
        .target = {0, 0, 0},
        .up = {0, 1, 0},
        .fovy = 60,
        .projection = CAMERA_PERSPECTIVE
    };

    int currentTest = 1;
    float blendPower = 2.0f;

    // Main loop
    while (!WindowShouldClose())
    {
        // Input: switch between tests
        if (IsKeyPressed(KEY_ONE)) currentTest = 1;
        if (IsKeyPressed(KEY_TWO)) currentTest = 2;

        // Input: adjust blend power for Test 2
        if (currentTest == 2 && textureBlendShader) {
            if (IsKeyDown(KEY_UP)) blendPower += 0.02f;
            if (IsKeyDown(KEY_DOWN)) blendPower -= 0.02f;
            if (blendPower < 0.1f) blendPower = 0.1f;
            if (blendPower > 5.0f) blendPower = 5.0f;
            R3D_SetMaterialFloat(&materialTest2, "uBlendPower", blendPower);
        }

        UpdateCamera(&camera, CAMERA_ORBITAL);

        // Select material based on current test
        R3D_Material* currentMaterial = (currentTest == 1) ? &materialTest1 : &materialTest2;

        BeginDrawing();
            ClearBackground(DARKGRAY);

            R3D_Begin(camera);
                R3D_DrawMesh(&sphere, currentMaterial, MatrixIdentity());
                R3D_DrawMesh(&plane, &floorMaterial, MatrixTranslate(0, -1.5f, 0));
            R3D_End();

            // UI
            DrawText("Custom Shader Example", 10, 10, 20, WHITE);
            DrawText("Press 1: Color blend (red->blue)", 10, 40, 16, WHITE);
            DrawText("Press 2: Texture blend with custom uniforms", 10, 60, 16, WHITE);

            if (currentTest == 1) {
                DrawText("Test 1: Simple color blend", 10, 100, 20, YELLOW);
                DrawText("Vertex color blue channel controls blend", 10, 125, 16, LIGHTGRAY);
            } else {
                DrawText("Test 2: Texture blend", 10, 100, 20, YELLOW);
                DrawText(TextFormat("Blend power: %.2f (UP/DOWN to adjust)", blendPower), 10, 125, 16, LIGHTGRAY);
            }

            DrawFPS(10, GetScreenHeight() - 30);

        EndDrawing();
    }

    // Cleanup
    R3D_DestroyCustomShader(colorBlendShader);
    R3D_DestroyCustomShader(textureBlendShader);
    R3D_UnloadMesh(&sphere);
    R3D_UnloadMesh(&plane);
    R3D_Close();
    CloseWindow();

    return 0;
}
