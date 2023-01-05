#include "frame_handler.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <utility>

#include "vk_util.h"
#include <render/data_types.h>
#include <render/command_buffer_filler.h>

render::FrameHandler::FrameHandler(const DeviceConfiguration& device_cfg, const Swapchain& swapchain, const RenderSetup& render_setup, 
	const std::array<Extent, kExtentTypeCnt>& extents, DescriptorSetsManager& descriptor_set_manager, const BatchesManager& batches_manager, 
	const ui::UI& ui, const Scene& scene, DebugGeometry& debug_geometry) :
	RenderObjBase(device_cfg), swapchain_(swapchain.GetHandle()), graphics_queue_(device_cfg.graphics_queue),
	command_buffer_(device_cfg.graphics_cmd_pool->GetCommandBuffer()),
	image_available_semaphore_(vk_util::CreateSemaphore(device_cfg.logical_device)),
	render_finished_semaphore_(vk_util::CreateSemaphore(device_cfg.logical_device)),
	cmd_buffer_fence_(vk_util::CreateFence(device_cfg.logical_device)), present_info_{}, submit_info_{}, wait_stages_(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT),
	model_scene_(device_cfg, batches_manager, scene),
	render_setup_(render_setup),
	render_graph_handler_(device_cfg, render_setup.GetRenderGraph(), extents, descriptor_set_manager),
	ui_scene_(device_cfg, ui),
	ui_(ui),
	viewport_vertex_buffer_(device_cfg_, 6 * sizeof(glm::vec3)),
	debug_geometry_(debug_geometry)
{
	std::vector<glm::vec3> viewport_vertex_data = {
	{-1, -1, 0},
	{-1, 1, 0},
	{1, 1, 0},

	{1, 1, 0},
	{1, -1, 0},
	{-1, -1, 0}
	};

	viewport_vertex_buffer_.LoadData(viewport_vertex_data.data(), viewport_vertex_data.size() * sizeof(glm::vec3));



	model_scene_.UpdateData();
	ui_scene_.UpdateData();

	model_scene_.AttachDescriptorSets(descriptor_set_manager);
	ui_scene_.AttachDescriptorSets(descriptor_set_manager);

	handle_ = (void*)(1);
}

void render::FrameHandler::AddModel(const render::Mesh& model)
{
	model_scene_.AddModel(model);
}

