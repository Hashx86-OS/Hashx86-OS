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

#ifndef RENDERER3D_H
#define RENDERER3D_H

#include <Bitmap.h>
#include <Math3D.h>

/**
 * struct Light - A directional light source.
 * @direction: Light direction, normalized on construction.
 * @intensity: Light intensity multiplier.
 */
struct Light {
    Vec3 direction;
    float intensity;
    Light() : direction(Vec3(0, -1, 0)), intensity(1.0f) {}
    Light(Vec3 dir, float inten) : direction(dir), intensity(inten) {
        direction.Normalize();
    }
};

/**
 * struct Vertex - A per-vertex record used during rasterization and clipping.
 * @pos: 3D position (camera space before projection, screen space after).
 * @uv: Texture coordinate.
 * @normal: Surface normal.
 * @worldPos: World-space position, used for shadow lookup.
 * @light: Pre-calculated light intensity.
 */
struct Vertex {
    Vec3 pos;       // 3D position (camera space before projection, screen space after).
    Vec2 uv;        // Texture coordinate.
    Vec3 normal;    // Surface normal.
    Vec3 worldPos;  // World-space position, used for shadow lookup.
    float light;    // Pre-calculated light intensity.
};

// Shadow map configuration.
#define SHADOW_MAP_SIZE 512
#define SHADOW_BIAS 0.5f

/**
 * class Renderer3D - Software 3D rasterizer.
 *
 * Transforms and rasterizes meshes into a caller-provided 32-bit framebuffer,
 * with optional texturing, simple lighting, backface culling and shadow
 * mapping.
 */
class Renderer3D {
private:
    int width, height;
    float* zBuffer;
    float halfWidth, halfHeight;

    // Rasterization settings.
    bool enableBackfaceCulling;
    bool enableTexturing;

    // Current texture state.
    Bitmap* currentTexture;
    Bitmap* skybox;

    uint32_t texWidthMask, texHeightMask;
    int texWidthShift;

    // Lighting settings.
    float ambientStrength;
    float specularStrength;
    float shininess;
    bool enableZRead;
    bool enableZWrite;
    bool enableLighting;

    // Shadow map state.
    float* shadowMap;
    bool shadowsEnabled;
    Vec3 shadowLightDir;    // Normalized light direction for shadow casting.
    float shadowOrthoSize;  // Half-size of the orthographic shadow volume.
    float shadowNear, shadowFar;
    // Shadow light-space transform cache.
    float slCosY, slSinY, slCosP, slSinP;
    float slCenterX, slCenterY, slCenterZ;

    // Skybox camera state.
    float skyYaw, skyPitch;

    // OBJ parsing helpers.
    float ParseFloat(char*& ptr);
    int ParseInt(char*& ptr);
    void SkipWhitespace(char*& ptr);

    float CalculateLighting(const Vec3& normal, const Vec3& viewDir, Light* lights, int lightCount);

    void ClipTriangle(Vertex v1, Vertex v2, Vertex v3, uint32_t* buffer);

    // Shadow map internals.
    void RasterizeShadowTriangle(Vec3 p0, Vec3 p1, Vec3 p2);
    float SampleShadow(const Vec3& worldPos);
    Vec3 WorldToShadowUV(const Vec3& worldPos);

public:
    Renderer3D(int w, int h);
    ~Renderer3D();

    void Clear(uint32_t* buffer, uint32_t color);
    void ClearSky(uint32_t* buffer, float camYaw, float camPitch);

    void DrawMesh(uint32_t* buffer, Mesh* mesh, float camX, float camY, float camZ, float camYaw,
                  float camPitch, Light* lights, int lightCount);

    void FillTriangle(uint32_t* buffer, Vertex v1, Vertex v2, Vertex v3);

    void BindTexture(Bitmap* texture);
    void SetSkybox(Bitmap* skyTexture);
    void SetMaterial(float ambient, float specular, float shininess);
    Mesh* LoadOBJ(uint8_t* data, uint32_t dataSize);

    // Shadow map API.
    void SetupShadows(const Vec3& lightDir, float orthoSize, float nearPlane, float farPlane);
    void BeginShadowPass(float centerX, float centerY, float centerZ);
    void RenderMeshToShadowMap(Mesh* mesh);
    void EndShadowPass();
    void EnableShadows(bool enable) {
        shadowsEnabled = enable;
    }
};

#endif  // RENDERER3D_H
