#include <fmt/format.h>
#include <legsmi/legsmi.hpp>
#include <levk/io/serializer.hpp>
#include <levk/scene/scene.hpp>
#include <levk/service.hpp>
#include <levk/util/logger.hpp>
#include <charconv>
#include <concepts>
#include <cstdio>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace legsmi {
namespace {

constexpr std::string_view usage_v{R"(Usage:
  legsmi <mesh|scene> [--data-root=/path/] [--verbose] [--dest-dir=uri/] <path.gltf> [asset_indices...]
  legsmi list <path.gltf>
)"};

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

struct Args::Parser {
	enum class Result { eUnused, eUsed, eError };

	std::span<char const* const> args;
	Args& out;

	bool at_end() const { return args.empty(); }
	std::string_view peek() const { return args.empty() ? std::string_view{} : args.front(); }

	std::string_view advance() {
		if (args.empty()) { return {}; }
		auto ret = args.front();
		args = args.subspan(1);
		return ret;
	}

	static bool unrecognized(std::string_view type, std::string_view arg) {
		std::fprintf(stderr, "%s", fmt::format("Unrecognized {}: {}\n", type, arg).c_str());
		return false;
	}

	bool get_value(std::string_view key, levk::Ptr<std::string_view> out_value = {}) {
		auto arg = peek();
		if (arg.starts_with(key)) {
			arg = arg.substr(key.size());
			if (arg.empty() || arg.front() != '=') { return false; }
			if (out_value) { *out_value = arg.substr(1); }
			advance();
			return true;
		}
		return false;
	}

	bool try_parse_data_root() {
		auto value = std::string_view{};
		if (get_value("--data-root", &value)) {
			out.data_root = value;
			return true;
		}
		return false;
	}

	bool parse_command() {
		auto const arg = advance();
		if (arg == "mesh") {
			out.command = Command::eMesh;
			return true;
		}
		if (arg == "scene") {
			out.command = Command::eScene;
			return true;
		}
		if (arg == "list") {
			out.command = Command::eList;
			return true;
		}
		unrecognized("command", arg);
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

	bool try_parse_verbose() {
		if (peek() == "--verbose") {
			out.verbose = true;
			advance();
			return true;
		}
		return false;
	}

	Result parse_option() {
		if (try_parse_dest_dir()) { return Result::eUsed; }
		if (try_parse_data_root()) { return Result::eUsed; }
		if (try_parse_verbose()) { return Result::eUsed; }
		return Result::eUnused;
	}

	bool try_parse_asset_index() {
		auto const value = peek();
		auto& asset_index = out.asset_indices.emplace_back();
		auto [_, ec] = std::from_chars(value.data(), value.data() + value.size(), asset_index);
		if (ec != std::errc{}) {
			std::fprintf(stderr, "%s", fmt::format("Invalid value for [index]: {}\n", value).c_str());
			return false;
		}
		advance();
		return true;
	}

	bool parse(std::span<char const* const> const in) {
		args = in;
		if (!parse_command()) { return false; }
		while (peek().starts_with("--")) {
			switch (parse_option()) {
			default:
			case Result::eError: return false;
			case Result::eUsed: break;
			case Result::eUnused: {
				unrecognized("option", peek());
				return false;
			}
			}
		}
		out.gltf = advance();
		if (out.gltf.empty()) {
			std::fprintf(stderr, "%s", fmt::format("Missing required argument: gltf-path\n").c_str());
			return false;
		}
		while (!at_end()) {
			if (!try_parse_asset_index()) { return false; }
		}
		if (out.command == Command::eList) { return peek().empty(); }
		if (out.asset_indices.empty()) { out.asset_indices.push_back(0u); }
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
