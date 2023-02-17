#include <levk/asset/asset_loader.hpp>
#include <levk/runtime.hpp>
#include <levk/scene.hpp>
#include <levk/serializer.hpp>
#include <levk/util/error.hpp>
#include <filesystem>

#include <levk/scene.hpp>

namespace levk {
namespace {
namespace fs = std::filesystem;

fs::path find_dir(fs::path exe, std::span<std::string_view const> patterns) {
	auto check = [patterns](fs::path const& prefix) {
		for (auto const pattern : patterns) {
			auto path = prefix / pattern;
			if (fs::is_directory(path)) { return path; }
		}
		return fs::path{};
	};
	while (!exe.empty()) {
		if (auto ret = check(exe); !ret.empty()) { return ret; }
		auto parent = exe.parent_path();
		if (exe == parent) { break; }
		exe = std::move(parent);
	}
	return {};
}
} // namespace

Runtime::Runtime(std::unique_ptr<Reader> reader, ContextFactory const& context_factory)
	: m_reader(std::move(reader)), m_context(context_factory.make(*m_reader)) {}

Runtime::ReturnCode Runtime::run() {
	try {
		setup_bindings();
		m_context.show();
		while (m_context.is_running()) {
			auto frame = m_context.next_frame();
			tick(frame);
			render();
		}
	} catch (levk::Error const& e) {
		logger::error("Runtime error: {}", e.what());
		return EXIT_FAILURE;
	} catch (std::exception const& e) {
		logger::error("Fatal error: {}", e.what());
		return EXIT_FAILURE;
	} catch (...) {
		logger::error("Unknown error");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

bool Runtime::load_into(Scene& out, Uri<Scene> const& uri, Bool reload_asset) const {
	auto flags = std::uint8_t{};
	if (reload_asset) { flags |= Reader::eReload; }
	auto json = AssetLoader::load_json(uri, *m_reader, flags);
	if (!json) { return {}; }
	return out.deserialize(json);
}

bool Runtime::save(Scene const& scene, Uri<Scene> const& uri) const {
	auto* file_reader = dynamic_cast<FileReader*>(m_reader.get());
	if (!file_reader) { return false; }
	auto json = dj::Json{};
	scene.serialize(json);
	return json.to_file(file_reader->absolute_path_for(uri.value()).c_str());
}

void Runtime::setup_bindings() {
	m_component_factory.get().bind<SkeletonController>();
	m_component_factory.get().bind<MeshRenderer>();

	m_serializer.get().bind<LitMaterial>();
	m_serializer.get().bind<UnlitMaterial>();
}
} // namespace levk

std::string levk::find_directory(char const* exe_path, std::span<std::string_view const> uri_patterns) {
	if (uri_patterns.empty()) { return {}; }
	return find_dir(exe_path, uri_patterns).generic_string();
}
