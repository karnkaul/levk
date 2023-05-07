#include <levk/asset/pcm_provider.hpp>
#include <filesystem>

namespace levk {
PcmProvider::Payload PcmProvider::load_payload(Uri<capo::Pcm> const& uri, Stopwatch const& stopwatch) const {
	auto bytes = data_source().read(uri);
	if (bytes.empty()) {
		m_logger.error("Failed to load audio data [{}]", uri.value());
		return {};
	}
	auto result = capo::Pcm::make(bytes.span(), capo::to_compression(std::filesystem::path{uri.value()}.extension().string()));
	if (!result) {
		m_logger.error("Failed to load audio data [{}]", uri.value());
		return {};
	}

	auto ret = Payload{};
	ret.asset = std::move(result.pcm);
	m_logger.info("[{:.3f}s] Pcm loaded [{}]", stopwatch().count(), uri.value());
	return ret;
}
} // namespace levk
