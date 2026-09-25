/*
 * MIT License
 *
 * Copyright (c) 2025 Malaka Gunawardana
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <Bitmap.h>
#include <Hx86/Hx86.h>
#include <Math3D.h>
#include <Renderer3D.h>

HX86_DECLARE_APP(HX86_APP_GUI);

// Keyboard scancodes for WASD movement.
#define SC_W 0x11
#define SC_A 0x1E
#define SC_S 0x1F
#define SC_D 0x20
#define SC_SPACE 0x39
#define SC_LSHIFT 0x2A
#define SC_ESC 0x01
#define SC_Q 0x10
#define SC_E 0x12

// Internal render resolution, upscaled to the screen.
#define RENDER_W 1024
#define RENDER_H 576

// --------------------------------------------------------------------------
// SKY SPHERE GENERATOR
// --------------------------------------------------------------------------

/**
 * GenerateSkySphere() - Build an inward-facing sphere used as the sky shell.
 * @stacks: Number of latitude divisions.
 * @slices: Number of longitude divisions.
 *
 * Normals point inward so the sphere is visible from its center.
 *
 * Return: A newly allocated mesh, or null on allocation failure.
 */
Mesh* GenerateSkySphere(int stacks, int slices) {
    Mesh* mesh = new Mesh();
    if (!mesh) return nullptr;
    int triangleCount = stacks * slices * 2;
    mesh->tris = new Triangle[triangleCount];
    if (!mesh->tris) {
        delete mesh;
        return nullptr;
    }
    mesh->triCount = 0;

    float radius = 800.0f;

    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            float phi0 = (float)i / stacks * PI;
            float phi1 = (float)(i + 1) / stacks * PI;
            float theta0 = (float)j / slices * TWO_PI;
            float theta1 = (float)(j + 1) / slices * TWO_PI;

            Vec3 p0(radius * sin(phi0) * cos(theta0), radius * cos(phi0),
                    radius * sin(phi0) * sin(theta0));
            Vec3 p1(radius * sin(phi1) * cos(theta0), radius * cos(phi1),
                    radius * sin(phi1) * sin(theta0));
            Vec3 p2(radius * sin(phi0) * cos(theta1), radius * cos(phi0),
                    radius * sin(phi0) * sin(theta1));
            Vec3 p3(radius * sin(phi1) * cos(theta1), radius * cos(phi1),
                    radius * sin(phi1) * sin(theta1));

            Vec2 uv0(1.0f - (float)j / slices, (float)i / stacks);
            Vec2 uv1(1.0f - (float)j / slices, (float)(i + 1) / stacks);
            Vec2 uv2(1.0f - (float)(j + 1) / slices, (float)i / stacks);
            Vec2 uv3(1.0f - (float)(j + 1) / slices, (float)(i + 1) / stacks);

            // Triangle 1.
            Triangle* t1 = &mesh->tris[mesh->triCount++];
            t1->p[0] = p0;
            t1->p[1] = p1;
            t1->p[2] = p2;
            t1->uv[0] = uv0;
            t1->uv[1] = uv1;
            t1->uv[2] = uv2;
            t1->n[0] = p0.Normalized() * -1.0f;
            t1->n[1] = p1.Normalized() * -1.0f;
            t1->n[2] = p2.Normalized() * -1.0f;

            // Triangle 2.
            Triangle* t2 = &mesh->tris[mesh->triCount++];
            t2->p[0] = p2;
            t2->p[1] = p1;
            t2->p[2] = p3;
            t2->uv[0] = uv2;
            t2->uv[1] = uv1;
            t2->uv[2] = uv3;
            t2->n[0] = p2.Normalized() * -1.0f;
            t2->n[1] = p1.Normalized() * -1.0f;
            t2->n[2] = p3.Normalized() * -1.0f;
        }
    }

    return mesh;
}

// --------------------------------------------------------------------------
// FILE LOADING HELPERS
// --------------------------------------------------------------------------

/**
 * LoadFileData() - Read a whole file from disk through the syscall API.
 * @filename: Absolute path of the file to load.
 * @outSize: Receives the number of bytes read.
 *
 * Return: A newly allocated buffer with the file contents, or null on error.
 */
uint8_t* LoadFileData(const char* filename, uint32_t* outSize) {
    // Buffer large enough for the textures (~2MB BMP files).
    uint32_t maxSize = 2 * 1024 * 1024 + 4096;
    uint8_t* buffer = new uint8_t[maxSize];
    if (!buffer) {
        printf("Failed to allocate file buffer for %s\n", filename);
        return nullptr;
    }

    int32_t fd = syscall_open(filename, 0);
    if (fd < 0) {
        printf("Failed to open file: %s\n", filename);
        delete[] buffer;
        return nullptr;
    }

    int32_t bytesRead = syscall_read(fd, (char*)buffer, maxSize);
    syscall_close(fd);

    if (bytesRead <= 0) {
        printf("Failed to read file: %s\n", filename);
        delete[] buffer;
        return nullptr;
    }

    if (outSize) *outSize = (uint32_t)bytesRead;
    printf("Loaded file: %s (%d bytes)\n", filename, bytesRead);
    return buffer;
}

