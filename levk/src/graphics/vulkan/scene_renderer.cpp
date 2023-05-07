#include <graphics/vulkan/material.hpp>
#include <graphics/vulkan/primitive.hpp>
#include <graphics/vulkan/scene_renderer.hpp>
#include <graphics/vulkan/shader.hpp>
#include <levk/asset/asset_providers.hpp>
#include <levk/defines.hpp>
#include <levk/graphics/material.hpp>
#include <levk/scene/scene.hpp>
#include <levk/util/logger.hpp>

namespace levk::vulkan {
namespace {
auto const g_log{Logger{"SceneRenderer"}};

SceneRenderer::Frame build_render_frame(SceneRenderer& scene_renderer, Scene const& scene, RenderList const& render_list) {
	auto ret = SceneRenderer::Frame{
		.primary_light_direction = scene.lights.primary.direction,
		.camera_3d = scene.camera,
	};

	using Std430DirLight = SceneRenderer::Frame::Std430DirLight;

	auto dir_lights = FlexArray<Std430DirLight, max_lights_v>{};
	dir_lights.insert(Std430DirLight::make(scene.lights.primary));
	if (!scene.lights.dir_lights.empty()) {
		for (auto const& light : scene.lights.dir_lights) {
			dir_lights.insert(Std430DirLight::make(light));
			if (dir_lights.size() == dir_lights.capacity_v) { break; }
		}
	}
	scene_renderer.dir_lights_ssbo.write(dir_lights.span().data(), dir_lights.span().size_bytes());

	auto opaque = DrawList{};
	auto transparent = DrawList{};
	opaque.import_skins(render_list.scene.skins());
	transparent.import_skins(render_list.scene.skins());
	for (auto const& drawable : render_list.scene.drawables()) {
		if (drawable.material->is_opaque()) {
			opaque.add(drawable);
		} else {
			transparent.add(drawable);
		}
	}

	auto& buffer_pool = scene_renderer.buffer_pools[*scene_renderer.device.buffered_index];

	opaque.sort_by([](Drawable const& a, Drawable const& b) { return a.material.get() < b.material.get(); });
	ret.opaque = RenderObject::build_objects(opaque, buffer_pool);

	transparent.sort_by([camera_position = scene.camera.transform.position()](Drawable const& a, Drawable const& b) {
		auto const transform_a = Transform::from(a.parent);
		auto const transform_b = Transform::from(b.parent);
		auto const sqr_dist_a = glm::length2(transform_a.position() - camera_position);
		auto const sqr_dist_b = glm::length2(transform_b.position() - camera_position);
		return sqr_dist_a < sqr_dist_b;
	});
	ret.transparent = RenderObject::build_objects(transparent, buffer_pool);

	ret.ui = RenderObject::build_objects(render_list.ui, buffer_pool);

	opaque.clear();
	scene_renderer.collision_renderer.render(opaque);
	opaque.sort_by([](Drawable const& a, Drawable const& b) { return a.material.get() < b.material.get(); });
	ret.overlay = RenderObject::build_objects(opaque, buffer_pool);

	return ret;
}

constexpr RenderMode combine(RenderMode const in, RenderMode def) {
	if (in.type != RenderMode::Type::eDefault) {
		def.type = in.type;
		def.line_width = in.line_width;
	}
	def.depth_test = in.depth_test;
	return def;
}

constexpr vk::PolygonMode from(RenderMode::Type const mode) {
	switch (mode) {
	case RenderMode::Type::ePoint: return vk::PolygonMode::ePoint;
	case RenderMode::Type::eLine: return vk::PolygonMode::eLine;
	default: return vk::PolygonMode::eFill;
	}
}

constexpr vk::PrimitiveTopology from(Topology const topo) {
	switch (topo) {
	case Topology::ePointList: return vk::PrimitiveTopology::ePointList;
	case Topology::eLineList: return vk::PrimitiveTopology::eLineList;
	case Topology::eLineStrip: return vk::PrimitiveTopology::eLineStrip;
	case Topology::eTriangleStrip: return vk::PrimitiveTopology::eTriangleStrip;
	default: return vk::PrimitiveTopology::eTriangleList;
	}
}

SceneRenderer::GlobalLayout make_global_layout(vk::Device device) {
	auto ret = SceneRenderer::GlobalLayout{};
	static constexpr auto stages_v = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
	auto const dslb = vk::DescriptorSetLayoutBinding{0u, vk::DescriptorType::eUniformBuffer, 1u, stages_v};
	auto const dslci = vk::DescriptorSetLayoutCreateInfo{{}, dslb};
	ret.global_set_layout = device.createDescriptorSetLayoutUnique(dslci);
	auto plci = vk::PipelineLayoutCreateInfo{};
	plci.setLayoutCount = 1u;
	plci.pSetLayouts = &*ret.global_set_layout;
	ret.global_pipeline_layout = device.createPipelineLayoutUnique(plci);
	return ret;
}

Geometry make_wire_cube(glm::vec3 const& size, glm::vec3 const& origin) {
	auto geometry = Geometry::from(Cube{.size = size, .origin = origin});
	// clang-format off
	geometry.indices = {
		0, 1, 1, 2, 2, 3, 3, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		8, 9, 9, 10, 10, 11, 11, 8,
		12, 13, 13, 14, 14, 15, 15, 12,
		16, 17, 17, 18, 18, 19, 19, 16,
		20, 21, 21, 22, 22, 23, 23, 20,
	};
	// clang-format on
	return geometry;
}

struct Drawer {
	DeviceView device;
	AssetProviders const& asset_providers;
	PipelineBuilder& pipeline_builder;
	vk::Extent2D extent;
	vk::CommandBuffer cb;

