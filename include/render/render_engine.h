#ifndef RENDER_ENGINE_RENDER_VKVF_H_
#define RENDER_ENGINE_RENDER_VKVF_H_

#include <span>
#include <array>
#include <variant>
#include <vector>
#include <cmath>
#include <memory>
#include <windows.h>

#include "glm/vec3.hpp"
#include "glm/mat4x4.hpp"

#pragma warning(push, 0)
#include "tiny_gltf.h"
#pragma warning(pop)
namespace render
{
#ifdef WIN32
	using InitParam = HINSTANCE;
#endif

	template<typename ElementType>
	static std::span<const ElementType> GetBufferSpanByAccessor(const tinygltf::Model& gltf_model, int acc_ind)
	{
		auto&& buffer_acc = gltf_model.accessors[acc_ind];
		auto&& buffer_view = gltf_model.bufferViews[buffer_acc.bufferView];

		auto overriden_stride = buffer_view.byteStride;
		auto element_size = tinygltf::GetNumComponentsInType(buffer_acc.type) * tinygltf::GetComponentSizeInBytes(buffer_acc.componentType);

		assert(sizeof(ElementType) == element_size);

		auto actual_stride = overriden_stride > 0 ? overriden_stride : element_size;

		auto offset = buffer_view.byteOffset + buffer_acc.byteOffset;


		const ElementType* begin = reinterpret_cast<const ElementType*>(gltf_model.buffers[buffer_view.buffer].data.data() + offset);
		const ElementType* end = begin + (buffer_acc.count);

		std::span<const ElementType> result(begin, end);

		return result;
	}

	enum class ObjectType
	{
		Node,
		Camera,
		StaticModel,
		DbgPoints
	};

	struct Camera
	{
		glm::vec3 position;
		glm::vec3 orientation;
		glm::vec3 up;
	};

	struct InputState
	{
		std::array<int, 0xFF> button_states;
		std::pair<int, int> mouse_position;
	};

	struct LoadCommand
	{
		std::string pack_name;
		std::shared_ptr<tinygltf::Model> model;
	};


	struct DbgPoints
	{
		std::vector<glm::vec3> poists;
		glm::vec4 color;
	};

	struct GeomCommand
	{
		std::vector<glm::vec3> faces;
	};

	struct ObjectsUpdate
	{
		std::vector<std::pair<uint32_t, glm::mat4>> updates;
	};

	template<ObjectType Type> 
	struct ObjectDescription
	{
		std::string name;
	};

	template<>
	struct ObjectDescription<ObjectType::StaticModel>
	{
		std::string pack_name;
		std::string model_name;
	};

	template<>
	struct ObjectDescription<ObjectType::DbgPoints>
	{
		std::vector<glm::vec3> points;
		glm::vec4 color;
	};

	template<ObjectType Type>
	struct AddObjectCommand
	{
		ObjectDescription<Type> desc;
	};
	
	using RenderCommand = std::variant<LoadCommand, GeomCommand, ObjectsUpdate, 
		AddObjectCommand<ObjectType::Camera>,
		AddObjectCommand<ObjectType::StaticModel>,
		AddObjectCommand<ObjectType::DbgPoints>
	>;



	template<ObjectType Type>
	struct ObjectId
	{
		ObjectId() : id(-1) {}
		ObjectId(const std::string& name, uint32_t id) : name(name), id(id) {}
		std::string name;
		uint32_t id;
	};

	class RenderEngine
	{
	public:
		RenderEngine(InitParam param);

		//void CreateGraphicsPipeline();
		void StartRender();

		const InputState& GetInputState();

		struct DebugPoint
		{
			glm::vec3 position;
			glm::vec3 color;
		};

		void SetDebugLines(const std::vector<std::pair<DebugPoint, DebugPoint>>& lines);

		template<ObjectType Type>
		ObjectId<Type> AddObject(const ObjectDescription<Type>& desc);

		//void UpdateCamera(uint32_t id, glm::vec3 pos, glm::vec3 dir);

		void QueueCommand(const RenderCommand& render_command);

		~RenderEngine();

		bool VKInitSuccess();
		//class RenderEngineImpl;
	private:
		class RenderEngineImpl;
		std::unique_ptr<RenderEngineImpl> impl_;
	};
}
#endif  // RENDER_ENGINE_RENDER_VKVF_H_