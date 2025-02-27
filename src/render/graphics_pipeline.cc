#include "graphics_pipeline.h"

#include <array>
#include <vector>

#include "glm/glm/glm.hpp"

#include "common.h"
#include "render/data_types.h"

#include "global.h"

namespace render
{

	GraphicsPipeline::GraphicsPipeline(const Global& global, const RenderNode& render_node, const ShaderModule& vertex_shader_module, const ShaderModule& fragment_shader_module, const std::array<Extent, kExtentTypeCnt>& extents, PrimitiveFlags required_primitive_flags, Params params) :
		RenderObjBase(global), layout_(VK_NULL_HANDLE), required_primitive_flags_(required_primitive_flags), vertex_bindings_count_(0)
	{
		InitPipeline(render_node, vertex_shader_module, {}, fragment_shader_module, extents, params);
	}

	GraphicsPipeline::GraphicsPipeline(const Global& global, const RenderNode& render_node, util::NullableRef<const ShaderModule> vertex_shader_module, util::NullableRef<const ShaderModule> geometry_shader_module, util::NullableRef<const ShaderModule> fragment_shader_module, const std::array<Extent, kExtentTypeCnt>& extents, PrimitiveFlags required_primitive_flags, Params params) :
		RenderObjBase(global), layout_(VK_NULL_HANDLE), required_primitive_flags_(required_primitive_flags), vertex_bindings_count_(0)
	{
		InitPipeline(render_node, vertex_shader_module, geometry_shader_module, fragment_shader_module, extents, params);
	}

	const std::map<uint32_t, const DescriptorSetLayout&>& GraphicsPipeline::GetDescriptorSetLayouts() const
	{
		return descriptor_sets_;
	}

	const std::map<uint32_t, ShaderModule::VertexBindingDesc>& GraphicsPipeline::GetVertexBindingsDescs() const
	{
		return vertex_bindings_descs_;
	}

	PrimitiveFlags GraphicsPipeline::GetRequiredPrimitiveFlags() const
	{
		return required_primitive_flags_;
	}

	const VkPipelineLayout& GraphicsPipeline::GetLayout() const
	{
		return layout_;
	}

	uint32_t GraphicsPipeline::GetVertexBindingsCount() const
	{
		return vertex_bindings_count_;
	}

	GraphicsPipeline::~GraphicsPipeline()
	{
		if (handle_ != VK_NULL_HANDLE)
		{
			vkDestroyPipeline(global_.logical_device, handle_, nullptr);

			if (layout_ != VK_NULL_HANDLE)
			{
				vkDestroyPipelineLayout(global_.logical_device, layout_, nullptr);
			}
		}
	}

