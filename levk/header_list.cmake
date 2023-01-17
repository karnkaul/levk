set(util_headers
  include/levk/util/async_queue.hpp
  include/levk/util/binary_file.hpp
  include/levk/util/bool.hpp
  include/levk/util/cli_opts.hpp
  include/levk/util/contiguous_storage.hpp
  include/levk/util/dyn_array.hpp
  include/levk/util/enum_array.hpp
  include/levk/util/enumerate.hpp
  include/levk/util/env.hpp
  include/levk/util/error.hpp
  include/levk/util/fixed_string.hpp
  include/levk/util/flex_array.hpp
  include/levk/util/hash_combine.hpp
  include/levk/util/id.hpp
  include/levk/util/loader.hpp
  include/levk/util/logger.hpp
  include/levk/util/monotonic_map.hpp
  include/levk/util/nvec3.hpp
  include/levk/util/path_tree.hpp
  include/levk/util/pinned.hpp
  include/levk/util/ptr.hpp
  include/levk/util/reader.hpp
  include/levk/util/thread_pool.hpp
  include/levk/util/time.hpp
  include/levk/util/type_id.hpp
  include/levk/util/unique_task.hpp
  include/levk/util/unique.hpp
  include/levk/util/version.hpp
  include/levk/util/visitor.hpp
  include/levk/util/zip_ranges.hpp
)

set(impl_headers
  include/levk/impl/defer_queue.hpp
  include/levk/impl/desktop_window.hpp
  include/levk/impl/vulkan_device.hpp
  include/levk/impl/vulkan_surface.hpp
)

set(imcpp_headers
  include/levk/imcpp/common.hpp
  include/levk/imcpp/drag_drop.hpp
  include/levk/imcpp/editor_window.hpp
  include/levk/imcpp/engine_status.hpp
  include/levk/imcpp/input_text.hpp
  include/levk/imcpp/inspector.hpp
  include/levk/imcpp/log_renderer.hpp
  include/levk/imcpp/mesh_importer.hpp
  include/levk/imcpp/reflector.hpp
  include/levk/imcpp/resource_list.hpp
  include/levk/imcpp/ring_buffer.hpp
  include/levk/imcpp/scene_graph.hpp
)

set(assets_headers
  include/levk/asset/asset_loader.hpp
  include/levk/asset/common.hpp
  include/levk/asset/gltf_importer.hpp
  include/levk/asset/uri.hpp
)

set(levk_headers
  ${util_headers}
  ${impl_headers}
  ${imcpp_headers}
  ${assets_headers}

  include/levk/asset_id.hpp
  include/levk/camera.hpp
  include/levk/component_factory.hpp
  include/levk/component.hpp
  include/levk/context.hpp
  include/levk/defines.hpp
  include/levk/engine.hpp
  include/levk/entity.hpp
  include/levk/geometry.hpp
  include/levk/graphics_common.hpp
  include/levk/graphics_device.hpp
  include/levk/image.hpp
  include/levk/input.hpp
  include/levk/interpolator.hpp
  include/levk/lights.hpp
  include/levk/material.hpp
  include/levk/mesh_geometry.hpp
  include/levk/mesh_primitive.hpp
  include/levk/node.hpp
  include/levk/pixel_map.hpp
  include/levk/render_resources.hpp
  include/levk/resource_map.hpp
  include/levk/resources.hpp
  include/levk/rgba.hpp
  include/levk/runtime.hpp
  include/levk/scene.hpp
  include/levk/service.hpp
  include/levk/serializable.hpp
  include/levk/serializer.hpp
  include/levk/shader.hpp
  include/levk/skeleton.hpp
  include/levk/skinned_mesh.hpp
  include/levk/static_mesh.hpp
  include/levk/surface.hpp
  include/levk/texture_sampler.hpp
  include/levk/texture.hpp
  include/levk/transform_animation.hpp
  include/levk/transform_controller.hpp
  include/levk/transform.hpp
  include/levk/uri.hpp
  include/levk/window_common.hpp
  include/levk/window_state.hpp
  include/levk/window.hpp
)
