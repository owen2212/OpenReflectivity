#include "products.hpp"

#include <GLFW/glfw3.h>

const std::array<ProductDescriptor, kProductCount>& product_table() {
    static const std::array<ProductDescriptor, kProductCount> table = {{
        {rsl::ProductType::REFLECTIVITY, "reflectivity", "ref", nullptr,
         "Reflectivity", GLFW_KEY_1},
        {rsl::ProductType::VELOCITY, "velocity", "vel", nullptr,
         "Velocity", GLFW_KEY_2},
        {rsl::ProductType::SPECTRAL_WIDTH, "spectrum_width", "sw", "spectral_width",
         "Spectrum Width", GLFW_KEY_3},
    }};
    return table;
}

size_t product_index(rsl::ProductType pt) {
    return static_cast<size_t>(pt);
}

const char* product_name(rsl::ProductType pt) {
    for (const ProductDescriptor &d : product_table()) {
        if (d.type == pt) return d.cli_name;
    }
    return "unknown";
}

const char* product_label(rsl::ProductType pt) {
    for (const ProductDescriptor &d : product_table()) {
        if (d.type == pt) return d.label;
    }
    return "Unknown";
}

bool parse_product(const std::string &name, rsl::ProductType &out) {
    for (const ProductDescriptor &d : product_table()) {
        if (name == d.cli_name ||
            (d.alias && name == d.alias) ||
            (d.alias2 && name == d.alias2)) {
            out = d.type;
            return true;
        }
    }
    return false;
}
