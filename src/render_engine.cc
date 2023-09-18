#include "render/render_engine.h"

#include "platform.h"
#include "stl_util.h"

#include <tchar.h>


#include <vector>
#include <stack>
#include <map>
#include <tuple>
#include <chrono>
#include <sstream>

#include "vulkan/vulkan.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/matrix_transform.hpp>

#include "common.h"

#include "render/global.h"

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
#include "render/render_setup.h"
#include "render/sampler.h"
#include "render/scene.h"
#include "render/swapchain.h"

#include "render/ui/panel.h"

#include "sync_util.h"

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

		RenderEngineImpl(InitParam param) : external_command_queue_(32), last_object_id_(0)
		{
			vk_init_success_ = false;

			if (!InitInstanceExtensions())
			{
				LOG(err, "Could not init InstanceExtensions");
				return;
			}

			if (!util::All(platform::GetRequiredInstanceExtensions(), vk_instance_extensions_, [](auto&& ext_name, auto&& ext) { return std::strcmp(ext_name, ext.extensionName) == 0; }))
			{
				LOG(err, "Required platform Extension missing");
				return;
			}

			if (!InitInstanceLayers())
			{
				LOG(err, "InitInstanceLayers error");
				return;
			}

			if (!util::All(GetValidationLayers(), vk_instance_layers_, [](auto&& ext_name, auto&& ext) { return std::strcmp(ext_name, ext.layerName) == 0; }))
			{
				LOG(err, "Instance Layers missing");
				return;
			}

			application_info_.apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0); // CHECK WHAT DOES THIS MEAN!
			application_info_.applicationVersion = 1;
			application_info_.engineVersion = 1;
			application_info_.pApplicationName = "vulkan_concepts";
			application_info_.pEngineName = "vulkan_concepts_engine";
			application_info_.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			application_info_.pNext = nullptr;

			VkInstanceCreateInfo instance_create_info{};
			instance_create_info.enabledExtensionCount = static_cast<uint32_t>(platform::GetRequiredInstanceExtensions().size());
			instance_create_info.ppEnabledExtensionNames = platform::GetRequiredInstanceExtensions().data();
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

			global_.physical_device = vk_physical_devices_[selected_device_index];
			vkGetPhysicalDeviceProperties(global_.physical_device, &global_.physical_device_properties);
			auto images_properties = GetImageFormatsProperties();



			global_.graphics_queue_index = FindDeviceQueueFamalyWithFlag(global_.physical_device, VK_QUEUE_GRAPHICS_BIT);
			global_.transfer_queue_index = FindDeviceQueueFamalyWithFlag(global_.physical_device, VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT);
			if (global_.transfer_queue_index == -1)
			{
				global_.transfer_queue_index = FindDeviceQueueFamalyWithFlag(global_.physical_device, VK_QUEUE_TRANSFER_BIT);
			}

			std::vector<uint32_t> queue_indices;
			queue_indices.push_back(global_.graphics_queue_index);
			if (global_.graphics_queue_index != global_.transfer_queue_index)
			{
				queue_indices.push_back(global_.transfer_queue_index);
			}

			if (!InitLogicalDevice(global_.physical_device, queue_indices))
			{
				LOG(err, "InitLogicalDevices error");
				return;
			}

			global_.logical_device = vk_logical_devices_[0];

			vkGetDeviceQueue(global_.logical_device, global_.graphics_queue_index, 0, &global_.graphics_queue);
			if (global_.graphics_queue_index != global_.transfer_queue_index)
			{
				vkGetDeviceQueue(global_.logical_device, global_.transfer_queue_index, 0, &global_.transfer_queue);
			}
			else
			{
				global_.transfer_queue = global_.graphics_queue;
			}
			
			platform::Window window = platform::CreatePlatformWindow(param);

			surface_ptr_ = std::make_unique<Surface>(window, vk_instance_, global_);



			graphics_command_pool_ptr_ = std::make_unique<CommandPool>(global_, CommandPool::PoolType::kGraphics);
			transfer_command_pool_ptr_ = std::make_unique<CommandPool>(global_, CommandPool::PoolType::kTransfer);

			global_.graphics_cmd_pool = graphics_command_pool_ptr_.get();
			global_.transfer_cmd_pool = transfer_command_pool_ptr_.get();

			vk_init_success_ = true;
		}


		bool VKInitSuccess()
		{
			return vk_init_success_;
		}

		void cleanup() {
			//vkDestroyPipelineLayout(vk_logical_devices_[selected_device_index], pipelineLayout, nullptr);
		}

		void StartRenderThread()
		{
			ready = false;
			render_thread_ = std::thread(&RenderEngine::RenderEngineImpl::RenderLoop, this);
			while (!ready)
			{
				std::this_thread::yield();
			}
		}

		void RenderLoop()
		{

			std::vector<ModelPack> model_packs;
			std::unordered_map<std::string, uint32_t> model_packs_name_to_index;

			static auto start_time = std::chrono::high_resolution_clock::now();
			static auto start_time_fps = std::chrono::high_resolution_clock::now();


			platform::ShowWindow(surface_ptr_->GetWindow());

			bool should_refresh_swapchain = true;

			std::vector<FrameHandler> frames;

			glm::vec3 position(2, 2, 1.7);

			uint32_t current_frame_index = -1;

			Image error_image(global_, Image::BuiltinImageType::kError);
			global_.error_image = error_image;

			Image default_normal(global_, Image::BuiltinImageType::kNormal);
			global_.default_normal = default_normal;

			global_.presentation_format = surface_ptr_->GetSurfaceFormat(global_.physical_device).format;

			for (int i = 0; i < 16; i++)
			{
				global_.mipmap_cnt_to_global_samplers.push_back(Sampler(global_, i));
			}

			global_.nearest_sampler.emplace(Sampler(global_, 0, Sampler::AddressMode::kClampToBorder, true));

			DescriptorSetsManager descriptor_set_manager(global_);
			RenderSetup render_setup(global_);
			
			model_packs.push_back(ModelPack(global_, descriptor_set_manager));

			DebugGeometry dg(global_, descriptor_set_manager);
			
			scenes_.emplace_back(global_, descriptor_set_manager, dg); // TODO fix scene impl move

			ready = true;
			uint32_t frame_cnt = 0;

			ui::UI ui(global_);
			std::shared_ptr<ui::Panel> screen_panel;
			std::shared_ptr<ui::TextBlock> block;
			while (!platform::IsWindowClosed(surface_ptr_->GetWindow()) && should_refresh_swapchain)
			{
				//descriptor_set_manager.FreeAll();

				graphics_command_pool_ptr_->ClearCommandBuffers();
				graphics_command_pool_ptr_->CreateCommandBuffers(kFramesCount);

				should_refresh_swapchain = false;

				Swapchain swapchain(global_, *surface_ptr_);

				auto swapchain_extent = swapchain.GetExtent();

				std::array<Extent, kExtentTypeCnt> extents
				{
					swapchain_extent,
					swapchain_extent,
					{512,512}
				};

				scenes_[0].aspect = 1.0f * swapchain_extent.width / swapchain_extent.height;

				render_setup.InitPipelines(descriptor_set_manager, extents);



				std::vector<Framebuffer> swapchain_framebuffers;

				for (int i = 0; i < swapchain.GetImagesCount(); i++)
				{
					Framebuffer::ConstructParams params
					{
						render_setup.GetSwapchainRenderPass(),
						swapchain.GetExtent()
					};

					params.attachments.push_back(swapchain.GetImageView(i));

					swapchain_framebuffers.push_back(Framebuffer(global_, params));
				}
				
				frames.clear();
				frames.reserve(kFramesCount);

				if (screen_panel)
				{
					screen_panel->SetWidth((float)swapchain_extent.width);
					screen_panel->SetHeight((float)swapchain_extent.height);
				}

				for (size_t frame_ind = 0; frame_ind < kFramesCount; frame_ind++)
				{
					frames.push_back(FrameHandler(global_, swapchain, render_setup, extents, descriptor_set_manager, ui, scenes_[0]));
				}



				while (!platform::IsWindowClosed(surface_ptr_->GetWindow()) && !should_refresh_swapchain)
				{

					frame_cnt++;

					current_frame_index = (current_frame_index + 1) % kFramesCount;
					global_.frame_ind = current_frame_index;

					uint32_t image_index;

					VkResult result = vkAcquireNextImageKHR(global_.logical_device, swapchain.GetHandle(), UINT64_MAX,
						frames[current_frame_index].GetImageAvailableSemaphore(), VK_NULL_HANDLE, &image_index);


					if (result != VK_SUCCESS)
					{
						if (result == VK_ERROR_OUT_OF_DATE_KHR)
						{
							should_refresh_swapchain = true;
						}
						continue;
					}

					FrameInfo frame_info
					{
						swapchain_framebuffers[image_index],
						swapchain.GetImage(image_index),
						image_index,
						current_frame_index
					};

					should_refresh_swapchain = !frames[current_frame_index].Draw(frame_info, scenes_[0]);

					int command_count_to_execute = external_command_queue_.Size();



					if (block)
					{
						std::chrono::duration<float> dur = std::chrono::high_resolution_clock::now() - start_time_fps;

						std::string s = std::to_string(1.0f * frame_cnt / dur.count());
						std::basic_string<char32_t> us(s.data(), s.data() + s.length());

						if (dur.count() > 3)
						{
							start_time_fps = std::chrono::high_resolution_clock::now();
							frame_cnt = 0;
						}

						block->SetText(us, 30);
					}

					while (command_count_to_execute-- > 0)
					{


						auto&& command = external_command_queue_.Pop();

						if (std::holds_alternative<command::Load>(command))
						{
							model_packs.push_back(ModelPack(global_, descriptor_set_manager));

							auto&& specified_command = std::get<command::Load>(command);
							model_packs.back().AddGLTF(*specified_command.model);
							model_packs_name_to_index.emplace(specified_command.pack_name, (uint32_t)(model_packs.size() - 1));
						}

						if (std::holds_alternative<command::Geometry>(command))
						{
							auto&& specified_command = std::get<command::Geometry>(command);

							model_packs[0].AddSimpleMesh(specified_command.faces, PrimitiveProps::kDebugPos);

							auto node_id = scenes_[0].AddNode();
							auto&& scene_node = scenes_[0].GetNode(node_id);
							scenes_[0].AddModel(scene_node, model_packs[0].meshes[0]);
						}

						if (std::holds_alternative<command::AddObject<ObjectType::StaticModel>>(command))
						{
							auto&& specified_command = std::get<command::AddObject<ObjectType::StaticModel>>(command);

							auto&& pack = model_packs[model_packs_name_to_index.at(specified_command.desc.pack_name)];
							auto&& pack_model = pack.models[specified_command.desc.model_name];

							auto node_id = scenes_[0].AddNode();
							auto&& node = scenes_[0].GetNode(node_id);

							while (object_id_to_scene_object_id_.size() <= specified_command.object_id)
							{
								object_id_to_scene_object_id_.resize(object_id_to_scene_object_id_.size() * 3 / 2 + 1, ObjectInfo{ {}, (uint32_t)-1});
							}

							object_id_to_scene_object_id_[specified_command.object_id].type = ObjectType::Node;
							object_id_to_scene_object_id_[specified_command.object_id].id = node_id;

							scenes_[0].AddModel(node, *pack_model.mesh);
						}

						if (std::holds_alternative<command::AddObject<ObjectType::Node>>(command))
						{
							auto&& specified_command = std::get<command::AddObject<ObjectType::Node>>(command);

							auto node_id = scenes_[0].AddNode();
							auto&& node = scenes_[0].GetNode(node_id);

							while (object_id_to_scene_object_id_.size() <= specified_command.object_id)
							{
								object_id_to_scene_object_id_.resize(object_id_to_scene_object_id_.size() * 3 / 2 + 1, ObjectInfo{ {}, (uint32_t)-1 });
							}

							object_id_to_scene_object_id_[specified_command.object_id].type = ObjectType::Node;
							object_id_to_scene_object_id_[specified_command.object_id].id = node_id;
						}


						if (std::holds_alternative<command::AddObject<ObjectType::DbgPoints>>(command))
						{
							auto&& specified_command = std::get<command::AddObject<ObjectType::DbgPoints>>(command);

							model_packs[0].AddSimpleMesh(specified_command.desc.points, PrimitiveProps::kDebugPoints);
							std::get<primitive::Geometry>(model_packs[0].meshes.back().primitives.back()).material.color = specified_command.desc.color;

							auto node_id = scenes_[0].AddNode();
							auto&& node = scenes_[0].GetNode(node_id);

							while (object_id_to_scene_object_id_.size() <= specified_command.object_id)
							{
								object_id_to_scene_object_id_.resize(object_id_to_scene_object_id_.size() * 3 / 2 + 1, ObjectInfo{ {}, (uint32_t)-1 });
							}

							object_id_to_scene_object_id_[specified_command.object_id].type = ObjectType::Node;
							object_id_to_scene_object_id_[specified_command.object_id].id = node_id;

							scenes_[0].AddModel(node, model_packs[0].meshes.back());
						}

						if (std::holds_alternative<command::AddObject<ObjectType::Camera>>(command))
						{
							auto&& specified_command = std::get<command::AddObject<ObjectType::Camera>>(command);
						}

						if (std::holds_alternative<command::SetActiveCameraNode>(command))
						{
							auto&& specified_command = std::get<command::SetActiveCameraNode>(command);

							auto&& info = object_id_to_scene_object_id_[specified_command.node_id.value];
							scenes_[0].camera_node_id_ = { info.id };

							screen_panel = std::make_shared<ui::Panel>(scenes_[0], 0, 0, extents[u32(ExtentType::kPresentation)].width, extents[u32(ExtentType::kPresentation)].height);
							block = std::make_shared<ui::TextBlock>(ui, scenes_[0], descriptor_set_manager, 30, 30);
							block->SetText(U"�����", 30);
							screen_panel->AddChild(block);
						}

						if (std::holds_alternative<command::ObjectsUpdate>(command))
						{
							auto&& specified_command = std::get<command::ObjectsUpdate>(command);

							for (auto&& [id, transform] : specified_command.updates)
							{
								if (object_id_to_scene_object_id_.size() > id)
								{
									if (object_id_to_scene_object_id_[id].type == ObjectType::Node)
									{
										NodeId node_id = object_id_to_scene_object_id_[id].id;
										auto&& node = scenes_[0].GetNode({ node_id });
										node.local_transform = transform;
									}
								}
							}

							//model_packs[0].AddSimpleMesh(specified_command.desc.points, PrimitiveProps::kDebugPoints);
							//model_packs[0].meshes.back().primitives.back().material.color = specified_command.desc.color;

							//Node node{};
							//node.local_transform = glm::identity<glm::mat4>();
							//auto&& scene_node = scenes_[0].AddNode(node);
							//scenes_[0].AddModel(scene_node, model_packs[0].meshes.back());
						}
						
					}
				}

				vkDeviceWaitIdle(global_.logical_device);
			}

			platform::JoinWindowThread(surface_ptr_->GetWindow());
		}

		void SetDebugLines(const std::vector<std::pair<DebugPoint, DebugPoint>> lines)
		{
			std::vector<DebugGeometry::Line> debug_lines;

			for (auto&& line : lines)
			{
				debug_lines.push_back({
					{line.first.position,line.first.color},
					{line.second.position,line.second.color}
					}
				);
			}

			scenes_[0].debug_geometry_.SetDebugLines(debug_lines);
		}

		template<ObjectType Type>
		ObjectId<Type> AddObject(const ObjectDescription<Type>& desc)
		{
			uint32_t id = -1;

			if (!free_ids_.empty())
			{
				id = free_ids_.top();
				free_ids_.pop();
			}
			else
			{
				id = last_object_id_;
				last_object_id_++;
			}

			ObjectId<Type> result{ desc.name, id};
			external_command_queue_.Push(render::command::AddObject{ id, desc });

			return result;
		}

		~RenderEngineImpl()
		{

		}

		byes::sync_util::ConcQueue1P1C<command::Command> external_command_queue_;
		std::vector<Scene> scenes_;
	private:

		const std::vector<const char*>& GetRequiredDeviceExtensions()
		{
			static const std::vector<const char*> extensions{ "VK_KHR_swapchain", "VK_KHR_synchronization2"};
			return extensions;
		}

		const std::vector<const char*>& GetValidationLayers()
		{
#ifndef NDEBUG1
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
				InitPhysicalDeviceMemoryProperties(vk_physical_devices_[i]);
				InitPhysicalDeviceQueueFamaliesProperties(vk_physical_devices_[i]);
				InitPhysicalDeviceFeatures(vk_physical_devices_[i]);
				if (!(InitPhysicalDeviceLayers(vk_physical_devices_[i]) &&
					InitPhysicalDeviceExtensions(vk_physical_devices_[i])))
					return false;
			}
			return true;
		}

		std::map<VkFormat, VkImageFormatProperties> GetImageFormatsProperties()
		{
			std::map<VkFormat, VkImageFormatProperties> res;

			for (int i = 0; i < 125; i++)
			{
				VkImageFormatProperties prop;

				VkResult vk_res = vkGetPhysicalDeviceImageFormatProperties(
					global_.physical_device,
					static_cast<VkFormat>(i),
					VK_IMAGE_TYPE_2D,
					VK_IMAGE_TILING_OPTIMAL,
					VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					0,
					&prop);

				if (vk_res == VK_SUCCESS)
				{
					res.emplace(static_cast<VkFormat>(i), prop);
				}
			}

			return res;
		}


		uint32_t FindDeviceQueueFamalyWithFlag(const VkPhysicalDevice& physical_deivce, VkQueueFlags enabled_flags, VkQueueFlags disabled_flags = 0)
		{
			auto&& queue_famalies_properties = vk_physical_devices_to_queues_[physical_deivce];


			for (uint32_t i = 0; i < queue_famalies_properties.size(); i++)
			{
				if ((queue_famalies_properties[i].queueFlags & enabled_flags) == enabled_flags 
					&& (queue_famalies_properties[i].queueFlags & disabled_flags) == 0)
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
						if(physical_device_out == nullptr || 
							vk_physical_devices_propeties_[physical_device].deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
							)

						physical_device_out = physical_device;
					}
				}
			}

			auto physical_device_out_it = std::find(vk_physical_devices_.begin(), vk_physical_devices_.end(), physical_device_out);

			if (physical_device_out_it != vk_physical_devices_.end())
			{
				return static_cast<uint32_t>(physical_device_out_it - vk_physical_devices_.begin());
			}

			return -1;
		}


		void InitPhysicalDeviceMemoryProperties(VkPhysicalDevice physical_device)
		{
			using PhDevPropMapType = std::map<VkPhysicalDevice, VkPhysicalDeviceMemoryProperties>;

			auto it = vk_physical_devices_memory_propeties_.lower_bound(physical_device);

			if (it == vk_physical_devices_memory_propeties_.end() || it->first != physical_device)
			{
				it = vk_physical_devices_memory_propeties_.emplace_hint(it,
					std::piecewise_construct, std::forward_as_tuple(physical_device), std::forward_as_tuple());
				vkGetPhysicalDeviceMemoryProperties(physical_device, &it->second);
			}
		}

		void InitPhysicalDeviceProperties(VkPhysicalDevice physical_device)
		{
			using PhDevPropMapType = std::map<VkPhysicalDevice, VkPhysicalDeviceMemoryProperties>;

			auto it = vk_physical_devices_propeties_.lower_bound(physical_device);

			if (it == vk_physical_devices_propeties_.end() || it->first != physical_device)
			{
				it = vk_physical_devices_propeties_.emplace_hint(it,
					std::piecewise_construct, std::forward_as_tuple(physical_device), std::forward_as_tuple());
				vkGetPhysicalDeviceProperties(physical_device, &it->second);
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

				VkPhysicalDeviceFeatures2 features2;
				//VkPhysicalDeviceImagelessFramebufferFeatures imageless_features;
				VkPhysicalDeviceSynchronization2Features synchronization2_features;
				VkPhysicalDeviceVulkan13Features vk13_features;

				features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				features2.pNext = &synchronization2_features;

				//imageless_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES;
				//imageless_features.pNext = &synchronization2_features;

				synchronization2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
				synchronization2_features.pNext = &vk13_features;

				vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
				vk13_features.pNext = nullptr;


				vkGetPhysicalDeviceFeatures2(physical_device, &features2);
				int a = 1;
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

			if (util::All(GetRequiredDeviceExtensions(), vk_physical_devices_extensions_[physical_device], [](auto&& ext_name, auto&& ext) { return std::strcmp(ext_name, ext.extensionName) == 0; }))
			{
				logical_device_create_info.enabledExtensionCount = static_cast<uint32_t>(GetRequiredDeviceExtensions().size());
				logical_device_create_info.ppEnabledExtensionNames = GetRequiredDeviceExtensions().data();
			}
			else return false;

			//VkPhysicalDeviceImagelessFramebufferFeatures imageless_features;
			//imageless_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES;
			//imageless_features.pNext = nullptr;
			//imageless_features.imagelessFramebuffer = VK_TRUE;
			//VkPhysicalDeviceSynchronization2Features vk_synchronization2_features = {};
			VkPhysicalDeviceVulkan13Features vk13_features = {};

			//vk_synchronization2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
			//vk_synchronization2_features.synchronization2 = VK_TRUE;
			//vk_synchronization2_features.pNext = &vk13_features;

			vk13_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
			vk13_features.synchronization2 = VK_TRUE;
			vk13_features.pNext = nullptr;

			logical_device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			logical_device_create_info.pNext = &vk13_features;
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
		std::map<VkPhysicalDevice, VkPhysicalDeviceMemoryProperties> vk_physical_devices_memory_propeties_;
		std::map<VkPhysicalDevice, VkPhysicalDeviceProperties> vk_physical_devices_propeties_;
		std::map<VkPhysicalDevice, std::vector<VkQueueFamilyProperties>> vk_physical_devices_to_queues_;
		std::map<VkPhysicalDevice, VkPhysicalDeviceFeatures> vk_physical_devices_features_;
		std::vector<VkLayerProperties > vk_instance_layers_;
		std::map<VkPhysicalDevice, std::vector<VkLayerProperties>> vk_physical_devices_layers_;
		std::vector<VkExtensionProperties> vk_instance_extensions_;
		std::map<VkPhysicalDevice, std::vector<VkExtensionProperties>> vk_physical_devices_extensions_;
		
		std::vector<VkDeviceWrapper> vk_logical_devices_;

		Global global_;

		std::unique_ptr<Surface> surface_ptr_;

		std::unique_ptr<DescriptorPool> descriptor_pool_ptr_;
		std::unique_ptr<CommandPool> graphics_command_pool_ptr_;
		std::unique_ptr<CommandPool> transfer_command_pool_ptr_;



		std::thread render_thread_;



		bool ready;

		uint32_t last_object_id_;
		

		struct ObjectInfo
		{
			ObjectType type;
			util::UniId id;
		};

		std::stack<uint32_t> free_ids_;


		public:
		
		std::vector<ObjectInfo> object_id_to_scene_object_id_;



		//VkSemaphore image_available_semaphore_;
		//VkSemaphore render_finished_semaphore_;
};

	RenderEngine::RenderEngine(InitParam param)
	{
		impl_ = std::make_unique<RenderEngineImpl>(param);
	}

	void RenderEngine::SetDebugLines(const std::vector<std::pair<DebugPoint, DebugPoint>>& lines)
	{
		impl_->SetDebugLines(lines);
	}

	//void RenderEngine::UpdateCamera(uint32_t id, glm::vec3 pos, glm::vec3 dir)
	//{
	//	if (impl_)
	//	{
	//		impl_->scenes_[0].cameras_[impl_->scenes_[0].active_camera_].orientation = dir;
	//		impl_->scenes_[0].cameras_[impl_->scenes_[0].active_camera_].position = pos;
	//	}
	//}

	void RenderEngine::QueueCommand(const command::Command& render_command)
	{
		impl_->external_command_queue_.Push(render_command);
	}

	RenderEngine::~RenderEngine() = default;

	bool RenderEngine::VKInitSuccess()
	{
		return impl_->VKInitSuccess();
	}

	void RenderEngine::StartRender()
	{
		impl_->StartRenderThread();
	}

	const InputState& RenderEngine::GetInputState()
	{
		return platform::GetInputState();
	}

#define RENDER_ENGINE_OBJECT(x)																					\
	template<>																									\
	ObjectId<ObjectType::x> RenderEngine::AddObject(const ObjectDescription<ObjectType::x>& desc)				\
	{																											\
		return impl_->AddObject<ObjectType::x>(desc);															\
	}																											

#include "render\render_engine_objects.inl"

}

//render::ui::TextBlockProxy::TextBlock(render::RenderEngine::RenderEngineImpl& reimpl)
//{
//}
