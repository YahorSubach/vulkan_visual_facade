
file(GLOB_RECURSE render_engine_SRC
     "*.h"
     "*.cpp"
     "*.cc"
     "*.inl"
)

target_sources(render_engine 
	PRIVATE
	${render_engine_SRC}
)

#target_sources(render_engine 
#	PRIVATE
#		${CMAKE_CURRENT_LIST_DIR}/render_engine.cc
#		${CMAKE_CURRENT_LIST_DIR}/stl_util.h
#		${CMAKE_CURRENT_LIST_DIR}/common.h
#		${CMAKE_CURRENT_LIST_DIR}/platform.h
#		${CMAKE_CURRENT_LIST_DIR}/platform.cc
#		${CMAKE_CURRENT_LIST_DIR}/surface.h
#		${CMAKE_CURRENT_LIST_DIR}/surface.cc
#		${CMAKE_CURRENT_LIST_DIR}/sync_util.h
#		${CMAKE_CURRENT_LIST_DIR}/render/animator.h
#		${CMAKE_CURRENT_LIST_DIR}/render/animator.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/batch.h
#		${CMAKE_CURRENT_LIST_DIR}/render/batch.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/batches_manager.h
#		${CMAKE_CURRENT_LIST_DIR}/render/batches_manager.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/buffer.h
#		${CMAKE_CURRENT_LIST_DIR}/render/buffer.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/command_buffer.h
#		${CMAKE_CURRENT_LIST_DIR}/render/command_buffer.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/command_buffer_filler.h
#		${CMAKE_CURRENT_LIST_DIR}/render/command_buffer_filler.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/command_pool.h
#		${CMAKE_CURRENT_LIST_DIR}/render/command_pool.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/data_types.h
#		${CMAKE_CURRENT_LIST_DIR}/render/descriptor.h
#		${CMAKE_CURRENT_LIST_DIR}/render/descriptor_pool.h
#		${CMAKE_CURRENT_LIST_DIR}/render/descriptor_pool.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/descriptor_set.h
#		${CMAKE_CURRENT_LIST_DIR}/render/descriptor_set_holder.h
#		${CMAKE_CURRENT_LIST_DIR}/render/descriptor_set_layout.h
#		${CMAKE_CURRENT_LIST_DIR}/render/descriptor_set_layout.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/descriptor_sets_manager.h
#		${CMAKE_CURRENT_LIST_DIR}/render/descriptor_sets_manager.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/descriptor_types.inl
#		${CMAKE_CURRENT_LIST_DIR}/render/frame_handler.h
#		${CMAKE_CURRENT_LIST_DIR}/render/frame_handler.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/framebuffer.h
#		${CMAKE_CURRENT_LIST_DIR}/render/framebuffer.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/framebuffer_collection.h
#		${CMAKE_CURRENT_LIST_DIR}/render/framebuffer_collection.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/global.h
#		${CMAKE_CURRENT_LIST_DIR}/render/gltf_wrapper.h
#		${CMAKE_CURRENT_LIST_DIR}/render/gltf_wrapper.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/graphics_pipeline.h
#		${CMAKE_CURRENT_LIST_DIR}/render/graphics_pipeline.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/image.h
#		${CMAKE_CURRENT_LIST_DIR}/render/image.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/image_view.h
#		${CMAKE_CURRENT_LIST_DIR}/render/image_view.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/memory.h
#		${CMAKE_CURRENT_LIST_DIR}/render/memory.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/mesh.h
#		${CMAKE_CURRENT_LIST_DIR}/render/mesh.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/object_base.h
#		${CMAKE_CURRENT_LIST_DIR}/render/render_graph.h
#		${CMAKE_CURRENT_LIST_DIR}/render/render_graph.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/render_setup.h
#		${CMAKE_CURRENT_LIST_DIR}/render/render_setup.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/render_pass.h
#		${CMAKE_CURRENT_LIST_DIR}/render/render_pass.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/scene.h
#		${CMAKE_CURRENT_LIST_DIR}/render/scene.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/sampler.h
#		${CMAKE_CURRENT_LIST_DIR}/render/sampler.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/shader_module.h
#		${CMAKE_CURRENT_LIST_DIR}/render/shader_module.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/swapchain.h
#		${CMAKE_CURRENT_LIST_DIR}/render/swapchain.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/vertex_buffer.h
#		${CMAKE_CURRENT_LIST_DIR}/render/vertex_buffer.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/vertex_buffer_types.inl
#		${CMAKE_CURRENT_LIST_DIR}/render/vk_util.h
#
#
#		${CMAKE_CURRENT_LIST_DIR}/render/ui/panel.h
#		${CMAKE_CURRENT_LIST_DIR}/render/ui/panel.cc
#		${CMAKE_CURRENT_LIST_DIR}/render/ui/ui.h
#		${CMAKE_CURRENT_LIST_DIR}/render/ui/ui.cc
#)
