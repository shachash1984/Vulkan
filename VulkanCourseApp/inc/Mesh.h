#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include "Utilities.h"

class Mesh
{
public:
	Mesh() = default;
	Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, std::vector<Vertex>* vertices);

	unsigned long long GetVertexCount() const;
	VkBuffer GetVertexBuffer() const;

	void DestroyVertexBuffer() const;

	~Mesh() = default;

protected:
	unsigned long long m_ullVertexCount;
	VkBuffer m_VertexBuffer{};
	VkDeviceMemory m_VertexBufferMemory{};

	VkPhysicalDevice m_PhysicalDevice;
	VkDevice m_Device;
	std::vector<Vertex>* vertices_;

	void CreateVertexBuffer(const std::vector<Vertex>* vertices);
	VkResult FindMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties, uint32_t& outTypeIndex) const;
};

