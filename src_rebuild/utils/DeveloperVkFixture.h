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

/* Returns 1 when the command line asked for the fixture and it ran (the caller
   should then exit), 0 when the arguments are unrelated to it. */
int DeveloperVkFixture_HandleCommandLine(int argc, char** argv);

#endif // DEVELOPER_VK_FIXTURE_H
