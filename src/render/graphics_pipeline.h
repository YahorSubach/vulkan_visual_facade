#ifndef RENDER_ENGINE_RENDER_GRAPHICS_PIPELINE_H_
#define RENDER_ENGINE_RENDER_GRAPHICS_PIPELINE_H_


#include "vulkan/vulkan.h"

#include "render/object_base.h"
#include "render/render_pass.h"

namespace render
{
	class GraphicsPipeline : public RenderObjBase<VkPipeline>
	{
	public:
		GraphicsPipeline(const VkDevice& device, const VkShaderModule& vert_shader_module, const VkShaderModule& frag_shader_module, const VkExtent2D& extent, const RenderPass& render_pass);

		GraphicsPipeline(const GraphicsPipeline&) = delete;
		GraphicsPipeline(GraphicsPipeline&&) = default;

		GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;
		GraphicsPipeline& operator=(GraphicsPipeline&&) = default;

		const VkDescriptorSetLayout& GetDescriptorSetLayout() const;
		const VkPipelineLayout& GetLayout() const;

		virtual ~GraphicsPipeline() override;
	private:
		VkDescriptorSetLayout descriptor_set_layot_;
		VkPipelineLayout layout_;
	};
}
#endif  // RENDER_ENGINE_RENDER_GRAPHICS_PIPELINE_H_