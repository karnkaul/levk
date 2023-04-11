#include <levk/asset/common.hpp>
#include <levk/graphics/shape.hpp>
#include <levk/imcpp/reflector.hpp>

namespace levk {
bool Shape::serialize(dj::Json& out) const {
	asset::to_json(out["rgb"], rgb);
	asset::to_json(out["origin"], origin);
	return true;
}

bool Shape::deserialize(dj::Json const& json) {
	asset::from_json(json["rgb"], rgb);
	asset::from_json(json["origin"], origin);
	return true;
}

void Shape::inspect(imcpp::OpenWindow w) {
	auto const reflector = imcpp::Reflector{w};
	reflector(rgb, {false});
	reflector("Origin", origin, 0.25f);
}
} // namespace levk
