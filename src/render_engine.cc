#include "render/render_engine.h"

#include "platform.h"
#include "stl_util.h"

#include <tchar.h>


#include <vector>
#include <map>
#include <tuple>
#include <chrono>

#include "vulkan/vulkan.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>

#include "common.h"
#include "surface.h"
#include "render/vk_util.h"
#include "render/data_types.h"

#include "render/batch.h"
#include "render/batches_manager.h"
#include "render/buffer.h"
#include "render/command_pool.h"
#include "render/descriptor_pool.h"
#include "render/framebuffer.h"
#include "render/frame_handler.h"
#include "render/graphics_pipeline.h"
#include "render/image.h"
#include "render/image_view.h"
#include "render/sampler.h"
#include "render/swapchain.h"

namespace render
{

	class VkDeviceWrapper
	{
	public:
		
		operator const VkDevice&() const { return device_; }

		VkDevice* operator&() { return &device_; }
		
		~VkDeviceWrapper()
		{
			vkDeviceWaitIdle(device_);
			vkDestroyDevice(device_, nullptr);
		}
	private:
		VkDevice device_;
	};

	class VkInstanceWrapper
	{
	public:
		operator const VkInstance& () const { return instance_; }

		VkInstance* operator&() { return &instance_; }

		~VkInstanceWrapper()
		{
			vkDestroyInstance(instance_, nullptr);
		}
	private:
		VkInstance instance_;
	};

	class RenderEngine::RenderEngineImpl
	{
	public:

		RenderEngineImpl(const RenderEngineImpl&) = delete;
		RenderEngineImpl& operator=(const RenderEngineImpl&) = delete;

		RenderEngineImpl(InitParam param)
		{
			vk_init_success_ = false;

			if (!InitInstanceExtensions())
			{
				LOG(err, "Could not init InstanceExtensions");
				return;
			}

			if (!stl_util::All(platform::GetRequiredExtensions(), vk_instance_extensions_, [](auto&& ext_name, auto&& ext) { return std::strcmp(ext_name, ext.extensionName) == 0; }))
			{
				LOG(err, "Required platform Extension missing");
				return;
			}

			if (!InitInstanceLayers())
			{
				LOG(err, "InitInstanceLayers error");
				return;
			}

			if (!stl_util::All(GetValidationLayers(), vk_instance_layers_, [](auto&& ext_name, auto&& ext) { return std::strcmp(ext_name, ext.layerName) == 0; }))
			{
				LOG(err, "Instance Layers missing");
				return;
			}

			application_info_.apiVersion = VK_API_VERSION_1_0; // CHECK WHAT DOES THIS MEAN!
			application_info_.applicationVersion = 1;
			application_info_.engineVersion = 1;
			application_info_.pApplicationName = "vulkan_concepts";
			application_info_.pEngineName = "vulkan_concepts_engine";
			application_info_.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			application_info_.pNext = nullptr;

			VkInstanceCreateInfo instance_create_info{};
			instance_create_info.enabledExtensionCount = static_cast<uint32_t>(platform::GetRequiredExtensions().size());
			instance_create_info.ppEnabledExtensionNames = platform::GetRequiredExtensions().data();
			instance_create_info.enabledLayerCount = static_cast<uint32_t>(GetValidationLayers().size());
			instance_create_info.ppEnabledLayerNames = GetValidationLayers().data();
			instance_create_info.pApplicationInfo = &application_info_;
			instance_create_info.pNext = nullptr;
			instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;


			if (vkCreateInstance(&instance_create_info, nullptr, &vk_instance_) != VK_SUCCESS)
			{
				LOG(err, "vkCreateInstance error");
				return;
			}


			if (!InitPhysicalDevices())
			{
				LOG(err, "InitPhysicalDevices error");
				return;
			}


			uint32_t selected_device_index = DetermineDeviceForUse();

			if (selected_device_index == -1)
			{
				LOG(err, "Could not determine valid device");
				return;
			}

			device_cfg_.physical_device = vk_physical_devices_[selected_device_index];
			vkGetPhysicalDeviceProperties(device_cfg_.physical_device, &device_cfg_.physical_device_properties);


			device_cfg_.graphics_queue_index = FindDeviceQueueFamalyWithFlag(device_cfg_.physical_device, VK_QUEUE_GRAPHICS_BIT);
			device_cfg_.transfer_queue_index = FindDeviceQueueFamalyWithFlag(device_cfg_.physical_device, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT);

			if (!InitLogicalDevice(device_cfg_.physical_device, { device_cfg_.graphics_queue_index , device_cfg_.transfer_queue_index }))
			{
				LOG(err, "InitLogicalDevices error");
				return;
			}

			device_cfg_.logical_device = vk_logical_devices_[0];

			vkGetDeviceQueue(device_cfg_.logical_device, device_cfg_.graphics_queue_index, 0, &device_cfg_.graphics_queue);
			vkGetDeviceQueue(device_cfg_.logical_device, device_cfg_.transfer_queue_index, 0, &device_cfg_.transfer_queue);

			platform::Window window = platform::CreatePlatformWindow(param);

			surface_ptr_ = std::make_unique<Surface>(window, vk_instance_, device_cfg_);



			graphics_command_pool_ptr_ = std::make_unique<CommandPool>(device_cfg_, CommandPool::PoolType::kGraphics);
			transfer_command_pool_ptr_ = std::make_unique<CommandPool>(device_cfg_, CommandPool::PoolType::kTransfer);

			device_cfg_.graphics_cmd_pool = graphics_command_pool_ptr_.get();
			device_cfg_.transfer_cmd_pool = transfer_command_pool_ptr_.get();

			vk_init_success_ = true;
		}


