#ifndef DEVELOPER_VK_FIXTURE_H
#define DEVELOPER_VK_FIXTURE_H

/*
 * Developer entry point for the experimental native-Vulkan backend (renderer
 * roadmap R7). It opens its own SDL window and renders the owned modern-mesh
 * fixtures through PsyX_Vk: the mesh/material/light scene, a shadow map,
 * swapchain presentation, resize, RGBA readback and the ImGui overlay.
 *
 * The game itself is not started and no OpenGL context is created.
 */

enum class DeveloperVkFixtureResult { NotRequested, Success, Failure };

/* Requested fixtures own application startup. The caller must propagate Failure
   as a nonzero process exit code; unrelated arguments continue normal startup. */
DeveloperVkFixtureResult DeveloperVkFixture_HandleCommandLine(int argc, char** argv);

#endif // DEVELOPER_VK_FIXTURE_H
