#ifndef RENDER_ENGINE_RENDER_DATA_TYPES_H_
#define RENDER_ENGINE_RENDER_DATA_TYPES_H_

#include <vector>
#include <array>

#include "vulkan/vulkan.h"
#include "glm/glm/glm.hpp"

#include "common.h"
#include "render/object_base.h"

namespace render
{
	struct Vertex {
		glm::vec2 pos;
		glm::vec3 color;

		static VkVertexInputBindingDescription GetBindingDescription()
		{
			VkVertexInputBindingDescription binding_description{};

			binding_description.binding = 0;
			binding_description.stride = sizeof(Vertex);
			binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return binding_description;
		}

		static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions() {
			std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions{};

			attribute_descriptions[0].binding = 0;
			attribute_descriptions[0].location = 0;
			attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT; // we can use more component but they will be discarted
			attribute_descriptions[0].offset = offsetof(Vertex, pos);

			attribute_descriptions[1].binding = 0;
			attribute_descriptions[1].location = 1;
			attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attribute_descriptions[1].offset = offsetof(Vertex, color);


			return attribute_descriptions;
		}
	};

}
#endif  // RENDER_ENGINE_RENDER_DATA_TYPES_H_