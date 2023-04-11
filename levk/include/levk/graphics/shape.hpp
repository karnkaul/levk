#pragma once
#include <levk/graphics/geometry.hpp>
#include <levk/graphics/rgba.hpp>
#include <levk/imcpp/common.hpp>
#include <levk/io/serializable.hpp>

namespace levk {
struct Shape : Serializable {
	bool serialize(dj::Json& out) const override;
	bool deserialize(dj::Json const& json) override;
	virtual Geometry make_geometry() const = 0;
	virtual void inspect(imcpp::OpenWindow w);

	Rgba rgb{white_v};
	glm::vec3 origin{};
};
} // namespace levk
