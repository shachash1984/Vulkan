#include "Mesh.h"


Mesh::Mesh(VkPhysicalDevice newPhysicalDevice, VkDevice newDevice, std::vector<Vertex>* vertices)
	: m_ullVertexCount(vertices->size())
	, m_PhysicalDevice(newPhysicalDevice)
	, m_Device(newDevice)
	, vertices_(vertices)
{
	CreateVertexBuffer(vertices);
}

unsigned long long Mesh::GetVertexCount() const
{
	return m_ullVertexCount;
}

VkBuffer Mesh::GetVertexBuffer() const
{
	return m_VertexBuffer;
}

void Mesh::DestroyVertexBuffer() const
{
	vkDestroyBuffer(m_Device, m_VertexBuffer, nullptr);
	vkFreeMemory(m_Device, m_VertexBufferMemory, nullptr);
}

void Mesh::CreateVertexBuffer(const std::vector<Vertex>* vertices)
{
	// CREATE VERTEX BUFFER
	// Information to create a buffer (doesn't include assigning memory - just a header)
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(Vertex) * vertices->size();			// Size of buffer (size of 1 vertex * number of vertices)
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;			// Multiple types of buffer possible, we want vertex buffer
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;				// Similar to Swap Chain images, can share vertex buffers

	VkResult result = vkCreateBuffer(m_Device, &bufferInfo, nullptr, &m_VertexBuffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Vertex Buffer");
	}

	// GET BUFFER MEMORY REQUIREMENTS
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(m_Device, m_VertexBuffer, &memRequirements);

	// ALLOCATE MEMORY TO BUFFER
	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memRequirements.size;
	uint32_t memTypeIndex = 0xFFFFFFFF;
	constexpr VkMemoryPropertyFlags compatibleProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	result = FindMemoryTypeIndex(memRequirements.memoryTypeBits, compatibleProperties, memTypeIndex);
				// Index of memory type on Physical Device that has required bit flags
				// VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : CPU can interact with memory
				// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT: Allows placement of data straight into buffer after mapping (otherwise would have to specify manually)
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to find memory type index");
	}

	memoryAllocateInfo.memoryTypeIndex = memTypeIndex;

	// Allocate memory to VkDeviceMemory
	result = vkAllocateMemory(m_Device, &memoryAllocateInfo, nullptr, &m_VertexBufferMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate Vertex Buffer Memory");
	}

	// Allocate memory to given vertex buffer
	vkBindBufferMemory(m_Device, m_VertexBuffer, m_VertexBufferMemory, 0);

	// MAP MEMORY TO VERTEX BUFFER
	void* data;																									// 1. Create pointer to a point in normal memory
	vkMapMemory(m_Device, m_VertexBufferMemory, 0, bufferInfo.size, 0, &data);		// 2. "Map" the vertex buffer memory to that point
	memcpy(data, vertices->data(), bufferInfo.size);											// 3. Copy memory from vertices vector to the point
	vkUnmapMemory(m_Device, m_VertexBufferMemory);																// 4. Unmap the vertex buffer memory

}																										

VkResult Mesh::FindMemoryTypeIndex(uint32_t allowedTypes, VkMemoryPropertyFlags properties, uint32_t& outTypeIndex) const
{
	// Get properties of physical device memory
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memoryProperties);

	for (uint32_t i = 0; i<memoryProperties.memoryTypeCount; i++)
	{
		if ((allowedTypes & (1 << i))														// Index of memory type must match corresponding bit in allowedTypes
			&& (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)	// Desired property bit flags are part of memory types property flags
		{
			// This memory type is valid so return its index
			outTypeIndex = i;
			return VK_SUCCESS;
		}
	}
	return VK_ERROR_MEMORY_MAP_FAILED;
}
