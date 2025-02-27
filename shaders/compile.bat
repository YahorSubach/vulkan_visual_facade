
pushd %~dp0..\shaders\

glslc.exe test.vert -o vert.spv
glslc.exe test.frag -o frag.spv
glslc.exe test_2.vert -o vert_2.spv
glslc.exe test_2.frag -o frag_2.spv
glslc.exe color.vert -o color.vert.spv
glslc.exe color_skin.vert -o color_skin.vert.spv
glslc.exe color.frag -o color.frag.spv
glslc.exe shadow.vert -o shadow.vert.spv
glslc.exe shadow_skin.vert -o shadow_skin.vert.spv
glslc.exe shadow.frag -o shadow.frag.spv
glslc.exe ui.vert -o ui.vert.spv
glslc.exe ui.frag -o ui.frag.spv
glslc.exe bitmap.vert -o bitmap.vert.spv
glslc.exe bitmap.frag -o bitmap.frag.spv
glslc.exe collect_g_buffers.vert -o collect_g_buffers.vert.spv
glslc.exe collect_g_buffers.frag -o collect_g_buffers.frag.spv
glslc.exe build_g_buffers.vert -o build_g_buffers.vert.spv
glslc.exe build_g_buffers.frag -o build_g_buffers.frag.spv

popd