// --------------------------------------------------------------------------
// UPSCALE BLIT
// --------------------------------------------------------------------------

/**
 * BlitUpscale() - Copy the render buffer onto the screen, scaling it up.
 * @dest: Destination framebuffer.
 * @destW: Destination width in pixels.
 * @destH: Destination height in pixels.
 * @src: Source render buffer.
 * @srcW: Source width in pixels.
 * @srcH: Source height in pixels.
 */
static void BlitUpscale(uint32_t* dest, int destW, int destH, uint32_t* src, int srcW, int srcH) {
    for (int y = 0; y < destH; y++) {
        int srcY = (y * srcH) / destH;
        int dstOff = y * destW;
        int srcOff = srcY * srcW;
        for (int x = 0; x < destW; x++) {
            int srcX = (x * srcW) / destW;
            dest[dstOff + x] = src[srcOff + srcX];
        }
    }
}

// --------------------------------------------------------------------------
// MAIN GAME
// --------------------------------------------------------------------------

/**
 * _start() - Application entry point for the 3D demo.
 * @arg: Program arguments passed by the loader.
 *
 * Initializes the framebuffer and renderer, loads the sky, stone and mesh
 * assets, then runs the render loop: poll input, cast shadows, draw the sky,
 * floor and walls, and upscale the internal buffer to the screen.
 */
