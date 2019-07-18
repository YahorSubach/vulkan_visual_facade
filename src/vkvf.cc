#include "vkvf.h"

#include <vector>
#include <map>
#include <tuple>

#include "vulkan/vulkan.h"

namespace vkvf
{
	class VKVisualFacade::VKVisualFacadeImpl
	{
	public:

		VKVisualFacadeImpl(const VKVisualFacadeImpl&) = delete;
		VKVisualFacadeImpl& operator=(const VKVisualFacadeImpl&) = delete;

		VKVisualFacadeImpl()
		{
			vk_init_success_ = false;

			auto application_info = std::make_unique<VkApplicationInfo>();
			application_info->apiVersion = VK_HEADER_VERSION;
			application_info->applicationVersion = 1;
			application_info->engineVersion = 1;
			application_info->pApplicationName = "vulkan_concepts";
			application_info->pEngineName = "vulkan_concepts_engine";
			application_info->sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;

			auto create_info = std::make_unique<VkInstanceCreateInfo>();
			create_info->enabledExtensionCount = 0;
			create_info->enabledLayerCount = 0;
			create_info->pApplicationInfo = application_info.get();
			create_info->pNext = nullptr;
			create_info->sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

			vk_instance_ = std::make_unique<VkInstance>();

			if (vkCreateInstance(create_info.get(), nullptr, vk_instance_.get()) == VK_SUCCESS)
			{
				if (InitPhysicalDevices())
					vk_init_success_ = true;
			}
		}

		bool VKInitSuccess()
		{
			return vk_init_success_;
		}

	private:

		bool InitPhysicalDevices()
		{
			uint32_t physical_devices_count;

			if (vkEnumeratePhysicalDevices(*vk_instance_, &physical_devices_count, nullptr) != VK_SUCCESS)
				return false;

			vk_physical_devices_.resize(physical_devices_count);

			if (vkEnumeratePhysicalDevices(*vk_instance_, &physical_devices_count, reinterpret_cast<VkPhysicalDevice*>(vk_physical_devices_.data())) != VK_SUCCESS)
				return false;

			for (size_t i = 0; i < physical_devices_count; i++)
			{
				InitPhysicalDeviceProperties(vk_physical_devices_[i]);
				InitPhysicalDeviceQueueFamaliesProperties(vk_physical_devices_[i]);
				if (!InitLogicalDevice(vk_physical_devices_[i]))
					return false;
			}

			return true;
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

			auto it = vk_physical_devices_queue_famalies_propeties_.lower_bound(physical_device);

			if (it == vk_physical_devices_queue_famalies_propeties_.end() || it->first != physical_device)
			{
				uint32_t device_queue_famaly_prop_cnt;
				vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &device_queue_famaly_prop_cnt, nullptr);

				it = vk_physical_devices_queue_famalies_propeties_.emplace_hint(it,
					std::piecewise_construct, std::forward_as_tuple(physical_device), std::forward_as_tuple());

				it->second.resize(device_queue_famaly_prop_cnt);

				vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &device_queue_famaly_prop_cnt, it->second.data());
			}
		}


		void InitPhysicalDeviceFeatures(VkPhysicalDevice physical_device)
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

		bool InitLogicalDevice(VkPhysicalDevice physical_device)
		{
			VkDeviceCreateInfo logical_device_create_info;
			logical_device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			logical_device_create_info.pNext = nullptr;
			logical_device_create_info.flags = 0;
			logical_device_create_info.enabledExtensionCount = 0;
			logical_device_create_info.enabledLayerCount = 0;
			logical_device_create_info.ppEnabledExtensionNames = nullptr;
			logical_device_create_info.ppEnabledLayerNames = nullptr;
			logical_device_create_info.pEnabledFeatures = &vk_physical_devices_features_[physical_device];



			VkDeviceQueueCreateInfo queueu_create_info;
			queueu_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueu_create_info.flags = 0;
			queueu_create_info.queueFamilyIndex = 0;
			queueu_create_info.queueCount = vk_physical_devices_queue_famalies_propeties_[physical_device][0].queueCount;
			queueu_create_info.pQueuePriorities = nullptr;
			queueu_create_info.pNext = nullptr;

			logical_device_create_info.queueCreateInfoCount = 1;
			logical_device_create_info.pQueueCreateInfos = &queueu_create_info;

			vk_logical_devices_.emplace_back();

			if (vkCreateDevice(physical_device, &logical_device_create_info, nullptr, &vk_logical_devices_[vk_logical_devices_.size() - 1]) != VK_SUCCESS)
				return false;
			return true;
		}

		bool vk_init_success_;
		std::unique_ptr<VkApplicationInfo> application_info_;
		std::unique_ptr<VkInstance> vk_instance_;

		std::vector<VkPhysicalDevice> vk_physical_devices_;
		std::map<VkPhysicalDevice, VkPhysicalDeviceMemoryProperties> vk_physical_devices_propeties_;
		std::map<VkPhysicalDevice, std::vector<VkQueueFamilyProperties>> vk_physical_devices_queue_famalies_propeties_;
		std::map<VkPhysicalDevice, VkPhysicalDeviceFeatures> vk_physical_devices_features_;

		std::vector<VkDevice> vk_logical_devices_;

	};


	VKVisualFacade::VKVisualFacade()
	{
		impl_ = std::make_unique<VKVisualFacadeImpl>();
	}

	VKVisualFacade::~VKVisualFacade() = default;

	bool VKVisualFacade::VKInitSuccess()
	{
		return impl_->VKInitSuccess();
	}
}