		bool VKInitSuccess()
		{
			return vk_init_success_;
		}

		void cleanup() {
			//vkDestroyPipelineLayout(vk_logical_devices_[selected_device_index], pipelineLayout, nullptr);
		}

		void ShowWindow()
		{

			DescriptorPool descriptor_pool(device_cfg_, 100, 100);

			device_cfg_.descriptor_pool = &descriptor_pool;

			static auto start_time = std::chrono::high_resolution_clock::now();

			platform::ShowWindow(surface_ptr_->GetWindow());

			bool should_refresh_swapchain = true;

			std::vector<FrameHandler> frames;

			float yaw = glm::pi<float>() * 5 / 4;
			float pitch = -glm::pi<float>() /4;

			glm::vec3 position(2, 2, 2);

			uint32_t current_frame_index = -1;

			while (!platform::IsWindowClosed(surface_ptr_->GetWindow()) && should_refresh_swapchain)
			{
				graphics_command_pool_ptr_->ClearCommandBuffers();
				graphics_command_pool_ptr_->CreateCommandBuffers(kFramesCount);

				should_refresh_swapchain = false;

				Swapchain swapchain(device_cfg_, *surface_ptr_);

				PipelineCollection pipeline_collection(device_cfg_, swapchain.GetExtent(), swapchain.GetFormat());

				std::vector<Framebuffer> swapchain_framebuffers;

				VkFormat depth_format = VK_FORMAT_D32_SFLOAT;
				Image depth_image(device_cfg_, depth_format, swapchain.GetExtent().width, swapchain.GetExtent().height, Image::ImageType::kDepthImage);
				ImageView depth_image_view(device_cfg_, depth_image);

				for (int i = 0; i < swapchain.GetImagesCount(); i++)
				{
					std::vector<std::reference_wrapper<const ImageView>> attachments;
					
					attachments.push_back(swapchain.GetImageView(i));
					attachments.push_back(depth_image_view);

					swapchain_framebuffers.push_back(Framebuffer(device_cfg_, swapchain.GetExtent(), attachments, pipeline_collection.GetRenderPass(PipelineCollection::RenderPassId::kScreen)));
				}

				BatchesManager batches_manager(device_cfg_, kFramesCount, swapchain, descriptor_pool);
				
				frames.clear();
				frames.reserve(kFramesCount);

				for (size_t frame_ind = 0; frame_ind < kFramesCount; frame_ind++)
				{
					frames.push_back(FrameHandler(device_cfg_, swapchain));
				}

				while (!platform::IsWindowClosed(surface_ptr_->GetWindow()) && !should_refresh_swapchain)
				{
					current_frame_index = (current_frame_index + 1) % kFramesCount;

					uint32_t image_index;


					VkResult result = vkAcquireNextImageKHR(device_cfg_.logical_device, swapchain.GetHandle(), UINT64_MAX,
						frames[current_frame_index].GetImageAvailableSemaphore(), VK_NULL_HANDLE, &image_index);


					if (result != VK_SUCCESS)
					{
						if (result == VK_ERROR_OUT_OF_DATE_KHR)
						{
							should_refresh_swapchain = true;
						}
						continue;
					}


					auto&& render_batches = batches_manager.GetBatches();

					int mouse_x_delta;
					int mouse_y_delta;

					platform::GetMouseDelta(mouse_x_delta, mouse_y_delta);
					auto&& buttons = platform::GetButtonState();

					yaw += (1.0f * mouse_x_delta / 500);
					pitch += (-1.0f * mouse_y_delta / 500);
					if (pitch > 1.505f)
						pitch = 1.505f;
					if (pitch < -1.505f)
						pitch = -1.505f;

					glm::vec3 look = glm::normalize(glm::vec3(glm::sin(yaw) * glm::cos(pitch), glm::cos(yaw) * glm::cos(pitch), glm::sin(pitch)));

					if (buttons['w' - 'a'])
					{
						position += (0.001f * glm::normalize(look));
					}

					if (buttons['s' - 'a'])
					{
						position -= (0.001f * glm::normalize(look));
					}

					if (buttons['d' - 'a'])
					{
						position += (0.001f * glm::normalize(glm::vec3(glm::sin(yaw + glm::pi<float>()/2), glm::cos(yaw + glm::pi<float>() / 2), 0)));
					}

					if (buttons['a' - 'a'])
					{
						position -= (0.001f * glm::normalize(glm::vec3(glm::sin(yaw + glm::pi<float>() / 2), glm::cos(yaw + glm::pi<float>() / 2), 0)));
					}

					should_refresh_swapchain = !frames[current_frame_index].Draw(swapchain_framebuffers[image_index], image_index, pipeline_collection, batches_manager, position, look);

					auto current_time = std::chrono::high_resolution_clock::now();

				}

				vkDeviceWaitIdle(device_cfg_.logical_device);
			}

			platform::JoinWindowThread(surface_ptr_->GetWindow());
		}

