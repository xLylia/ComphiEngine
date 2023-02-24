#include "cphipch.h"
#include "Camera.h"

namespace Comphi::Vulkan {

	glm::mat4 Camera::getProjectionMatrix()
	{
		glm::mat4 projectionMatrix = glm::perspective(
			glm::radians(properties.FOV),
			(float)GraphicsHandler::get()->swapChainExtent->width / GraphicsHandler::get()->swapChainExtent->height,
			properties.NearPlane, properties.FarPlane);
		projectionMatrix[1][1] *= -1;
		
		return projectionMatrix;
	}
}