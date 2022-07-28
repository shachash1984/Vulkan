#pragma once
#include <vector>
#include<fstream>
#include<glm/glm.hpp>

constexpr int MAX_FRAME_DRAWS = 2;

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Vertex data representation
struct Vertex
{
	glm::vec3 pos; // Vertex Position (x, y, z)
	glm::vec3 col; // Vertex Color (r, g, b)
};


//Indices (locations) of Queue Families (if they exist at all)

struct QueueFamilyIndices
{
	int graphicsFamily = -1;			// Location of Graphics Queue family
	int presentationFamily = -1;			// Location of Presentation Queue family

	// check if queue families are valid
	bool IsValid() const
	{
		return graphicsFamily >= 0 && presentationFamily >= 0;
	}
};

struct SwapChainDetails
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities{};		// Surface properties (e.g image size/extent)
	std::vector<VkSurfaceFormatKHR> formats;			// Surface image formats, e.g RGBA and size of each color
	std::vector<VkPresentModeKHR> presentationModes;	// How images should be presented to screen
};

struct SwapChainImage
{
	VkImage image;
	VkImageView imageView;
};

static std::vector<char> ReadFile(const std::string& fileName)
{
	// Open stream from given file
	// std::ios::binary tells stream to read file as binary
	// std::ios::ate tells stream to start reading from end of file
	std::ifstream file(fileName, std::ios::binary | std::ios::ate);

	// Check if file stream successfully opened
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open file: " + fileName);
	}

	// Get current read position and use to resize file buffer
	const size_t fileSize = (size_t)file.tellg();
	std::vector<char> fileBuffer(fileSize);

	// Move read position (seek to) the start of the file
	file.seekg(0);

	// Read the file data into the buffer (stream "fileSize" in total)
	file.read(fileBuffer.data(), static_cast<long long>(fileSize));

	// Close stream
	file.close();

	return fileBuffer;
}