		~RenderEngineImpl()
		{

		}

	private:

		const std::vector<const char*>& GetRequiredExtensions()
		{
			static const std::vector<const char*> extensions{ "VK_KHR_swapchain" };
			return extensions;
		}

		const std::vector<const char*>& GetValidationLayers()
		{
#ifndef NDEBUG
			static const std::vector<const char*> layers{ "VK_LAYER_KHRONOS_validation" };
			return layers;
#else
			static const std::vector<const char*> layers{};
			return layers;
#endif // !NDEBUG
		}

		bool InitPhysicalDevices()
		{
			uint32_t physical_devices_count;

			if (vkEnumeratePhysicalDevices(vk_instance_, &physical_devices_count, nullptr) != VK_SUCCESS)
				return false;

			vk_physical_devices_.resize(physical_devices_count);

			if (vkEnumeratePhysicalDevices(vk_instance_, &physical_devices_count, reinterpret_cast<VkPhysicalDevice*>(vk_physical_devices_.data())) != VK_SUCCESS)
				return false;
			//TODO: add device selection
			for (size_t i = 0; i < physical_devices_count; i++)
			{
				InitPhysicalDeviceProperties(vk_physical_devices_[i]);
				InitPhysicalDeviceQueueFamaliesProperties(vk_physical_devices_[i]);
				InitPhysicalDeviceFeatures(vk_physical_devices_[i]);
				if (!(InitPhysicalDeviceLayers(vk_physical_devices_[i]) &&
					InitPhysicalDeviceExtensions(vk_physical_devices_[i])))
					return false;
			}
			return true;
		}

		bool InitLogicalDevices()
		{
			//TODO: add device selection
			//for (VkPhysicalDevice& physical_device : vk_physical_devices_)
			//{
			//	if (!InitLogicalDevice(physical_device))
			//		return false;
			//}
			//return true;
			return false;
	}

		uint32_t FindDeviceQueueFamalyWithFlag(const VkPhysicalDevice& physical_deivce, VkQueueFlags flags, VkQueueFlags flags_to_check_mask = 0)
		{
			auto&& queue_famalies_properties = vk_physical_devices_to_queues_[physical_deivce];

			if (flags_to_check_mask == 0)
			{
				flags_to_check_mask = flags;
			}

			for (uint32_t i = 0; i < queue_famalies_properties.size(); i++)
			{
				if ((queue_famalies_properties[i].queueFlags & flags_to_check_mask) == flags)
				{
					return i;
				}
			}

			return -1;
		}

