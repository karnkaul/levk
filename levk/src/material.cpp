#include <levk/material.hpp>

namespace levk {
void UnlitMaterial::write_sets(Shader& shader, TextureFallback const& fallback) {
	shader.update(1, 0, fallback.white.or_self(texture));
	shader.write(2, 0, Rgba::to_srgb(tint.to_vec4()));
}
} // namespace levk
