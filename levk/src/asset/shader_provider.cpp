#include <levk/asset/shader_provider.hpp>
#include <levk/util/error.hpp>
#include <levk/util/hash_combine.hpp>
#include <levk/util/logger.hpp>
#include <algorithm>
#include <atomic>
#include <filesystem>

namespace levk {
namespace fs = std::filesystem;

namespace {
namespace compiler {
constexpr std::string_view glsl_extensions_v[] = {".vert", ".frag"};
constexpr std::string_view spir_v_compiler_v = "glslc";
constexpr std::string_view dev_null_v =
#if defined(_WIN32)
	"NUL";
#else
	"/dev/null";
#endif

bool is_glsl(Uri<ShaderCode> const& uri) {
	auto extension = fs::path{uri.value()}.extension();
	return std::find(std::begin(glsl_extensions_v), std::end(glsl_extensions_v), extension.generic_string()) != std::end(glsl_extensions_v);
}

bool try_compiler() {
	auto const cmd = fmt::format("{} --version > {}", spir_v_compiler_v, dev_null_v);
	return std::system(cmd.c_str()) == 0;
}

bool is_available() {
	static std::atomic<bool> s_ret{};
	if (s_ret) { return s_ret; }
	return s_ret = try_compiler();
}

std::string to_spir_v(std::string_view glsl_path) { return fmt::format("{}.spv", glsl_path); }

bool try_compile(std::string& out_error, char const* glsl_path, char const* spir_v_path, bool debug) {
	if (!is_available()) {
		out_error = "Shader compiler not available";
		return false;
	}
	if (!glsl_path || !*glsl_path) {
		out_error = "GLSL path is empty";
		return false;
	}
	if (!fs::is_regular_file(glsl_path)) {
		out_error = fmt::format("File doesn't exist [{}]", glsl_path);
		return false;
	}
	auto const dst = spir_v_path && *spir_v_path ? std::string{spir_v_path} : to_spir_v(glsl_path);
	char const* debug_str = debug ? " -g " : "";
	auto const cmd = fmt::format("{} {} {} -o {}", spir_v_compiler_v, debug_str, glsl_path, dst);
	if (std::system(cmd.c_str()) == 0) { return true; }
	out_error = fmt::format("Failed to compile [{}]", glsl_path);
	return false;
}

bool compile(Uri<> const& glsl, Uri<> const& spir_v, std::string_view prefix, bool debug) {
	static auto const s_logger{Logger{"ShaderCompiler"}};
	auto error = std::string{};
	if (!try_compile(error, glsl.absolute_path(prefix).c_str(), spir_v.absolute_path(prefix).c_str(), debug)) {
		s_logger.error("{}", error);
		return false;
	}
	s_logger.info("Compiled GLSL [{}] to SPIR-V [{}] successfully", glsl.value(), spir_v.value());
	return true;
}

struct Data {
	Uri<> glsl{};
	Uri<> spir_v{};

	static Data make(Uri<> const& uri) {
		auto ret = Data{};
		if (is_glsl(uri)) {
			ret.glsl = uri;
			ret.spir_v = to_spir_v(ret.glsl.value());
		} else {
			ret.spir_v = uri;
		}
		return ret;
	}
};
} // namespace compiler
} // namespace

ShaderProvider::Payload ShaderProvider::load_payload(Uri<ShaderCode> const& uri, Stopwatch const& stopwatch) const {
	auto data = compiler::Data::make(uri);
	auto ret = Payload{};
	if (data.glsl && compiler::is_available()) {
		ret.dependencies.push_back(data.glsl);
		compiler::compile(data.glsl, data.spir_v, data_source().mount_point(), false);
	}
	auto bytes = data_source().read(data.spir_v);
	if (bytes.empty()) {
		m_logger.error("Failed to load SPIR-V [{}]", data.spir_v.value());
		return {};
	}
	auto const hash_code = [](std::span<std::uint32_t const> code) {
		auto ret = std::size_t{};
		for (auto const c : code) { hash_combine(ret, c); }
		return ret;
	};
	if (bytes.size() % sizeof(std::uint32_t) != 0) { return {}; }
	ret.asset.emplace();
	ret.asset->spir_v = DynArray<std::uint32_t>{bytes.size() / sizeof(std::uint32_t)};
	std::memcpy(ret.asset->spir_v.data(), bytes.data(), bytes.size());
	ret.asset->hash = hash_code(ret.asset->spir_v.span());
	m_logger.info("[{:.3f}s] Shader loaded [{}]", stopwatch().count(), uri.value());
	return ret;
}
} // namespace levk
