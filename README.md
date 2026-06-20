# Convolution Engine

C++20 real-time Vulkan renderer and engine utilizing a dual-threaded frame execution model, clustered shading, ray tracing, and more

---

## Architecture & Threading Model

Convolution uses a **dual-threaded frame model** to parallelize CPU logic and GPU command recording:

*   **Main Thread:** Processes window input, game logic (ECS updates), and updates ImGui.
    *   *Entry point:* [Boot.cpp](Src/Boot.cpp)
    *   *Application lifecycle:* [Application.cpp](Src/Core/Application.cpp)
*   **Render Thread:** Handles sync with the ECS state, frame preprocessing, render pass submission, swapchain management, and GPU timing publication.
    *   *Render thread loop:* [RenderThread.cpp](Src/Core/RenderThread.cpp)
    *   *Front-end render layer:* [RenderLayer.cpp](Src/Core/Rendering/RenderLayer.cpp)
*   **Thread Safety:** Threads communicate using buffered state via [ApplicationState](Src/Core/Global/State/ApplicationState.h). Main-thread mutations are safely queued using `RegisterUpdateFunction`. Game and Render Thread state are synchronized once per frame, outside of that the render thread can not read game state and vice versa.

---

## ECS & Async Asset Pipeline

*   **Entity Component System (ECS):** Owned by the [EntityManager](Src/Core/ECS/EntityManager.h). Entities compose of modular components (`Transform`, `RenderComponent`, `Light`) and are updated on the main thread via decoupled systems (such as `STransform` or `SLight`).
*   **Mesh Loading Pipeline:** File formats are parsed via `MeshConverter` ([MeshConverter.h](Src/Core/IO/MeshConverter.h)), which maps data onto ECS entities and queues mesh uploads through the [SharedResourceManager](Src/Core/Rendering/Core/SharedResourceManager.h).
*   **Asynchronous I/O & GPU Transfers:**
    *   **File I/O:** The [FileReader](Src/Core/IO/FileReader.h) manages a dedicated I/O background thread and `ThreadPool` to process generic bytes, image/texture data (supporting DDS format parsing), and mesh data asynchronously, with callbacks on completion.
    *   **Texture Streaming:** The [VkTextureManager](Src/Core/Rendering/Vulkan/VkTextureManager.h) runs on a dedicated worker thread. It streams textures loaded by the file reader directly into GPU bindless arrays, using placeholder textures to keep rendering unblocked during loads.
    *   **Async Transfers:** The [TransferQueueHandler](Src/Core/Rendering/Core/TransferUtils/TransferQueueHandler.h) delegates GPU buffer copying and queue submissions to an internal thread pool.

---

## Core Features & Modules

### 1. Render Architecture

There is no framegraph yet so we drive the whole pipeline through a few huge central classe for now, expect to refactor this in the future.

#### PassManager
The [PassManager](Src/Core/Rendering/Passes/PassManager.h) is responsible for:
*   **Scheduling Execution Stages:** Rendering operations are grouped into stages defined by `PASS_SCHEDULE`. Passes within the same stage can run in parallel on graphics/compute queues, while stage boundaries are enforced sequentially.
*   **GPU-Side Synchronization:** Stages are synchronized using Vulkan timeline semaphores and fences but are grouped into as few commandbuffers as possible.
*   **State & Resource Propagation:** Prepares frame constants and handles resizing events by coordinating all resolution-dependent target updates to relevant subclasses.

