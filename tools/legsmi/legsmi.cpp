#include <fmt/format.h>
#include <legsmi/legsmi.hpp>
#include <levk/io/serializer.hpp>
#include <levk/scene/scene.hpp>
#include <levk/service.hpp>
#include <charconv>
#include <concepts>
#include <cstdio>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace legsmi {
namespace {

constexpr std::string_view usage_v{R"(Usage: 
  legsmi [--data-root=/path/] [--verbose] mesh [--dest-dir=uri/] <path.gltf> <asset_index>
  legsmi [--data-root=/path/] [--verbose] scene [--dest-dir=uri/] [--scene-uri=uri] <path.gltf> <asset_index>
  legsmi list <path.gltf>
)"};

struct Args {
	enum class Type { eMesh, eScene, eList };

	struct Parser;

	fs::path data_root{};
	Type type{};
	fs::path dest_dir{};
	fs::path scene_uri{};
	fs::path gltf{};
	std::size_t asset_index{};
	bool verbose{};
};

struct Args::Parser {
	std::span<char const* const> args;
	Args& out;

	std::string_view peek() const { return args.empty() ? std::string_view{} : args.front(); }

	std::string_view advance() {
		if (args.empty()) { return {}; }
		auto ret = args.front();
		args = args.subspan(1);
		return ret;
	}

	bool get_value(std::string_view key, levk::Ptr<std::string_view> out_value = {}) const {
		auto arg = peek();
		if (arg.starts_with(key)) {
			arg = arg.substr(key.size());
			if (arg.empty() || arg.front() != '=') { return false; }
			if (out_value) { *out_value = arg.substr(1); }
			return true;
		}
		return false;
	}

	bool try_parse_data_root() {
		auto value = std::string_view{};
		if (get_value("--data-root", &value)) {
			out.data_root = value;
			advance();
			return true;
		}
		return false;
	}

	bool parse_type() {
		auto const type = advance();
		if (type == "mesh") {
			out.type = Type::eMesh;
			return true;
		}
		if (type == "scene") {
			out.type = Type::eScene;
			return true;
		}
		if (type == "list") {
			out.type = Type::eList;
			return true;
		}
		return false;
	}

	bool try_parse_dest_dir() {
		auto value = std::string_view{};
		if (get_value("--dest-dir", &value)) {
			out.dest_dir = value;
			advance();
			return true;
		}
		return false;
	}

	bool try_parse_scene_uri() {
		auto value = std::string_view{};
		if (get_value("--scene-uri", &value)) {
			out.scene_uri = value;
			advance();
			return true;
		}
		return false;
	}

	bool try_parse_verbose() {
		if (peek() == "--verbose") {
			out.verbose = true;
			advance();
			return true;
		}
		return false;
	}

	bool parse_option() {
		if (try_parse_data_root()) { return true; }
		if (try_parse_verbose()) { return true; }
		return false;
	}

	bool parse_asset_option() {
		if (try_parse_dest_dir()) { return true; }
		if (out.type == Type::eScene && try_parse_scene_uri()) { return true; }
		return false;
	}

	bool parse(std::span<char const* const> const in) {
		args = in;
		while (peek().starts_with("--")) {
			if (!parse_option()) { return false; }
		}
		if (!parse_type()) { return false; }
		while (peek().starts_with("--")) {
			if (!parse_asset_option()) { return false; }
		}
		out.gltf = advance();
		if (out.type == Type::eList) { return peek().empty(); }
		auto const current = peek();
		auto [_, ec] = std::from_chars(current.data(), current.data() + current.size(), out.asset_index);
		if (ec != std::errc{}) { return false; }
		return true;
	}
};

using IndexRange = std::array<std::size_t, 2>;

template <typename Type>
constexpr IndexRange make_index_range(std::span<Type> assets) {
	auto ret = IndexRange{std::numeric_limits<std::size_t>::max(), 0u};
	for (auto const& asset : assets) {
		ret[0] = std::min(ret[0], asset.index);
		ret[1] = std::max(ret[1], asset.index);
	}
	return ret;
}

template <typename Type>
constexpr levk::Ptr<Type> find_by_index(std::span<Type> assets, std::size_t index) {
	if (index >= assets.size()) { return {}; }
	auto const it = std::ranges::find_if(assets, [index](auto const& asset) { return asset.index == index; });
	if (it != assets.end()) { return &*it; }
	return {};
}

constexpr std::string_view get_name(std::string_view const in) {
	if (in.empty()) { return "(Unnamed)"; }
	return in;
}

struct ImportList {
	AssetList asset_list{};
	IndexRange scenes{};
	IndexRange meshes{};

