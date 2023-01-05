#ifndef RENDER_ENGINE_RENDER_VKVF_H_
#define RENDER_ENGINE_RENDER_VKVF_H_

#include <array>
#include <variant>
#include <vector>
#include <cmath>
#include <memory>
#include <windows.h>

#include "glm/vec3.hpp"
#include "tiny_gltf.h"

namespace render
{
#ifdef WIN32
	using InitParam = HINSTANCE;
#endif

	struct Camera
	{
		glm::vec3 position;
		glm::vec3 orientation;
		glm::vec3 up;
	};

	struct Scene
	{
		Scene();
		Camera& GetActiveCamera();
		const Camera& GetActiveCamera() const;

	private:
		class SceneImpl;
		std::unique_ptr<SceneImpl> impl_;
	};

	struct InputState
	{
		std::array<int, 0xFF> button_states;

		std::pair<int, int> mouse_position;
		std::pair<int, int> mouse_position_prev;

		std::pair<int, int> mouse_delta;
	};

	struct LoadCommand
	{
		std::shared_ptr<tinygltf::Model> model;
	};

	using RenderCommand = std::variant<LoadCommand>;


	class RenderEngine
	{
	public:
		RenderEngine(InitParam param);

		//void CreateGraphicsPipeline();
		void StartRender();

		Scene& GetCurrentScene();

		const InputState& GetInputState();

		struct DebugPoint
		{
			glm::vec3 position;
			glm::vec3 color;
		};

		void SetDebugLines(const std::vector<std::pair<DebugPoint, DebugPoint>>& lines);
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