#### RenderPass Lifecycle
Each pass inherits from [ConvolutionRenderPass](Src/Core/Rendering/Passes/RenderPass.h), implementing a strict lifecycle:
*   `Init(...)`: Called once at startup to setup pipeline layouts, compile initial shaders, allocate descriptor sets, and build static buffers.
*   `RecreateResolutionDependentResources(...)`: Invoked on viewport resize or upscaling state change to recreate render targets and re-write resolution-bound descriptors.
*   `RebuildInternalData(...)`: Rebuild pass-local data when mesh updates hit the render thread during the sync stage.
*   `Render(...)`: Appends graphics or compute commands into a provided command buffer.
*   `WantsToRender()`: Returns a boolean indicating if the pass should run this frame (e.g. skips RT passes if ray tracing is disabled or TLAS isn't ready).

### 2. Clustered Shading & Light Culling
To support thousands of dynamic lights, the view frustum is split into clusters. Computes culling and light grid assignment asynchronously.
*   [LightTransformComputePass](Src/Core/Rendering/Passes/ClusteredShading/LightTransformComputePass.h) - Updates light transforms.
*   [ClusterGeneratorComputePass](Src/Core/Rendering/Passes/ClusteredShading/ClusterGeneratorComputePass.h) - Computes coarse view/depth cluster grids.
*   [TileAssignmentComputePass](Src/Core/Rendering/Passes/ClusteredShading/TileAssignmentComputePass.h) - Performs culling and grids assignment.
*   [LightGridComputePass](Src/Core/Rendering/Passes/ClusteredShading/LightGridComputePass.h) - Builds final cluster-light grid indices.

### 3. Geometry & G-Buffer Pipeline
*   [DepthPrePass](Src/Core/Rendering/Passes/PreProcess/DepthPrePass.h) - Reversed Z depth pre-pass.
*   [StaticMainMeshPass](Src/Core/Rendering/Passes/StaticMeshPass.h) - Main mesh rendering pass.
*   [RenderTargetManager](Src/Core/Rendering/Core/RenderTargetManager.h) - Manages lifetime, history rotation, and scaling of color/depth/velocity textures.
*   [FrameTransitionRecorder](Src/Core/Rendering/Core/FrameTransitionRecorder.h) - Records Vulkan image layout transitions and synchronization barriers.

### 4. Shadows & Lighting
*   [LightingPass](Src/Core/Rendering/Passes/Compositing/LightingPass.h) - Executes the clustered lighting.
*   [CSMPass](Src/Core/Rendering/Passes/ShadowPass.h) - Records Cascaded Shadow Maps (CSM) for directional light, only one for now but will be extended to a more generic architectyre when implementing point light shadows.
*   [ScreenSpaceShadowPass](Src/Core/Rendering/Passes/ScreenSpaceShadowPass.h) - SSS pass using the Bend studio implementation.

### 5. Hardware-Accelerated Ray Tracing (Vulkan RT)
*   [RTSceneManager](Src/Core/Rendering/Core/RT/RTSceneManager.h) - Rebuilds Bottom-Level (BLAS) and Top-Level (TLAS) Acceleration Structures.
*   [RTReflectionsPass](Src/Core/Rendering/Passes/RT/RTReflectionsPass.h) - Ray-traced specular reflections, very noisy at the moment and will need some significant improvements to be actually usable.
*   [RTAOPass](Src/Core/Rendering/Passes/RT/RTAOPass.h) - Ray-traced ambient occlusion, same here.
*   [RTCompositePass](Src/Core/Rendering/Passes/RT/RTCompositePass.h) - Combines rasterized lighting with ray-traced shadows, AO, and reflections and does some simple accumulation.

### 6. Anti-Aliasing & Temporal Upscaling, heavy WIP
The engine supports multiple post-process anti-aliasing techniques and industry-standard upscalers, the temporal pass logic and math and so on is still not great:
*   [TAAPass](Src/Core/Rendering/Passes/AA/TAAPass.h) - Native Temporal Anti-Aliasing utilizing camera sub-pixel jitter and G-Buffer motion vectors.
*   [SMAAPass](Src/Core/Rendering/Passes/AA/SMAAPass.h) - Subpixel Morphological Anti-Aliasing.
*   [DLSSPass](Src/Core/Rendering/Passes/AA/DLSSPass.h) - NVIDIA DLSS Super Resolution integrated via Streamline.

### 7. Compositing & UI
*   [CompositPass](Src/Core/Rendering/Passes/Compositing/CompositPass.h) - Performs post-processing, tonemapping, and copy to final swapchain.
*   [ImGuiPass](Src/Core/Rendering/Passes/ImGuiPass.h) - Renders the editor interface, settings, and profiling/timing UI overlay onto the final swapchain image.

---

## Prerequisites & Building

### Prerequisites
1.  **Vulkan SDK 1.4** (with shader-device-address and RT features enabled)
2.  **Visual Studio 2022** (C++ Desktop development workload)
3.  **CMake** & **Git**

### Build Instructions
1.  Generate the project files:
    ```bash
    cmake -S . -B build
    ```
2.  Compile the configuration:
    ```bash
    cmake --build build --config Debug
    ```
3. Download the Resources folder from [MEGA](https://mega.nz/file/WAlCRD7T#ffCl3fJWD4FZmf_ta6iiJrSlBGHYgA2KpjFgsajCg84) and just place it in the root folder of the project (where this README.md file is).

*Note: All external libraries (GLFW, ImGui, EASTL) are downloaded/configured automatically via CMake.*
