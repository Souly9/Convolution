#pragma once
// CommonGlobals.h - One-stop include for all global variables with full type
// definitions Include this header when you need to USE the globals (call
// methods on them) For headers that only need forward declarations, include
// GlobalVariables.h instead

#include "CoreCommon.h"
#include "GlobalVariables.h"

// Full type definitions for all global manager types
#include "Core/ECS/EntityManager.h"
#include "Core/Events/EventSystem.h"
#include "Core/IO/FileReader.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include "Core/SceneGraph/Mesh.h"
#include "State/ApplicationState.h"
