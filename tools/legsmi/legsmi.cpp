#include <fmt/format.h>
#include <legsmi/legsmi.hpp>
#include <levk/graphics/material.hpp>
#include <levk/io/serializer.hpp>
#include <levk/level/level.hpp>
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
	bool force{};
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
		case 'f': args.force = true; return true;
		}

		if (key.full == "data-root") {
			args.data_root = value;
		} else if (key.full == "dest-dir") {
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
			auto& asset_index = args.asset_indices.emplace_back();
			if (!cli_args::as(asset_index, arg)) {
				std::fprintf(stderr, "%s", fmt::format("invalid index, must be integral: {}\n", arg).c_str());
				return false;
			}
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
	levk::Service<levk::Serializer>::Instance serializer{};
	Args args{};
	fs::path src_dir{};
	ImportList import_list{};
	ImportMap imported{};

	bool can_import_scene() const { return !import_list.asset_list.has_skinned_mesh; }
	Logger import_logger{};

	bool run(std::span<char const* const> in) {
		if (in.empty()) {
			std::fprintf(stderr, "Missing required argument: command (scene|mesh|list)\n");
			return false;
		}

		serializer.get().bind<levk::LitMaterial>();
		serializer.get().bind<levk::UnlitMaterial>();
		serializer.get().bind<levk::SkinnedMaterial>();

		auto parser = Args::Parser{};

		auto spec = cli_args::Spec{};
		spec.app_name = "legsmi";
		spec.options = {
			cli_args::Opt{cli_args::Key{"force", 'f'}, {}, true, "force reimport existing assets"},
			cli_args::Opt{cli_args::Key{"data-root"}, "/path/", false, "path to data root"},
			cli_args::Opt{cli_args::Key{"dest-dir"}, "uri/", false, "destination directory"},
			cli_args::Opt{cli_args::Key{"verbose", 'v'}, {}, true, "verbose logging"},
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

		if (!fs::is_regular_file(args.gltf)) {
			std::fprintf(stderr, "%s", fmt::format("Invalid file path: {}\n", args.gltf.generic_string()).c_str());
			return false;
		}
		if (fs::is_regular_file(args.data_root)) {
			std::fprintf(stderr, "%s", fmt::format("Invalid data root: {}\n", args.data_root.generic_string()).c_str());
			return false;
		}
		if (!args.data_root.empty() && args.command != Command::eList && !fs::is_directory(args.data_root) && !fs::create_directories(args.data_root)) {
			std::fprintf(stderr, "%s", fmt::format("Failed to locate/create data root: {}\n", args.gltf.generic_string()).c_str());
			return false;
		}

		import_list = ImportList::make(peek_assets(args.gltf.string(), &serializer.get()));
		if (import_list.asset_list.scenes.empty() && import_list.asset_list.meshes.empty()) {
			std::fprintf(stderr, "%s", fmt::format("Could not find any assets in gltf file: {}\n", args.gltf.generic_string()).c_str());
			return false;
		}

		if (args.command == Command::eScene && !can_import_scene()) {
			std::fprintf(stderr, "Importing scenes with skinned meshes is not supported\n");
			return false;
		}

		if (!args.verbose) {
			for (auto& silenced : import_logger.silent.t) { silenced = true; }
		}
		src_dir = args.gltf.parent_path();
		if (args.data_root.empty()) { args.data_root = fs::current_path(); }
		if (args.dest_dir.empty()) {
			args.dest_dir = import_list.asset_list.default_dir_uri;
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
		auto make_importer = [this] {
			return import_list.asset_list.mesh_importer(args.data_root.generic_string(), args.dest_dir.generic_string(), import_logger, args.force);
		};
		for (auto const index : args.asset_indices) {
			if (!import_asset(std::span{import_list.asset_list.meshes}, "Mesh", index, make_importer)) { return false; }
		}
		return true;
	}

	bool import_scene() {
		for (auto const index : args.asset_indices) {
			auto make_importer = [this, uri = import_list.asset_list.make_default_level_uri(index)] {
				return import_list.asset_list.scene_importer(args.data_root.generic_string(), args.dest_dir.generic_string(), uri, import_logger, args.force);
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
		auto uri = importer.try_import(*asset, imported);
		if (!uri) {
			std::fprintf(stderr, "%s", fmt::format("Failed to import {} [{}]\n", type, index).c_str());
			return false;
		}
		std::printf("%s", fmt::format("[{}] {} imported successfully\n", (args.data_root / uri.value()).generic_string(), type).c_str());
		return true;
	}
};
} // namespace
} // namespace legsmi

int main(int argc, char** argv) {
	levk::Logger::s_format = "[{level}] [{context}] {message}";
	if (!legsmi::App{}.run({argv + 1, static_cast<std::size_t>(argc - 1)})) { return EXIT_FAILURE; }
}
