#ifndef RENDER_ENGINE_RENDER_MEMORY_HOLDER_H_
#define RENDER_ENGINE_RENDER_MEMORY_HOLDER_H_

#include <vector>
#include <array>

#include "vulkan/vulkan.h"

#include "common.h"
#include "render/object_base.h"
#include "render/data_types.h"

namespace render
{
	class Memory : public RenderObjBase<VkDeviceMemory>
	{
	public:
		Memory(const DeviceConfiguration& device_cfg, uint64_t size, uint32_t memory_type_bits);
	
		Memory(const Memory&) = delete;
		Memory(Memory&&) = default;

		Memory& operator=(const Memory&) = delete;
		Memory& operator=(Memory&&) = default;

		virtual ~Memory() override;

		VkDeviceMemory GetMemoryHandle();

		static uint32_t GetMemoryTypeIndex(const DeviceConfiguration& device_cfg, uint32_t acceptable_memory_types_bits, VkMemoryPropertyFlags memory_flags);

	};
}
#endif  // RENDER_ENGINE_RENDER_MEMORY_HOLDER_H_