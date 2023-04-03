#include <fmt/format.h>
#include <legsmi/legsmi.hpp>
#include <levk/io/serializer.hpp>
#include <levk/scene/scene.hpp>
#include <levk/service.hpp>
#include <levk/util/cli_args.hpp>
#include <levk/util/logger.hpp>
#include <charconv>
#include <concepts>
#include <cstdio>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
namespace cli_args = levk::cli_args;

namespace legsmi {
namespace {
enum class Command { eMesh, eScene, eList };

struct Args {
	struct Parser;

	fs::path data_root{};
	Command command{};
	fs::path dest_dir{};
	fs::path gltf{};
	std::vector<std::size_t> asset_indices{};
	bool verbose{};
};

struct Args::Parser : cli_args::Parser {
	Args args;

	bool parse_command(std::string_view arg) {
		if (arg == "mesh") {
			args.command = Command::eMesh;
			return true;
		}
		if (arg == "scene") {
			args.command = Command::eScene;
			return true;
		}
		if (arg == "list") {
			args.command = Command::eList;
			return true;
		}
		std::fprintf(stderr, "%s", fmt::format("invalid command: {}\n", arg).c_str());
		return false;
	}

	bool option(cli_args::Key key, cli_args::Value value) override {
		switch (key.single) {
		case '\0':
		default: break;
		case 'v': args.verbose = true; return true;
		}

		if (key.full == "data_root") {
			args.data_root = value;
		} else if (key.full == "dest_dir") {
			args.dest_dir = value;
		} else {
			return false;
		}

		return true;
	}

	bool arguments(std::span<char const* const> in) override {
		if (in.empty()) {
			std::fprintf(stderr, "missing required argument: <path.gltf>\n");
			return false;
		}
		args.gltf = in[0];
		for (in = in.subspan(1); !in.empty(); in = in.subspan(1)) {
			auto const arg = std::string_view{in.front()};
			auto asset_index = std::size_t{};
			auto [ptr, ec] = std::from_chars(arg.data(), arg.data() + arg.size(), asset_index);
			if (ec != std::errc{} || ptr != arg.data() + arg.size()) {
				std::fprintf(stderr, "%s", fmt::format("invalid index, must be integral: {}\n", arg).c_str());
				return false;
			}
			args.asset_indices.push_back(asset_index);
		}
		if (args.asset_indices.empty()) { args.asset_indices.push_back(0u); }
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
		if (in.empty()) {
			std::fprintf(stderr, "Missing required argument: command (scene|mesh|list)\n");
			return false;
		}
		auto parser = Args::Parser{};

		auto spec = cli_args::Spec{};
		spec.app_name = "legsmi";
		spec.options = {
			cli_args::Opt{cli_args::Key{"verbose", 'v'}, {}, true, "verbose logging"},
			cli_args::Opt{cli_args::Key{"data-root"}, "/path/", false, "path to data root"},
			cli_args::Opt{cli_args::Key{"dest-dir"}, "uri/", false, "destination directory"},
		};
		spec.commands = {
			"mesh",
			"scene",
			"list",
		};
		spec.arguments = "[asset-index...]";

		switch (cli_args::parse(spec, &parser, in)) {
		case cli_args::Result::eExitSuccess: return true;
		case cli_args::Result::eExitFailure: return false;
		default: break;
		}

		args = std::move(parser.args);

		if (parser.command == "mesh") {
			args.command = Command::eMesh;
		} else if (parser.command == "scene") {
			args.command = Command::eScene;
		} else {
			args.command = Command::eList;
		}

		// if (!Args::Parser{in, args}.parse(in)) {
		// 	print_usage(true);
		// 	return false;
		// }
		if (!fs::is_regular_file(args.gltf)) {
			std::fprintf(stderr, "%s", fmt::format("Invalid file path: {}\n", args.gltf.generic_string()).c_str());
			return false;
		}
		if (fs::is_regular_file(args.data_root)) {
			std::fprintf(stderr, "%s", fmt::format("Invalid data root: {}\n", args.data_root.generic_string()).c_str());
			return false;
		}
		if (args.command != Command::eList && !fs::is_directory(args.data_root) && !fs::create_directories(args.data_root)) {
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

		switch (args.command) {
		case Command::eList: list_assets(); return true;
		case Command::eMesh: return import_mesh();
		case Command::eScene: return import_scene();
		}

		return true;
	}

	void list_assets() const {
		std::printf("== Asset List ==\n\n");
		list_scenes(import_list);
		std::printf("\n");
		list_meshes(import_list);
		std::printf("\n");
	}

	bool import_mesh() {
		for (auto const index : args.asset_indices) {
			auto make_importer = [this] { return import_list.asset_list.mesh_importer(args.data_root.generic_string(), gltf_filename, import_logger); };
			if (!import_asset(std::span{import_list.asset_list.meshes}, "Mesh", index, make_importer)) { return false; }
		}
		return true;
	}

	bool import_scene() {
		for (auto const index : args.asset_indices) {
			auto scene_uri = args.dest_dir / args.dest_dir;
			scene_uri += ".scene.json";

			auto make_importer = [this, uri = scene_uri.generic_string()] {
				return import_list.asset_list.scene_importer(args.data_root.generic_string(), args.dest_dir.generic_string(), std::move(uri), import_logger);
			};

			if (!import_asset(std::span{import_list.asset_list.scenes}, "Scene", index, make_importer)) { return false; }
		}
		return true;
	}

	template <typename T, typename F>
	bool import_asset(std::span<T> assets, std::string_view type, std::size_t index, F make_importer) {
		auto* asset = find_by_index(assets, index);
		if (!asset) {
			std::fprintf(stderr, "%s", fmt::format("Invalid {} index: {}\n", type, index).c_str());
			return false;
		}
		auto importer = make_importer();
		auto uri = importer.try_import(*asset);
		if (!uri) {
			std::fprintf(stderr, "%s", fmt::format("Failed to import {} [{}]\n", type, index).c_str());
			return false;
		}
		std::printf("%s", fmt::format("[{}] {} imported successfully\n", uri.value(), type).c_str());
		return true;
	}
};
} // namespace
} // namespace legsmi

int main(int argc, char** argv) {
	levk::logger::g_format = "[{level}] {message}";
	auto serializer = levk::Service<levk::Serializer>::Instance{};
	serializer.get().bind<levk::LitMaterial>();
	serializer.get().bind<levk::UnlitMaterial>();
	serializer.get().bind<levk::SkinnedMaterial>();
	serializer.get().bind<levk::MeshRenderer>();
	if (!legsmi::App{}.run({argv + 1, static_cast<std::size_t>(argc - 1)})) { return EXIT_FAILURE; }
}
