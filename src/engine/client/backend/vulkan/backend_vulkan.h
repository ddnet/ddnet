#ifndef ENGINE_CLIENT_BACKEND_VULKAN_BACKEND_VULKAN_H
#define ENGINE_CLIENT_BACKEND_VULKAN_BACKEND_VULKAN_H

class CCommandProcessorFragmentGlBase;

static constexpr int gs_BackendVulkanMajor = 1;
static constexpr int gs_BackendVulkanMinor = 1;

CCommandProcessorFragmentGlBase *CreateVulkanCommandProcessorFragment();

#endif
