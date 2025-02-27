#ifndef RENDER_ENGINE_RENDER_FRAME_HANDLER_H_
#define RENDER_ENGINE_RENDER_FRAME_HANDLER_H_

#include "vulkan/vulkan.h"

#include "object_base.h"

#include "render/buffer.h"
#include "render/swapchain.h"
#include "render/render_setup.h"
#include "render/batches_manager.h"
#include "render/render_graph.h"
#include "render/sampler.h"
#include "render/scene.h"

namespace render
{
	class FrameHandler: public RenderObjBase<void*>
	{
	public:

		FrameHandler(const Global& global, const Swapchain& swapchain, const RenderSetup& render_setup,
			const Extents& extents, const Formats& formats, DescriptorSetsManager& descriptor_set_manager);
		
		FrameHandler(const FrameHandler&) = delete;
		FrameHandler(FrameHandler&&) = default;

		FrameHandler& operator=(const FrameHandler&) = delete;
		FrameHandler& operator=(FrameHandler&&) = default;
		
		//void AddModel(const render::Mesh& model);



		bool Draw(const FrameInfo& frame_info, const Scene& scene);

		VkSemaphore GetImageAvailableSemaphore() const;

		virtual ~FrameHandler() override;


	private:
		VkSwapchainKHR swapchain_;
		VkCommandBuffer command_buffer_;
		
		VkPipelineStageFlags wait_stages_;
		
		VkSemaphore render_finished_semaphore_;
		VkSemaphore image_available_semaphore_;

		VkFence cmd_buffer_fence_;

		VkSubmitInfo submit_info_;
		VkPresentInfoKHR present_info_;

		VkQueue graphics_queue_;

		DescriptorSetsManager& descriptor_set_manager_;

		const RenderSetup& render_setup_;

		//ModelSceneDescSetHolder model_scene_;
		RenderGraphHandler render_graph_handler_;
		//UIScene ui_scene_;
	};
}

#endif  // RENDER_ENGINE_RENDER_FRAME_HANDLER_H_