	BufferView dir_lights_ssbo{};
	ImageView shadow_map{};

	Ptr<Material const> previous_material{};

	void write_per_mat_sets(RenderObject const& object, Shader& shader) const {
		if (dir_lights_ssbo.buffer) { shader.update(Lights::set_v, DirLight::binding_v, dir_lights_ssbo); }
		if (shadow_map.view) {
			static constexpr auto shadow_sampler_v = TextureSampler{
				.wrap_s = TextureSampler::Wrap::eClampEdge,
				.wrap_t = TextureSampler::Wrap::eClampEdge,
				.border = TextureSampler::Border::eOpaqueWhite,
			};
			shader.update(Lights::set_v, RenderObject::Shadow::descriptor_binding_v, shadow_map, shadow_sampler_v);
		}
		object.drawable.material->write_sets(shader, asset_providers.texture());
	}

	void draw(RenderObject const& object) {
		auto* primitive = object.drawable.primitive.get();
		auto* material = object.drawable.material->vulkan_material();
		if (!primitive || !material) { return; }
		if (!material->build_layout(pipeline_builder, object.drawable.material->vertex_shader, object.drawable.material->fragment_shader)) { return; }
		auto rm = combine(object.drawable.material->render_mode, device.default_render_mode);
		auto const pipeline_state = PipelineState{
			.mode = from(rm.type),
			.topology = from(object.drawable.topology),
			.depth_test = rm.depth_test,
		};
		auto pipeline = pipeline_builder.try_build(primitive->layout().vertex_input, pipeline_state, material->shader_layout.hash);
		if (!pipeline) { return; }

		pipeline.bind(cb, extent, rm.line_width);
		auto shader = Shader{device, pipeline};
		if (material != previous_material) {
			write_per_mat_sets(object, shader);
			previous_material = material;
		}
		if (object.instances.mats_vbo.buffer) { cb.bindVertexBuffers(object.instances.vertex_binding_v, object.instances.mats_vbo.buffer, vk::DeviceSize{0}); }
		if (object.joints.mats_ssbo.buffer) { shader.update(object.joints.descriptor_set_v, object.joints.descriptor_binding_v, object.joints.mats_ssbo); }
		shader.bind(pipeline.layout, cb);
		primitive->draw(cb, object.instances.count);
		++*device.draw_calls;
	}
};
} // namespace

CollisionRenderer::CollisionRenderer(DeviceView device) : m_pool{device} {
	static constexpr RenderMode render_mode_v{
		.line_width = 3.0,
		.type = RenderMode::Type::eLine,
		.depth_test = false,
	};
	m_red.render_mode = m_green.render_mode = render_mode_v;
	m_red.tint = red_v;
	m_green.tint = green_v;
}

auto CollisionRenderer::Pool::next() -> Entry& {
	if (current_index >= entries.size()) { entries.push_back(Entry{.primitive = device}); }
	return entries[current_index++];
}

void CollisionRenderer::update(Collision const& collision) {
	m_pool.current_index = {};
	if (collision.draw_aabbs) {
		for (auto const& [id, in] : collision.entries()) {
			auto& out = m_pool.next();
			out.primitive.geometry = make_wire_cube(in.aabb.size, in.aabb.origin);
			out.material = in.colliding ? &m_red : &m_green;
		}
	}
}

void CollisionRenderer::render(DrawList& out) const {
	for (std::size_t i = 0; i < m_pool.current_index; ++i) {
		auto const& render_entry = m_pool.entries[i];
		out.add(Drawable{
			.primitive = &render_entry.primitive,
			.material = render_entry.material,
			.topology = Topology::eLineList,
		});
	}
}

SceneRenderer::SceneRenderer(DeviceView const& device)
	: device(device), global_layout(make_global_layout(device.device)), collision_renderer(device), device_block(device.device) {
	for (auto& buffer_pool : buffer_pools) { buffer_pool = HostBuffer::Pool::make(device); }

	view_ubo_3d = HostBuffer::make(device, vk::BufferUsageFlagBits::eUniformBuffer);
	view_ubo_ui = HostBuffer::make(device, vk::BufferUsageFlagBits::eUniformBuffer);
	dir_lights_ssbo = HostBuffer::make(device, vk::BufferUsageFlagBits::eStorageBuffer);

	for (auto& pool : buffer_pools) { pool.device = device; }
}

void SceneRenderer::update(Scene const& scene) { collision_renderer.update(scene.collision); }

void SceneRenderer::next_frame() {
	assert(scene && render_list);
	buffer_pools[*device.buffered_index].reset_all();

	if constexpr (debug_v) {
		static constexpr auto warn_size_v{2048};
		auto const list_size = render_list->size();
		if (list_size > warn_size_v) { g_log.warn("RenderList contains [{}] drawables / draw calls, consider reducing the count.", list_size); }
	}

	frame = build_render_frame(*this, *scene, *render_list);
}

void SceneRenderer::render_shadow(vk::CommandBuffer cb, Depthbuffer& depthbuffer) {
	if (frame.opaque.empty()) { return; }

	static auto const vertex_input = VertexInput::for_shadow();

	auto const format = depthbuffer.pipeline_format();
	auto pipeline_builder = PipelineBuilder{*device.pipeline_storage, asset_providers->shader(), device.device, format};
	auto layout = pipeline_builder.try_build_layout("shaders/shadow.vert", "shaders/noop.frag");
	assert(layout);
	auto pipeline = pipeline_builder.try_build(vertex_input, {}, layout->hash);
	assert(pipeline);
	pipeline.bind(cb, depthbuffer.image.extent);

	auto& view_buffer = buffer_pools[*device.buffered_index].next(vk::BufferUsageFlagBits::eUniformBuffer);
	assert(scene);
	auto const view_plane = ViewPlane{.near = -0.5f * scene->shadow_frustum.z, .far = 0.5f * scene->shadow_frustum.z};
	auto camera = Camera{.type = Camera::Orthographic{.view_plane = view_plane}, .face = Camera::Face::ePositiveZ};
	camera.transform.set_orientation(frame.primary_light_direction);
	camera.transform.set_position(frame.camera_3d.transform.position());
	frame.primary_light_mat = camera.projection(scene->shadow_frustum) * camera.view();
	view_buffer.write(&frame.primary_light_mat, sizeof(frame.primary_light_mat));
	auto shader = Shader{device, pipeline};
	shader.update(0, 0, view_buffer.view());
	shader.bind(pipeline.layout, cb);

	for (auto const& object : frame.opaque) {
		auto* primitive = object.drawable.primitive.get();
		assert(primitive);
		if (!object.instances.mats_vbo.buffer || primitive->layout().joints_binding) { continue; }
		cb.bindVertexBuffers(*primitive->layout().instances_binding, object.instances.mats_vbo.buffer, vk::DeviceSize{0});
		primitive->draw(cb, object.instances.count);
	}
}

void SceneRenderer::render_3d(vk::CommandBuffer cb, Framebuffer& framebuffer, ImageView const& shadow_map) {
	if (frame.opaque.empty() && frame.transparent.empty() && frame.overlay.empty()) { return; }

	auto const format = framebuffer.pipeline_format();
	auto pipeline_builder = PipelineBuilder{*device.pipeline_storage, asset_providers->shader(), device.device, format};
	auto drawer_3d = Drawer{device, *asset_providers, pipeline_builder, framebuffer.colour.extent, cb, dir_lights_ssbo.view(), shadow_map};
	bind_view_set(cb, view_ubo_3d, frame.camera_3d, {drawer_3d.extent.width, drawer_3d.extent.height});

	for (auto const& object : frame.opaque) { drawer_3d.draw(object); }
	for (auto const& object : frame.transparent) { drawer_3d.draw(object); }
	for (auto const& object : frame.overlay) { drawer_3d.draw(object); }
}

void SceneRenderer::render_ui(vk::CommandBuffer cb, Framebuffer& framebuffer, ImageView const& output_3d) {
	auto const format = framebuffer.pipeline_format();
	auto pipeline_builder = PipelineBuilder{*device.pipeline_storage, asset_providers->shader(), device.device, format};
	draw_3d_to_ui(cb, output_3d, pipeline_builder, framebuffer.output().extent);

	auto drawer_ui = Drawer{device, *asset_providers, pipeline_builder, framebuffer.colour.extent, cb};
	auto camera = Camera{.type = Camera::Orthographic{}};
	bind_view_set(drawer_ui.cb, view_ubo_ui, camera, {drawer_ui.extent.width, drawer_ui.extent.height});
	for (auto const& object : frame.ui) { drawer_ui.draw(object); }
}

void SceneRenderer::bind_view_set(vk::CommandBuffer cb, HostBuffer& out_ubo, Camera const& camera, glm::uvec2 const extent) {
	auto const view = Frame::Std140View{
		.mat_vp = camera.projection(extent) * camera.view(),
		.vpos_exposure = {camera.transform.position(), camera.exposure},
		.mat_shadow = frame.primary_light_mat,
		.shadow_dir = glm::vec4{frame.primary_light_direction * front_v, 1.0f},
	};
	out_ubo.write(&view, sizeof(view));
	auto const buffer_view = out_ubo.view();
	auto set0 = device.set_allocator->allocate(*global_layout.global_set_layout);
	auto const dbi = vk::DescriptorBufferInfo{buffer_view.buffer, {}, buffer_view.size};
	auto wds = vk::WriteDescriptorSet{set0, 0u, 0u, 1u, vk::DescriptorType::eUniformBuffer};
	wds.pBufferInfo = &dbi;
	device.device.updateDescriptorSets(wds, {});

	cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *global_layout.global_pipeline_layout, 0u, set0, {});
}

void SceneRenderer::draw_3d_to_ui(vk::CommandBuffer cb, ImageView const& output_3d, PipelineBuilder& pipeline_builder, vk::Extent2D extent) {
	auto layout = pipeline_builder.try_build_layout("shaders/fs_quad.vert", "shaders/fs_quad.frag");
	assert(layout);
	auto pipeline = pipeline_builder.try_build({}, {}, layout->hash);
	assert(pipeline);
	pipeline.bind(cb, extent, {}, false);
	auto shader = Shader{device, pipeline};
	shader.update(0, 0, output_3d, {});
	shader.bind(pipeline.layout, cb);
	cb.draw(6u, 1u, 0u, 0u);
}
} // namespace levk::vulkan
