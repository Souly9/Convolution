#pragma once
#include "Core/IO/DecodedScene.h"
#include <assimp/scene.h>

namespace MeshDecoder
{
// Converts an imported scene to plain data. Runs on the decode pool, so it must not touch engine or renderer state.
DecodedScene Decode(const aiScene* pScene);
} // namespace MeshDecoder
