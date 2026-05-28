#ifndef VOLUME_LOADER_HPP
#define VOLUME_LOADER_HPP

#include <array>
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "app/products.hpp"
#include "rsl/rsl_wrapper.hpp"

struct LoadedVolume {
    std::string path;
    rsl::SiteInfo site;
    std::array<rsl::Product, kProductCount> products;
    std::string error;  // nonempty on failure
};

// decodes Level 2 archives on a single background worker. RSL keeps global
// state so decoding has to stay serialized through this one thread. Don't
// decode on the main thread or add more workers.
class VolumeLoader {
    public:
        VolumeLoader() = default;
        ~VolumeLoader();

        VolumeLoader(const VolumeLoader&) = delete;
        VolumeLoader& operator=(const VolumeLoader&) = delete;

        bool busy() const { return busy_.load(); }

        // starts a decode, false if one is already running
        bool request(const std::string &path, const std::string &site_id);

        // non-blocking, yields the finished volume exactly once
        std::optional<LoadedVolume> poll();

    private:
        std::thread worker_;
        mutable std::mutex mutex_;
        std::optional<LoadedVolume> result_;
        std::atomic<bool> busy_{false};
};

// 4-letter site id from a standard archive filename (KTLX20130520_...)
std::string infer_site_id(const std::string &path, const std::string &fallback);

#endif