		uint32_t DetermineDeviceForUse()
		{
			VkPhysicalDevice physical_device_out = nullptr;


			for (auto&& [physical_device, queues_properties] : vk_physical_devices_to_queues_)
			{
				for (uint32_t i = 0; i < queues_properties.size(); i++)
				{
					if (platform::GetPhysicalDevicePresentationSupport(physical_device, i) && (queues_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
					{
						physical_device_out = physical_device;
						break;
					}
				}

				if (physical_device_out != nullptr)
					break;
			}

			auto physical_device_out_it = std::find(vk_physical_devices_.begin(), vk_physical_devices_.end(), physical_device_out);

			if (physical_device_out_it != vk_physical_devices_.end())
			{
				return static_cast<uint32_t>(physical_device_out_it - vk_physical_devices_.begin());
			}

			return -1;
		}


		void InitPhysicalDeviceProperties(VkPhysicalDevice physical_device)
		{
			using PhDevPropMapType = std::map<VkPhysicalDevice, VkPhysicalDeviceMemoryProperties>;

			auto it = vk_physical_devices_propeties_.lower_bound(physical_device);

			if (it == vk_physical_devices_propeties_.end() || it->first != physical_device)
			{
				it = vk_physical_devices_propeties_.emplace_hint(it,
					std::piecewise_construct, std::forward_as_tuple(physical_device), std::forward_as_tuple());
				vkGetPhysicalDeviceMemoryProperties(physical_device, &it->second);

			}
		}

		void InitPhysicalDeviceQueueFamaliesProperties(VkPhysicalDevice physical_device)
		{
			using PhDevFamQMapType = std::map<VkPhysicalDevice, std::vector<VkQueueFamilyProperties>>;

			auto it = vk_physical_devices_to_queues_.lower_bound(physical_device);

			if (it == vk_physical_devices_to_queues_.end() || it->first != physical_device)
			{
				uint32_t device_queue_famaly_prop_cnt;
				vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &device_queue_famaly_prop_cnt, nullptr);

				it = vk_physical_devices_to_queues_.emplace_hint(it,
					std::piecewise_construct, std::forward_as_tuple(physical_device), std::forward_as_tuple());

				it->second.resize(device_queue_famaly_prop_cnt);

				vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &device_queue_famaly_prop_cnt, it->second.data());
			}
		}


		void InitPhysicalDeviceFeatures(const VkPhysicalDevice& physical_device)
		{

			using PhDevPropMapType = std::map<VkPhysicalDevice, VkPhysicalDeviceFeatures>;

			auto it = vk_physical_devices_features_.lower_bound(physical_device);

			if (it == vk_physical_devices_features_.end() || it->first != physical_device)
			{
				it = vk_physical_devices_features_.emplace_hint(it,
					std::piecewise_construct, std::forward_as_tuple(physical_device), std::forward_as_tuple());
				vkGetPhysicalDeviceFeatures(physical_device, &it->second);

			}
		}

		bool InitInstanceLayers()
		{
			uint32_t count;
			if (vkEnumerateInstanceLayerProperties(&count, nullptr) == VK_SUCCESS)
			{
				vk_instance_layers_.resize(count);
				if (vkEnumerateInstanceLayerProperties(&count, vk_instance_layers_.data()) == VK_SUCCESS)
					return true;
			}
			return false;
		}

		bool InitPhysicalDeviceLayers(const VkPhysicalDevice& physical_device)
		{
			using PhDevFamQMapType = std::map<VkPhysicalDevice, std::vector<VkLayerProperties>>;

			auto it = vk_physical_devices_layers_.lower_bound(physical_device);

			if (it == vk_physical_devices_layers_.end() || it->first != physical_device)
			{
				uint32_t device_layers_cnt;
				if (vkEnumerateDeviceLayerProperties(physical_device, &device_layers_cnt, nullptr) != VK_SUCCESS)
					return false;

				it = vk_physical_devices_layers_.emplace_hint(it,
					std::piecewise_construct, std::forward_as_tuple(physical_device), std::forward_as_tuple());

				it->second.resize(device_layers_cnt);

				if (vkEnumerateDeviceLayerProperties(physical_device, &device_layers_cnt, it->second.data()) != VK_SUCCESS)
					return false;
			}
			return true;
		}

