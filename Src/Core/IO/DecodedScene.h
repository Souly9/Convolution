#pragma once
#include "Core/ECS/Components/Light.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Material.h"
#include "Core/Rendering/Core/TextureManagerTypes.h"
#include "Core/SceneGraph/Mesh.h"

// Plain data an import worker produces, the render owner thread turns it into entities and GPU resources

struct DecodedTextureRef
{
    // Already prefixed with the models folder
    stltype::string path;
    TextureSemantic semantic;
    // The material field that receives the bindless slot
    u32 Material::* slot;
};

struct DecodedMaterial
{
    stltype::string name;
    // Flag bits are set, the texture slots are filled in when the textures are requested
    Material params;
    stltype::vector<DecodedTextureRef> textures;
};

struct DecodedMesh
{
    stltype::string name;
    Mesh mesh;
    // Vertex plus index bytes
    u64 geometryBytes{0};
    u32 materialIndex{0};
    mathstl::Vector3 aabbMin;
    mathstl::Vector3 aabbMax;
};

struct DecodedNode
{
    stltype::string name;
    // -1 for the root
    s32 parentIndex{-1};
    mathstl::Vector3 position;
    mathstl::Vector3 rotationDegrees;
    mathstl::Vector3 scale;
    // The node's meshes are the range [firstMesh, firstMesh + meshCount) of DecodedScene::meshes
    u32 firstMesh{0};
    u32 meshCount{0};
    bool hasLight{false};
    ECS::Components::Light light;
};

struct DecodedCamera
{
    bool present{false};
    mathstl::Vector3 position;
    f32 yawDegrees{0.f};
    f32 pitchDegrees{0.f};
};

struct DecodedScene
{
    // Parents come before their children
    stltype::vector<DecodedNode> nodes;
    stltype::vector<DecodedMesh> meshes;
    stltype::vector<DecodedMaterial> materials;
    DecodedCamera camera;
    u64 totalVertexBytes{0};
    u64 totalIndexBytes{0};
    f32 decodeMs{0.f};
};
