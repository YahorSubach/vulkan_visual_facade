#ifndef RENDER_ENGINE_RENDER_IMAGE_H_
#define RENDER_ENGINE_RENDER_IMAGE_H_

#include <vector>
#include <array>
#include <memory>

#include "vulkan/vulkan.h"

#include "common.h"
#include "render/object_base.h"
#include "render/command_pool.h"
#include "render/buffer.h"
#include "render/memory.h"

namespace render
{
	//enum class ImageType
	//{
	//	kUndefined,

	//	kSwapchainImage,
	//	kColorImage,
	//	kColorAttachmentImage,
	//	kDepthMapImage,
	//	kGDepthImage,
	//	kBitmapImage,
	//};

	//enum class ImageProperty
	//{
	//	kShouldNotFreeHandle,

	//	kShaderInput,

	//	kColorAttachment,
	//	kDepthAttachment,

	//	kMipMap,
	//	kRead,
	//	kWrite,


	//	kLoad = kWrite,
	//};


	//class ImagePropertiesStorage
	//{
	//private:
	//	uint32_t flags_;
	//
	//public:
	//	ImagePropertiesStorage(std::initializer_list<ImageProperty> properties_list): flags_(0) {
	//		
	//		for (auto&& prop : properties_list)
	//		{
	//			flags_ |= (1 << static_cast<uint32_t>(prop));
	//		}
	//	}

	//	bool Check(ImageProperty prop) const
	//	{
	//		return flags_ & (1 << static_cast<uint32_t>(prop));
	//	}

	//	const ImagePropertiesStorage& Set(ImageProperty prop)
	//	{
	//		flags_ |= (1 << static_cast<uint32_t>(prop));
	//		return *this;
	//	}

	//	const ImagePropertiesStorage& Unset(ImageProperty prop)
	//	{
	//		flags_ &= ~(1 << static_cast<uint32_t>(prop));
	//		return *this;
	//	}

	//};



	class Image : public LazyRenderObj<VkImage>
	{
	public:
		
		enum class TransitionType
		{
			kTransferSrc,
			kTransferDst,
			kFragmentRead,
		};

		enum class BuiltinImageType
		{
			kBlack,
			kWhite,
			kError,
			kNormal,
		};

		Image(const Global& global, VkFormat format, Extent extent, const unsigned char* pixels);
		Image(const Global& global, VkFormat format, Extent extent, uint32_t layer_cnt = 1);
		Image(const Global& global, VkFormat format, VkImage image_handle);
		Image(const Global& global, BuiltinImageType type);

		static Image FromFile(const Global& global, const std::string_view& path);

		Image(const Image&) = delete;
		Image(Image&&) = default;

		Image& operator=(const Image&) = delete;
		Image& operator=(Image&&) = default;

		VkFormat GetFormat() const;
		uint32_t GetLayerCount() const;


		void TransitionImageLayout(const CommandPool& command_pool, TransitionType transfer_type) const;
		void CopyBuffer(const CommandPool& command_pool, const Buffer& buffer) const;

		virtual ~Image() override;

		uint32_t GetMipMapLevelsCount() const;

		uint32_t AddUsageFlag(uint32_t flag) const;
		uint32_t CheckUsageFlag(uint32_t flag) const;

		Extent GetExtent() const;

	private:

		virtual bool InitHandle() const override;

		Extent extent_;

		void GenerateMipMaps() const;

		mutable std::unique_ptr<std::vector<unsigned char>> pixels_data_;
		mutable std::unique_ptr<Memory> memory_;

		VkFormat format_;

		uint32_t layer_cnt_ = 1;
		uint32_t mipmap_levels_count_;
		mutable uint32_t usage_;

		bool holds_external_handle_;

	};
}
#endif  // RENDER_ENGINE_RENDER_IMAGE_H_