extern "C" void _start(void* arg) {
    init_sys(arg);

    printf("[Game3D] Starting 3D Engine v2.0...\n");

    // Get the framebuffer.
    FramebufferInfo fb = syscall_get_framebuffer();
    uint32_t* screenBuffer = (uint32_t*)fb.buffer;
    int screenW = (int)fb.width;
    int screenH = (int)fb.height;
    printf("[Game3D] Framebuffer: %dx%d @ 0x%x\n", screenW, screenH, fb.buffer);

    // Allocate the internal render buffer.
    uint32_t* renderBuffer = new uint32_t[RENDER_W * RENDER_H];
    if (!renderBuffer) {
        printf("[Game3D] FATAL: Failed to allocate render buffer!\n");
        syscall_exit(1);
    }

    // Initialize the renderer at internal resolution.
    Renderer3D* renderer = new Renderer3D(RENDER_W, RENDER_H);
    if (!renderer) {
        printf("[Game3D] FATAL: Failed to allocate Renderer3D!\n");
        syscall_exit(1);
    }

    printf("[Game3D] Render at %dx%d, upscale to %dx%d\n", RENDER_W, RENDER_H, screenW, screenH);

    // Generate the low-poly sky sphere (8x16 = 256 triangles).
    Mesh* skyMesh = GenerateSkySphere(8, 16);

    // Load the textures from disk.
    Bitmap* skyTexture = nullptr;
    Bitmap* stoneTexture = nullptr;

    uint32_t fileSize = 0;
    uint8_t* fileData = nullptr;

    fileData = LoadFileData("Apps/Game3D/sky.bmp", &fileSize);
    if (fileData) {
        skyTexture = new Bitmap(fileData, fileSize);
        delete[] fileData;
        if (skyTexture && skyTexture->IsValid()) {
            renderer->SetSkybox(skyTexture);
            printf("[Game3D] Sky texture loaded\n");
        }
    }

    fileData = LoadFileData("Apps/Game3D/map.bmp", &fileSize);
    if (fileData) {
        stoneTexture = new Bitmap(fileData, fileSize);
        delete[] fileData;
        printf("[Game3D] Stone texture loaded\n");
    }

    // Load the meshes from disk.
    Mesh* wallMesh = nullptr;
    Mesh* floorMesh = nullptr;

    fileData = LoadFileData("Apps/Game3D/obj.obj", &fileSize);
    if (fileData) {
        wallMesh = renderer->LoadOBJ(fileData, fileSize);
        delete[] fileData;
        printf("[Game3D] Wall mesh loaded\n");
    }

    fileData = LoadFileData("Apps/Game3D/floor.obj", &fileSize);
    if (fileData) {
        floorMesh = renderer->LoadOBJ(fileData, fileSize);
        delete[] fileData;
        printf("[Game3D] Floor mesh loaded\n");
    }

    // Set up three-point lighting.
    Light sceneLights[3];
    int activeLightCount = 3;

    sceneLights[0] = Light(Vec3(-0.3f, -1.0f, -0.2f), 0.8f);
    sceneLights[1] = Light(Vec3(0.5f, -0.5f, 0.5f), 0.3f);
    sceneLights[2] = Light(Vec3(0.2f, 0.3f, 1.0f), 0.4f);

    // Set up shadow mapping from the primary light.
    renderer->SetupShadows(Vec3(-0.3f, -1.0f, -0.2f),  // Same as the primary light direction.
                           60.0f,                      // Ortho half-size (covers 120x120 units).
                           1.0f,                       // Near plane.
                           200.0f                      // Far plane.
    );

    // Camera state.
    float camX = 0.0f, camY = 5.0f, camZ = -10.0f;
    float camYaw = 0.0f, camPitch = 0.0f;

    // Input state.
    InputState input;

    printf("[Game3D] Entering main loop...\n");

    // ------------------------------------------------------------------------
    // MAIN GAME LOOP
    // ------------------------------------------------------------------------
    while (1) {
        // Poll input.
        syscall_get_input(&input);

        // Check for ESC to exit.
        if (input.keyStates[SC_ESC]) {
            printf("[Game3D] ESC pressed, exiting...\n");
            syscall_exit(0);
        }

        // Camera movement.
        float speed = 0.4f;
        float yaw = -camYaw;
        float forwardX = sin(yaw);
        float forwardZ = cos(yaw);
        float rightX = cos(yaw);
        float rightZ = -sin(yaw);

        if (input.keyStates[SC_W]) {
            camX += forwardX * speed;
            camZ += forwardZ * speed;
        }
        if (input.keyStates[SC_S]) {
            camX -= forwardX * speed;
            camZ -= forwardZ * speed;
        }
        if (input.keyStates[SC_A]) {
            camX -= rightX * speed;
            camZ -= rightZ * speed;
        }
        if (input.keyStates[SC_D]) {
            camX += rightX * speed;
            camZ += rightZ * speed;
        }
        if (input.keyStates[SC_SPACE]) camY += speed;
        if (input.keyStates[SC_LSHIFT]) camY -= speed;

        // Mouse look.
        float sensitivity = 0.005f;
        camYaw -= (float)input.mouseDX * sensitivity;
        camPitch += (float)input.mouseDY * sensitivity;
        if (camPitch > 1.5f) camPitch = 1.5f;
        if (camPitch < -1.5f) camPitch = -1.5f;

        // --------------------------------------------------------------------
        // SHADOW PASS - Render depth from the light's perspective. Only walls
        // cast shadows (floor self-shadowing causes acne artifacts).
        // --------------------------------------------------------------------
        renderer->BeginShadowPass(camX, camY, camZ);

        if (wallMesh) renderer->RenderMeshToShadowMap(wallMesh);

        renderer->EndShadowPass();

        // --------------------------------------------------------------------
        // RENDER TO INTERNAL BUFFER
        // --------------------------------------------------------------------
        renderer->Clear(renderBuffer, 0xFF87CEEB);

        // Draw the sky sphere.
        if (skyMesh && skyTexture) {
            renderer->SetMaterial(1.0f, 0.0f, 0.0f);
            renderer->BindTexture(skyTexture);
            renderer->DrawMesh(renderBuffer, skyMesh, camX, camY, camZ, camYaw, camPitch,
                               sceneLights, 0);
        }

        // Draw the floor.
        if (floorMesh) {
            renderer->SetMaterial(0.25f, 0.2f, 16.0f);
            renderer->BindTexture(stoneTexture);
            renderer->DrawMesh(renderBuffer, floorMesh, camX, camY, camZ, camYaw, camPitch,
                               sceneLights, activeLightCount);
        }

        // Draw the walls.
        if (wallMesh) {
            renderer->SetMaterial(0.2f, 0.3f, 8.0f);
            renderer->BindTexture(stoneTexture);
            renderer->DrawMesh(renderBuffer, wallMesh, camX, camY, camZ, camYaw, camPitch,
                               sceneLights, activeLightCount);
        }

        // Draw the crosshair.
        int cx = RENDER_W / 2;
        int cy = RENDER_H / 2;
        for (int i = -4; i <= 4; i++) {
            if (cx + i >= 0 && cx + i < RENDER_W) renderBuffer[cy * RENDER_W + cx + i] = 0xFFFFFFFF;
            if (cy + i >= 0 && cy + i < RENDER_H)
                renderBuffer[(cy + i) * RENDER_W + cx] = 0xFFFFFFFF;
        }

        // Upscale to the screen.
        BlitUpscale(screenBuffer, screenW, screenH, renderBuffer, RENDER_W, RENDER_H);

        // Frame delay (about 60 FPS).
        syscall_sleep(16);
    }
}
