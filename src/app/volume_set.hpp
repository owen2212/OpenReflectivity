#ifndef VOLUME_SET_HPP
#define VOLUME_SET_HPP

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/products.hpp"
#include "app/radar_render_data.hpp"
#include "app/volume_loader.hpp"

// one Level 2 archive in a time-ordered set. The active entry's data is
// checked out into AppState and swapped back on deactivation, no deep copies.
struct VolumeEntry {
    std::string path;
    uint64_t time_key = 0;          // YYYYMMDDHHMMSS, 0 if unparsable
    bool requested = false;         // decode in flight
    bool failed = false;            // decode failed; never retried
    std::unique_ptr<LoadedVolume> data;
    std::array<std::vector<std::optional<ScanGpuData>>, kProductCount> gpu_cache;
    std::array<std::vector<std::optional<ScanPolarTexture>>, kProductCount> polar_cache;
    uint64_t last_used = 0;
};

// sortable key from standard archive names (SSSSYYYYMMDD_HHMMSS...)
uint64_t parse_volume_time_key(const std::string &filename);

class VolumeSet {
    public:
        // single archive or every archive in a directory, sorted by
        // timestamp. False if nothing loadable found.
        bool open(const std::string &file_or_dir);

        int size() const { return static_cast<int>(entries_.size()); }
        bool empty() const { return entries_.empty(); }
        VolumeEntry& entry(int i) { return entries_[static_cast<size_t>(i)]; }
        const VolumeEntry& entry(int i) const { return entries_[static_cast<size_t>(i)]; }

        int active() const { return active_; }
        void set_active(int i);     // bookkeeping only; data swap is the caller's

        bool decoded(int i) const;  // resident or checked out as active

        // keeps the loader fed: want_idx first, then prefetch the next
        // volume (wrapping) so time playback has no gaps
        void pump(VolumeLoader &loader, const std::string &site_fallback, int want_idx);

        // false if the result matches no entry (stale, set was replaced)
        bool store_result(LoadedVolume &&vol);

        void fail_request(const std::string &path);

        // free decoded volumes beyond the LRU budget, leaving the active
        // entry and its neighbors alone
        void evict(size_t max_decoded = 4);

        // "HH:MM:SS" from the time key, or the basename as fallback
        std::string time_label(int i) const;

    private:
        std::vector<VolumeEntry> entries_;
        int active_ = -1;
        uint64_t use_counter_ = 0;
};

#endif