	bool GraphicsPipeline::InitPipeline(const RenderNode& render_node, util::NullableRef<const ShaderModule> vertex_shader_module, util::NullableRef<const ShaderModule> geometry_shader_module,
		util::NullableRef<const ShaderModule> fragment_shader_module, const std::array<Extent, kExtentTypeCnt>& extents, Params params)
	{
		std::vector<VkPipelineShaderStageCreateInfo> shader_stage_create_infos;
		std::vector<VkVertexInputBindingDescription> vertex_input_bindings_descs;
		std::vector<VkVertexInputAttributeDescription> vertex_input_attr_descs;

		{
			VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
			vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vert_shader_stage_info.module = vertex_shader_module->GetHandle();
			vert_shader_stage_info.pName = "main"; // entry point in shader

			if (!vertex_shader_module->GetInputBindingsDescs().empty())
			{
				vertex_input_bindings_descs = BuildVertexInputBindingDescriptions(vertex_shader_module->GetInputBindingsDescs());
				vertex_input_attr_descs = BuildVertexAttributeDescription(vertex_shader_module->GetInputBindingsDescs());
				vertex_bindings_count_ = u32(vertex_input_bindings_descs.size());
			}

			shader_stage_create_infos.push_back(vert_shader_stage_info);

			vertex_bindings_descs_ = vertex_shader_module->GetInputBindingsDescs();
		}

		if (geometry_shader_module)
		{
			VkPipelineShaderStageCreateInfo shader_stage_info{};
			shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shader_stage_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
			shader_stage_info.module = geometry_shader_module->GetHandle();
			shader_stage_info.pName = "main"; // entry point in shader

			shader_stage_create_infos.push_back(shader_stage_info);
		}

		if (fragment_shader_module)
		{
			VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
			frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			frag_shader_stage_info.module = fragment_shader_module->GetHandle();
			frag_shader_stage_info.pName = "main"; // entry point in shader

			shader_stage_create_infos.push_back(frag_shader_stage_info);
		}

		VkPipelineVertexInputStateCreateInfo vertex_input_info{};
		vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_info.vertexBindingDescriptionCount = u32(vertex_input_bindings_descs.size());
		vertex_input_info.pVertexBindingDescriptions = vertex_input_bindings_descs.data(); // Optional
		vertex_input_info.vertexAttributeDescriptionCount = u32(vertex_input_attr_descs.size());
		vertex_input_info.pVertexAttributeDescriptions = vertex_input_attr_descs.data(); // Optional

		VkPipelineInputAssemblyStateCreateInfo input_assembly{};
		input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly.topology = params.Check(EParams::kLineTopology) ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : params.Check(EParams::kPointTopology) ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		input_assembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(extents[u32(render_node.GetExtentType())].width);
		viewport.height = static_cast<float>(extents[u32(render_node.GetExtentType())].height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = extents[u32(render_node.GetExtentType())];

		VkPipelineViewportStateCreateInfo viewport_state{};
		viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_state.viewportCount = 1;
		viewport_state.pViewports = &viewport;
		viewport_state.scissorCount = 1;
		viewport_state.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = !fragment_shader_module; //then geometry never passes through the rasterizer stage. This basically disables any output to the framebuffer.
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = params.Check(EParams::kPointTopology) ? 10.0f : 1.0f;
		rasterizer.cullMode = params.Check(EParams::kDepthBias) ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

		if (params.Check(EParams::kDepthBias))
		{
			rasterizer.depthBiasEnable = VK_TRUE;
			rasterizer.depthBiasConstantFactor = 10.0f; // Optional
			rasterizer.depthBiasClamp = 0.0f; // Optional
			rasterizer.depthBiasSlopeFactor = 1.0f; // Optional

		}
		else
		{
			rasterizer.depthBiasEnable = VK_FALSE;
			rasterizer.depthBiasConstantFactor = 0.0f; // Optional
			rasterizer.depthBiasClamp = 0.0f; // Optional
			rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

		}



		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f; // Optional
		multisampling.pSampleMask = nullptr; // Optional
		multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
		multisampling.alphaToOneEnable = VK_FALSE; // Optional

		//TODO: configure blend

		int color_attachments_cnt = 0;

		for (auto&& attachment : render_node.GetAttachments())
		{
			if (attachment.format_type != FormatType::kDepth)
				color_attachments_cnt++;
		}


		std::vector<VkPipelineColorBlendAttachmentState> blend_attachment_states(color_attachments_cnt);

		for (auto&& color_blend_attachment : blend_attachment_states)
		{
			color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			color_blend_attachment.blendEnable = VK_TRUE;
			color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; // Optional
			color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // Optional
			color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
			color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
			color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
			color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
		}



		VkPipelineColorBlendStateCreateInfo color_blending{};
		color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blending.logicOpEnable = VK_FALSE;
		color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
		color_blending.attachmentCount = u32(blend_attachment_states.size());
		color_blending.pAttachments = blend_attachment_states.data();
		color_blending.blendConstants[0] = 0.0f; // Optional
		color_blending.blendConstants[1] = 0.0f; // Optional
		color_blending.blendConstants[2] = 0.0f; // Optional
		color_blending.blendConstants[3] = 0.0f; // Optional

		VkPipelineDepthStencilStateCreateInfo depth_stencil{};
		depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil.depthTestEnable = !params.Check(EParams::kDisableDepthTest);
		depth_stencil.depthWriteEnable = VK_TRUE;
		depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depth_stencil.depthBoundsTestEnable = VK_FALSE;
		depth_stencil.minDepthBounds = -1.0f; // Optional
		depth_stencil.maxDepthBounds = 1.0f; // Optional
		depth_stencil.stencilTestEnable = VK_FALSE;
		depth_stencil.front = {}; // Optional
		depth_stencil.back = {}; // Optional


		VkPushConstantRange vertex_push_constant{};
		vertex_push_constant.offset = 0;
		vertex_push_constant.size = sizeof(VertexPushConstants);
		vertex_push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkPushConstantRange fragment_push_constant{};
		fragment_push_constant.offset = vertex_push_constant.size;
		fragment_push_constant.size = sizeof(FragmentPushConstants);
		fragment_push_constant.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		std::vector<VkPushConstantRange> push_constants = { vertex_push_constant, fragment_push_constant };

		for (auto&& [set_index, set_layout] : vertex_shader_module->GetDescriptorSets())
		{
			if (auto&& existing_set = descriptor_sets_.find(set_index); existing_set != descriptor_sets_.end())
			{
				if (existing_set->second.GetType() != set_layout.GetType())
				{
					throw std::runtime_error("invalid descripton set combination");
				}
			}

			descriptor_sets_.emplace(set_index, set_layout);
		}

		if (geometry_shader_module)
		{
			for (auto&& [set_index, set_layout] : geometry_shader_module->GetDescriptorSets())
			{
				if (auto&& existing_set = descriptor_sets_.find(set_index); existing_set != descriptor_sets_.end())
				{
					if (existing_set->second.GetType() != set_layout.GetType())
					{
						throw std::runtime_error("invalid descripton set combination");
					}
				}

				descriptor_sets_.emplace(set_index, set_layout);
			}
		}

		if (fragment_shader_module)
		{
			for (auto&& [set_index, set_layout] : fragment_shader_module->GetDescriptorSets())
			{
				if (auto&& existing_set = descriptor_sets_.find(set_index); existing_set != descriptor_sets_.end())
				{
					if (existing_set->second.GetType() != set_layout.GetType())
					{
						throw std::runtime_error("invalid descripton set combination");
					}
				}

				descriptor_sets_.emplace(set_index, set_layout);
			}
		}

		std::vector<VkDescriptorSetLayout> descriptor_sets_layouts;

		for (auto&& [set_index, set_layout] : descriptor_sets_)
		{
			descriptor_sets_layouts.push_back(set_layout.GetHandle());
		}

		//int processed_desc_layouts = 0;

		//for (int i = 0; processed_desc_layouts < descriptor_sets_.size(); i++)
		//{
		//	if (descriptor_sets_.find(i) != descriptor_sets_.end())
		//	{
		//		descriptor_sets_layouts.push_back(descriptor_sets_.at(i).GetHandle());
		//		processed_desc_layouts++;
		//	}
		//	else
		//	{
		//		//descriptor_sets_layouts.push_back(VK_NULL_HANDLE);
		//	}
		//}


		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = u32(descriptor_sets_layouts.size());
		pipelineLayoutInfo.pSetLayouts = descriptor_sets_layouts.data(); // Optional
		pipelineLayoutInfo.pushConstantRangeCount = u32(push_constants.size()); // Optional
		pipelineLayoutInfo.pPushConstantRanges = push_constants.data(); // Optional

		if (vkCreatePipelineLayout(global_.logical_device, &pipelineLayoutInfo, nullptr, &layout_) != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}

		VkGraphicsPipelineCreateInfo pipeline_info{};
		pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_info.stageCount = u32(shader_stage_create_infos.size());
		pipeline_info.pStages = shader_stage_create_infos.data();

		pipeline_info.pVertexInputState = &vertex_input_info;
		pipeline_info.pInputAssemblyState = &input_assembly;
		pipeline_info.pViewportState = &viewport_state;
		pipeline_info.pRasterizationState = &rasterizer;
		pipeline_info.pMultisampleState = &multisampling;
		pipeline_info.pDepthStencilState = nullptr; // Optional
		pipeline_info.pColorBlendState = &color_blending;
		pipeline_info.pDepthStencilState = &depth_stencil;
		pipeline_info.pDynamicState = nullptr; // Optional

		pipeline_info.layout = layout_;

		pipeline_info.renderPass = render_node.GetRenderPass().GetHandle();
		pipeline_info.subpass = 0;

		pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // Optional
		pipeline_info.basePipelineIndex = -1; // Optional

		if (vkCreateGraphicsPipelines(global_.logical_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &handle_) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphics pipeline!");
		}

		return true;
	}

	std::vector<VkVertexInputBindingDescription> GraphicsPipeline::BuildVertexInputBindingDescriptions(const std::map<uint32_t, ShaderModule::VertexBindingDesc>& vertex_bindings_descs)
	{
		std::vector<VkVertexInputBindingDescription> result(vertex_bindings_descs.size());
		int res_index = 0;

		for (auto&& [binding_index, binding_desc] : vertex_bindings_descs)
		{
			result[res_index].binding = binding_index;
			result[res_index].stride = binding_desc.stride;
			result[res_index].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			res_index++;
		}

		return result;
	}

	std::vector<VkVertexInputAttributeDescription> GraphicsPipeline::BuildVertexAttributeDescription(const std::map<uint32_t, ShaderModule::VertexBindingDesc>& vertex_bindings_descs)
	{
		std::vector<VkVertexInputAttributeDescription> result;

		for (auto&& [binding_index, binding_desc] : vertex_bindings_descs)
		{
			for (auto&& [location, attr_desc] : binding_desc.attributes)
			{
				VkVertexInputAttributeDescription vk_attr_desc;

				vk_attr_desc.binding = binding_index;
				vk_attr_desc.format = static_cast<VkFormat>(attr_desc.format);
				vk_attr_desc.location = location;
				vk_attr_desc.offset = attr_desc.offset;

				result.push_back(vk_attr_desc);
			}
		}

		return result;
	}
}