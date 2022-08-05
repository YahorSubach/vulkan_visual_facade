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

render::FrameHandler::FrameHandler(const DeviceConfiguration& device_cfg, const Swapchain& swapchain, const RenderSetup& render_setup, const BatchesManager& batches_manager, const ui::UI& ui) :
	RenderObjBase(device_cfg), swapchain_(swapchain.GetHandle()), graphics_queue_(device_cfg.graphics_queue),
	command_buffer_(device_cfg.graphics_cmd_pool->GetCommandBuffer()),
	image_available_semaphore_(vk_util::CreateSemaphore(device_cfg.logical_device)),
	render_finished_semaphore_(vk_util::CreateSemaphore(device_cfg.logical_device)),
	cmd_buffer_fence_(vk_util::CreateFence(device_cfg.logical_device)), present_info_{}, submit_info_{}, wait_stages_(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT),
	framebuffer_collection(device_cfg_, render_setup),
	model_scene_(device_cfg, batches_manager, framebuffer_collection),
	ui_scene_(device_cfg, ui),
	descriptor_sets_manager_(device_cfg, render_setup),
	ui_(ui)
{

	model_scene_.UpdateData();
	ui_scene_.UpdateData();

	model_scene_.AttachDescriptorSets(descriptor_sets_manager_);
	ui_scene_.AttachDescriptorSets(descriptor_sets_manager_);

	handle_ = (void*)(1);
}


bool render::FrameHandler::Draw(const Framebuffer& swapchain_framebuffer, uint32_t image_index, const RenderSetup& render_setup, glm::vec3 pos, glm::vec3 look)
{
	CommandBufferFiller command_filler(render_setup, framebuffer_collection);

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

	model_scene_.UpdateCameraData(pos, look, 1.0f * swapchain_framebuffer.GetExtent().width / swapchain_framebuffer.GetExtent().height);

	model_scene_.UpdateData();
	ui_scene_.UpdateData();


	std::vector<RenderPassInfo> render_info =
	{
		{
			RenderPassId::kBuildDepthmap,
			FramebufferId::kDepth,
			{
				{
					PipelineId::kDepth,
					RenderModelType::kStatic,
					model_scene_.GetRenderNode()
				},
				{
					PipelineId::kDepthSkinned,
					RenderModelType::kSkinned,
					model_scene_.GetRenderNode()
				}
			}
		},

		{
			RenderPassId::kSimpleRenderToScreen,
			FramebufferId::kScreen,
			{
				{
					PipelineId::kColor,
					RenderModelType::kStatic,
					model_scene_.GetRenderNode()
				},
				{
					PipelineId::kColorSkinned,
					RenderModelType::kSkinned,
					model_scene_.GetRenderNode()
				},
				{
					PipelineId::kUI,
					RenderModelType::kStatic,
					ui_scene_.GetRenderNode()
				},
			}
		},

		{
			RenderPassId::kBuildGBuffers,
			FramebufferId::kGBuffers,
			{
				{
					PipelineId::kBuildGBuffers,
					RenderModelType::kStatic,
					model_scene_.GetRenderNode()
				}
			}
		},

		{
			RenderPassId::kCollectGBuffers,
			FramebufferId::kScreen,
			{
				{
					PipelineId::kCollectGBuffers,
					RenderModelType::kStatic,
					model_scene_.GetRenderNode()
				}
			}
		},

//		{
//	RenderPassId::kSimpleRenderToScreen,
//	FramebufferId::kScreen,
//	{
//		{
//			PipelineId::kUI,
//			RenderModelType::kStatic,
//			ui_scene_.GetRenderNode()
//		}
//	}
//},
	};

	
	command_filler.Fill(command_buffer_, render_info, swapchain_framebuffer);

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