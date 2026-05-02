#ifndef PRODUCTS_HPP
#define PRODUCTS_HPP

#include <array>
#include <cstddef>
#include <string>

#include "rsl/rsl_wrapper.hpp"

constexpr size_t kProductCount = static_cast<size_t>(rsl::ProductType::COUNT);

// one row per product. Adding a moment means an enum value in
// rsl::ProductType, a row here, a config + stop table in
// radar_render_data.cpp, and a volume index in rsl_wrapper.cpp.
struct ProductDescriptor {
    rsl::ProductType type;
    const char *cli_name;   // e.g. "reflectivity"
    const char *alias;      // e.g. "ref" (nullptr if none)
    const char *alias2;     // e.g. "spectral_width" (nullptr if none)
    const char *label;      // e.g. "Reflectivity"
    int hotkey;             // GLFW_KEY_*
};

const std::array<ProductDescriptor, kProductCount>& product_table();

size_t product_index(rsl::ProductType pt);
const char* product_name(rsl::ProductType pt);
const char* product_label(rsl::ProductType pt);
bool parse_product(const std::string &name, rsl::ProductType &out);

#endif