	static ImportList make(AssetList asset_list) {
		auto ret = ImportList{.asset_list = std::move(asset_list)};
		ret.meshes = make_index_range(std::span{ret.asset_list.meshes});
		ret.scenes = make_index_range(std::span{ret.asset_list.scenes});
		return ret;
	}
};

bool print_usage(bool error) {
	auto fptr = error ? stderr : stdout;
	std::fprintf(fptr, "%s", usage_v.data());
	return !error;
}

void list_scenes(ImportList const& import_list) {
	std::printf("== Scenes ==\n");
	for (auto const& scene : import_list.asset_list.scenes) {
		auto const text = fmt::format("[{:>2}]  {}\n", scene.index, get_name(scene.name));
		std::printf("%s", text.c_str());
	}
}

void list_meshes(ImportList const& import_list) {
	std::printf("== Meshes ==\n");
	for (auto const& mesh : import_list.asset_list.meshes) {
		static constexpr auto get_type = [](levk::MeshType type) {
			if (type == levk::MeshType::eSkinned) { return "Skinned"; }
			return "Static";
		};
		auto const text = fmt::format("[{:>2}]  {} ({})\n", mesh.index, get_name(mesh.name), get_type(mesh.mesh_type));
		std::printf("%s", text.c_str());
	}
}

struct Menu {};

struct App {
	Args args{};
	fs::path src_dir{};
	std::string gltf_filename{};
	ImportList import_list{};

	bool can_import_scene() const { return !import_list.asset_list.has_skinned_mesh; }
	LogDispatch import_logger{};

	bool run(std::span<char const* const> in) {
		if (!Args::Parser{in, args}.parse(in)) {
			print_usage(true);
			return false;
		}
		if (!fs::is_regular_file(args.gltf)) {
			std::fprintf(stderr, "%s", fmt::format("Invalid file path: {}\n", args.gltf.generic_string()).c_str());
			return false;
		}
		if (fs::is_regular_file(args.data_root)) {
			std::fprintf(stderr, "%s", fmt::format("Invalid data root: {}\n", args.data_root.generic_string()).c_str());
			return false;
		}
		if (args.type != Args::Type::eList && !fs::is_directory(args.data_root) && !fs::create_directories(args.data_root)) {
			std::fprintf(stderr, "%s", fmt::format("Failed to locate/create data root: {}\n", args.gltf.generic_string()).c_str());
			return false;
		}

		import_list = ImportList::make(peek_assets(args.gltf.string()));
		if (import_list.asset_list.scenes.empty() && import_list.asset_list.meshes.empty()) {
			std::fprintf(stderr, "%s", fmt::format("Could not find any assets in gltf file: {}\n", args.gltf.generic_string()).c_str());
			return false;
		}

		if (!args.verbose) {
			for (auto& level : import_logger.silenced) { level = true; }
		}
		src_dir = args.gltf.parent_path();
		gltf_filename = args.gltf.stem().filename().string();
		if (args.data_root.empty()) { args.data_root = fs::current_path(); }
		if (args.dest_dir.empty()) {
			args.dest_dir = args.gltf.stem();
			assert(!args.dest_dir.empty());
		}
		if (args.scene_uri.empty()) { args.scene_uri = args.dest_dir / args.dest_dir; }
		if (args.scene_uri.extension().empty()) { args.scene_uri += ".scene.json"; }

		switch (args.type) {
		case Args::Type::eList: list_assets(); return true;
		case Args::Type::eMesh: return import_mesh();
		case Args::Type::eScene: return import_scene();
		}

		return true;
	}

	void list_assets() const {
		std::printf("== Asset List ==\n\n");
		list_scenes(import_list);
		std::printf("\n");
		list_meshes(import_list);
	}

	bool import_mesh() {
		auto make_importer = [this] { return import_list.asset_list.mesh_importer(args.data_root.generic_string(), std::move(gltf_filename), import_logger); };
		return import_asset(std::span{import_list.asset_list.meshes}, "Mesh", make_importer);
	}

	bool import_scene() {
		auto make_importer = [this] {
			return import_list.asset_list.scene_importer(args.data_root.generic_string(), args.dest_dir.generic_string(), args.scene_uri.generic_string(),
														 import_logger);
		};
		return import_asset(std::span{import_list.asset_list.scenes}, "Scene", make_importer);
	}

	template <typename T, typename F>
	bool import_asset(std::span<T> assets, std::string_view type, F make_importer) {
		auto* asset = find_by_index(assets, args.asset_index);
		if (!asset) {
			std::fprintf(stderr, "%s", fmt::format("Invalid {} index: {}\n", type, args.asset_index).c_str());
			return false;
		}
		auto importer = make_importer();
		auto uri = importer.try_import(*asset);
		if (!uri) {
			std::fprintf(stderr, "%s", fmt::format("Failed to import {} [{}]\n", type, asset->index).c_str());
			return false;
		}
		std::printf("%s", fmt::format("[{}] {} imported successfully\n", uri.value(), type).c_str());
		return true;
	}

	bool to_do() {
		std::printf("\n==TODO==\n");
		return true;
	}
};
} // namespace
} // namespace legsmi

int main(int argc, char** argv) {
	auto serializer = levk::Service<levk::Serializer>::Instance{};
	serializer.get().bind<levk::LitMaterial>();
	serializer.get().bind<levk::UnlitMaterial>();
	serializer.get().bind<levk::MeshRenderer>();
	if (!legsmi::App{}.run({argv + 1, static_cast<std::size_t>(argc - 1)})) { return EXIT_FAILURE; }
}
