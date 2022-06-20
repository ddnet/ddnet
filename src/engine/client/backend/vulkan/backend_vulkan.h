#ifndef ENGINE_CLIENT_BACKEND_VULKAN_BACKEND_VULKAN_H
#define ENGINE_CLIENT_BACKEND_VULKAN_BACKEND_VULKAN_H

#include <engine/client/backend/backend_base.h>

static constexpr int gs_BackendVulkanMajor = 1;
static constexpr int gs_BackendVulkanMinor = 0;

CCommandProcessorFragment_GLBase *CreateVulkanCommandProcessorFragment();

#endif