bool render::FrameHandler::Draw(const Framebuffer& swapchain_framebuffer, const Image& swapchain_image, uint32_t image_index)
{
	//CommandBufferFiller command_filler(render_setup, framebuffer_collection);

	submit_info_.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	submit_info_.waitSemaphoreCount = 1;

	submit_info_.pWaitDstStageMask = &wait_stages_;

	submit_info_.commandBufferCount = 1;
	submit_info_.pCommandBuffers = &command_buffer_;

	submit_info_.signalSemaphoreCount = 1;

	submit_info_.pSignalSemaphores = &render_finished_semaphore_;


	present_info_.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	present_info_.waitSemaphoreCount = 1;
	present_info_.pWaitSemaphores = &render_finished_semaphore_;

	present_info_.swapchainCount = 1;
	present_info_.pSwapchains = &swapchain_;

	present_info_.pResults = nullptr; // Optional


	submit_info_.pWaitSemaphores = &image_available_semaphore_;

	vkWaitForFences(device_cfg_.logical_device, 1, &cmd_buffer_fence_, VK_TRUE, UINT64_MAX);
	vkResetFences(device_cfg_.logical_device, 1, &cmd_buffer_fence_);

	//model_scene_.UpdateCameraData(scene pos, look, 1.0f * swapchain_framebuffer.GetExtent().width / swapchain_framebuffer.GetExtent().height);

	model_scene_.UpdateData();
	ui_scene_.UpdateData();


//	std::vector<RenderPassInfo> render_info =
//	{
//		{
//			RenderPassId::kBuildDepthmap,
//			FramebufferId::kDepth,
//			{
//				{
//					PipelineId::kDepth,
//					RenderModelType::kStatic,
//					model_scene_.GetRenderNode()
//				},
//				{
//					PipelineId::kDepthSkinned,
//					RenderModelType::kSkinned,
//					model_scene_.GetRenderNode()
//				}
//			}
//		},
//
//		//{
//		//	RenderPassId::kSimpleRenderToScreen,
//		//	FramebufferId::kScreen,
//		//	{
//		//		{
//		//			PipelineId::kColor,
//		//			RenderModelType::kStatic,
//		//			model_scene_.GetRenderNode()
//		//		},
//		//		{
//		//			PipelineId::kColorSkinned,
//		//			RenderModelType::kSkinned,
//		//			model_scene_.GetRenderNode()
//		//		},
//		//		{
//		//			PipelineId::kUI,
//		//			RenderModelType::kStatic,
//		//			ui_scene_.GetRenderNode()
//		//		},
//		//	}
//		//},
//
//		{
//			RenderPassId::kBuildGBuffers,
//			FramebufferId::kGBuffers,
//			{
//				{
//					PipelineId::kBuildGBuffers,
//					RenderModelType::kStatic,
//					model_scene_.GetRenderNode()
//				}
//			}
//		},
//
//		{
//			RenderPassId::kCollectGBuffers,
//			FramebufferId::kScreen,
//			{
//				{
//					PipelineId::kCollectGBuffers,
//					RenderModelType::kStatic,
//					model_scene_.GetRenderNode()
//				}
//			}
//		},
////
////		{
////	RenderPassId::kSimpleRenderToScreen,
////	FramebufferId::kScreen,
////	{
////		{
////			PipelineId::kUI,
////			RenderModelType::kStatic,
////			ui_scene_.GetRenderNode()
////		}
////	}
////},
//	};

	std::vector<render::RenderModel> render_models;
	render_models.reserve(64);
	for (auto&& model : model_scene_.GetRenderNode().GetChildren())
	{
		for (auto&& primitive : model.GetPrimitives())
		{
			render_models.push_back(RenderModel
				{
					RenderModelCategory::kRenderModel,
					render_setup_.GetPipeline(PipelineId::kBuildGBuffers),
					u32(primitive.indices.count),
					model.GetDescriptorSets(),
				{},
				{},
				std::make_pair(primitive.indices.buffer->GetHandle(), primitive.indices.offset)
				});

			for (auto&& vertex_buffer : primitive.vertex_buffers)
			{
				render_models.back().vertex_buffers.push_back(vertex_buffer.buffer->GetHandle());
				render_models.back().vertex_buffers_offsets.push_back(vertex_buffer.offset);
			}
		}
	}

	for (auto&& model : ui_scene_.GetRenderNode().GetChildren())
	{
		for (auto&& primitive : model.GetPrimitives())
		{
			render_models.push_back(RenderModel
				{
					RenderModelCategory::kUIShape,
					render_setup_.GetPipeline(PipelineId::kUI),
					u32(primitive.indices.count),
					model.GetDescriptorSets(),
				{},
				{},
				std::make_pair(primitive.indices.buffer->GetHandle(), primitive.indices.offset),

				});

			for (auto&& vertex_buffer : primitive.vertex_buffers)
			{
				render_models.back().vertex_buffers.push_back(vertex_buffer.buffer->GetHandle());
				render_models.back().vertex_buffers_offsets.push_back(vertex_buffer.offset);
			}
		}
	}

	render_models.push_back(RenderModel
		{
			RenderModelCategory::kViewport,
			render_setup_.GetPipeline(PipelineId::kCollectGBuffers),
			6,
		{},
		{},
		{},
		{}
		}
	);

	render_models.back().vertex_buffers.push_back(viewport_vertex_buffer_.GetHandle());
	render_models.back().vertex_buffers_offsets.push_back(0);

	debug_geometry_.Update();

	render_models.push_back(RenderModel
		{
			RenderModelCategory::kViewport,
			render_setup_.GetPipeline(PipelineId::kDebugLines),
			debug_geometry_.coords_lines_vertex_cnt,
			{}
		}
	);

	render_models.back().vertex_buffers.push_back(debug_geometry_.coords_lines_position_buffer_.GetHandle());
	render_models.back().vertex_buffers.push_back(debug_geometry_.coords_lines_color_buffer_.GetHandle());
	render_models.back().vertex_buffers_offsets.push_back(0);
	render_models.back().vertex_buffers_offsets.push_back(0);


	render_models.push_back(RenderModel
		{
			RenderModelCategory::kViewport,
			render_setup_.GetPipeline(PipelineId::kDebugLines),
			debug_geometry_.debug_lines_vertex_cnt,
			{}
		}
	);

	render_models.back().vertex_buffers.push_back(debug_geometry_.debug_lines_position_buffer_.GetHandle());
	render_models.back().vertex_buffers.push_back(debug_geometry_.debug_lines_color_buffer_.GetHandle());
	render_models.back().vertex_buffers_offsets.push_back(0);
	render_models.back().vertex_buffers_offsets.push_back(0);


	auto scene_descriptor_sets = model_scene_.GetRenderNode().GetDescriptorSets();
	scene_descriptor_sets.insert(ui_scene_.GetDescriptorSets().begin(), ui_scene_.GetDescriptorSets().end());


	render_graph_handler_.FillCommandBuffer(command_buffer_, swapchain_framebuffer, swapchain_image, scene_descriptor_sets, render_models);

	present_info_.pImageIndices = &image_index;

	if (vkQueueSubmit(graphics_queue_, 1, &submit_info_, cmd_buffer_fence_) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	return vkQueuePresentKHR(graphics_queue_, &present_info_) == VK_SUCCESS;
}

VkSemaphore render::FrameHandler::GetImageAvailableSemaphore() const
{
	return image_available_semaphore_;
}


render::FrameHandler::~FrameHandler()
{
	if (handle_ != nullptr)
	{
		vkDestroyFence(device_cfg_.logical_device, cmd_buffer_fence_, nullptr);
		vkDestroySemaphore(device_cfg_.logical_device, render_finished_semaphore_, nullptr);
	}
}

std::pair<render::DebugGeometry::Point, render::DebugGeometry::Point> render::DebugGeometry::Point::operator>>(const Point& rhs)
{
	return { *this, rhs };
}

render::DebugGeometry::DebugGeometry(const DeviceConfiguration& device_cfg):
	coords_lines_position_buffer_(device_cfg, sizeof(glm::vec3) * 256),
	coords_lines_color_buffer_(device_cfg, sizeof(glm::vec3) * 256),
	coords_lines_vertex_cnt(0),
	debug_lines_position_buffer_(device_cfg, sizeof(glm::vec3) * 256),
	debug_lines_color_buffer_(device_cfg, sizeof(glm::vec3) * 256),
	debug_lines_vertex_cnt(0)
{
	ready_to_write.store(true);
	ready_to_read.store(false);
}

void render::DebugGeometry::Update()
{
	if (coords_lines_vertex_cnt == 0)
	{
		std::vector<glm::vec3> coords_lines_position_data = {
		{0, 0, 0},
		{1, 0, 0},

		{0, 0, 0},
		{0, 1, 0},

		{0, 0, 0},
		{0, 0, 1},
		};

		std::vector<glm::vec3> coords_lines_color_data = {
			{1, 0, 0},
			{1, 0, 0},

			{0, 1, 0},
			{0, 1, 0},

			{0, 0, 1},
			{0, 0, 1},
		};

		for (int i = -20; i < 21; i++)
		{
			coords_lines_position_data.push_back({ 0.5 * i, -10, -0.1 });
			coords_lines_position_data.push_back({ 0.5 * i, 10, -0.1 });

			coords_lines_color_data.push_back({ 0.2, 0.2, 0.2 });
			coords_lines_color_data.push_back({ 0.2, 0.2, 0.2 });
		}

		for (int i = -20; i < 21; i++)
		{
			coords_lines_position_data.push_back({ -10, 0.5 * i, -0.1 });
			coords_lines_position_data.push_back({ 10, 0.5 * i, -0.1 });

			coords_lines_color_data.push_back({ 0.2, 0.2, 0.2 });
			coords_lines_color_data.push_back({ 0.2, 0.2, 0.2 });
		}


		coords_lines_position_buffer_.LoadData(coords_lines_position_data.data(), coords_lines_position_data.size() * sizeof(glm::vec3));
		coords_lines_color_buffer_.LoadData(coords_lines_color_data.data(), coords_lines_position_data.size() * sizeof(glm::vec3));

		coords_lines_vertex_cnt = coords_lines_position_data.size();
	}

	if (ready_to_read.load(std::memory_order_acquire))
	{
		debug_lines_position_buffer_.LoadData(debug_lines_position_data_.data(), debug_lines_position_data_.size() * sizeof(glm::vec3));
		debug_lines_color_buffer_.LoadData(debug_lines_color_data_.data(), debug_lines_color_data_.size() * sizeof(glm::vec3));

		debug_lines_vertex_cnt = debug_lines_position_data_.size();

		ready_to_read.store(false, std::memory_order_relaxed);
		ready_to_write.store(true, std::memory_order_release);
	}
}

void render::DebugGeometry::SetDebugLines(const std::vector<Line>& lines)
{

	if (ready_to_write.load(std::memory_order_acquire))
	{

		debug_lines_position_data_.resize(0);
		debug_lines_color_data_.resize(0);

		for (auto&& line : lines)
		{
			debug_lines_position_data_.push_back(line.first.position);
			debug_lines_position_data_.push_back(line.second.position);

			debug_lines_color_data_.push_back(line.first.color);
			debug_lines_color_data_.push_back(line.second.color);
		}

		ready_to_write.store(false, std::memory_order_relaxed);
		ready_to_read.store(true, std::memory_order_release);
	}
}