		bool InitInstanceExtensions()
		{
			uint32_t count;
			if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) == VK_SUCCESS)
			{
				vk_instance_extensions_.resize(count);
				if (vkEnumerateInstanceExtensionProperties(nullptr, &count, vk_instance_extensions_.data()) == VK_SUCCESS)
					return true;
			}
			return false;
		}

		bool InitPhysicalDeviceExtensions(const VkPhysicalDevice& physical_device)
		{
			using PhDevFamQMapType = std::map<VkPhysicalDevice, std::vector<VkExtensionProperties>>;

			auto it = vk_physical_devices_extensions_.lower_bound(physical_device);

			if (it == vk_physical_devices_extensions_.end() || it->first != physical_device)
			{
				uint32_t device_extensions_cnt;
				if (vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &device_extensions_cnt, nullptr) != VK_SUCCESS)
					return false;

				it = vk_physical_devices_extensions_.emplace_hint(it,
					std::piecewise_construct, std::forward_as_tuple(physical_device), std::forward_as_tuple(device_extensions_cnt));

				if (vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &device_extensions_cnt, it->second.data()) != VK_SUCCESS)
					return false;
			}
			return true;
		}

		bool InitLogicalDevice(const VkPhysicalDevice& physical_device, const std::vector<uint32_t> queue_famaly_indices)
		{
			VkDeviceCreateInfo logical_device_create_info{};

			if (stl_util::All(GetRequiredExtensions(), vk_physical_devices_extensions_[physical_device], [](auto&& ext_name, auto&& ext) { return std::strcmp(ext_name, ext.extensionName) == 0; }))
			{
				logical_device_create_info.enabledExtensionCount = static_cast<uint32_t>(GetRequiredExtensions().size());
				logical_device_create_info.ppEnabledExtensionNames = GetRequiredExtensions().data();
			}
			else return false;



			logical_device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			logical_device_create_info.pNext = nullptr;
			logical_device_create_info.flags = 0;
			logical_device_create_info.enabledLayerCount = 0;
			logical_device_create_info.ppEnabledLayerNames = nullptr;
			logical_device_create_info.pEnabledFeatures = &vk_physical_devices_features_[physical_device];


			std::vector<VkDeviceQueueCreateInfo> queue_create_infos(queue_famaly_indices.size());
			std::vector<std::vector<float>> queue_priorities(queue_famaly_indices.size());
			for (auto&& queue_famaly_index : queue_famaly_indices)
			{
				queue_create_infos[queue_famaly_index].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queue_create_infos[queue_famaly_index].flags = 0;
				queue_create_infos[queue_famaly_index].queueFamilyIndex = queue_famaly_index;

				uint32_t device_queue_count = vk_physical_devices_to_queues_[physical_device][queue_famaly_index].queueCount;

				queue_create_infos[queue_famaly_index].queueCount = device_queue_count;

				queue_priorities[queue_famaly_index].resize(device_queue_count);
				
				for (auto&& priority : queue_priorities[queue_famaly_index]) 
					priority = 1.0f;

				queue_create_infos[queue_famaly_index].pQueuePriorities = queue_priorities[queue_famaly_index].data();
				queue_create_infos[queue_famaly_index].pNext = nullptr;
			}



			logical_device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
			logical_device_create_info.pQueueCreateInfos = queue_create_infos.data();

			vk_logical_devices_.emplace_back();

			if (vkCreateDevice(physical_device, &logical_device_create_info, nullptr, &vk_logical_devices_[vk_logical_devices_.size() - 1]) != VK_SUCCESS)
				return false;
			return true;
		}




		bool vk_init_success_;
		VkApplicationInfo application_info_;
		VkInstanceWrapper vk_instance_;



		std::vector<VkPhysicalDevice> vk_physical_devices_;
		std::map<VkPhysicalDevice, VkPhysicalDeviceMemoryProperties> vk_physical_devices_propeties_;
		std::map<VkPhysicalDevice, std::vector<VkQueueFamilyProperties>> vk_physical_devices_to_queues_;
		std::map<VkPhysicalDevice, VkPhysicalDeviceFeatures> vk_physical_devices_features_;
		std::vector<VkLayerProperties > vk_instance_layers_;
		std::map<VkPhysicalDevice, std::vector<VkLayerProperties>> vk_physical_devices_layers_;
		std::vector<VkExtensionProperties> vk_instance_extensions_;
		std::map<VkPhysicalDevice, std::vector<VkExtensionProperties>> vk_physical_devices_extensions_;
		
		std::vector<VkDeviceWrapper> vk_logical_devices_;

		DeviceConfiguration device_cfg_;

		std::unique_ptr<Surface> surface_ptr_;

		std::unique_ptr<DescriptorPool> descriptor_pool_ptr_;
		std::unique_ptr<CommandPool> graphics_command_pool_ptr_;
		std::unique_ptr<CommandPool> transfer_command_pool_ptr_;

		const uint32_t kFramesCount = 4;
		//VkSemaphore image_available_semaphore_;
		//VkSemaphore render_finished_semaphore_;
};

	RenderEngine::RenderEngine(InitParam param)
	{
		impl_ = std::make_unique<RenderEngineImpl>(param);
	}

	RenderEngine::~RenderEngine() = default;

	bool RenderEngine::VKInitSuccess()
	{
		return impl_->VKInitSuccess();
	}

	void RenderEngine::ShowWindow()
	{
		impl_->ShowWindow();
	}
}