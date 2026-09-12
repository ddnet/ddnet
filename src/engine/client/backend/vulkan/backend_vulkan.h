#ifndef ENGINE_CLIENT_BACKEND_VULKAN_BACKEND_VULKAN_H
#define ENGINE_CLIENT_BACKEND_VULKAN_BACKEND_VULKAN_H

class CCommandProcessorFragment_GLBase;

static constexpr int BACKEND_VULKAN_VERSION_MAJOR = 1;
static constexpr int BACKEND_VULKAN_VERSION_MINOR = 1;

class CVulkanCapabilities
{
public:
	// Render into images owned by the backend instead of a window's swap chain.
	bool m_Headless;
};

CCommandProcessorFragment_GLBase *CreateVulkanCommandProcessorFragment(const CVulkanCapabilities &Capabilities);

#endif
