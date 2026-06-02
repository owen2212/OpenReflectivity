#include "volume_set.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>

uint64_t parse_volume_time_key(const std::string &filename) {
    // SSSSYYYYMMDD_HHMMSS... -> YYYYMMDDHHMMSS
    if (filename.size() < 19 || filename[12] != '_') return 0;
    for (size_t i = 0; i < 4; ++i) {
        if (!std::isalpha(static_cast<unsigned char>(filename[i]))) return 0;
    }
    uint64_t key = 0;
    for (size_t i = 4; i < 12; ++i) {
        const char c = filename[i];
        if (c < '0' || c > '9') return 0;
        key = key * 10 + static_cast<uint64_t>(c - '0');
    }
    for (size_t i = 13; i < 19; ++i) {
        const char c = filename[i];
        if (c < '0' || c > '9') return 0;
        key = key * 10 + static_cast<uint64_t>(c - '0');
    }
    return key;
}

bool VolumeSet::open(const std::string &file_or_dir) {
    std::vector<VolumeEntry> entries;
    std::error_code ec;
    if (std::filesystem::is_directory(file_or_dir, ec)) {
        for (const auto &de : std::filesystem::directory_iterator(file_or_dir, ec)) {
            if (!de.is_regular_file()) continue;
            const std::string name = de.path().filename().string();
            if (name.empty() || name[0] == '.') continue;
            VolumeEntry e;
            e.path = de.path().string();
            e.time_key = parse_volume_time_key(name);
            entries.push_back(std::move(e));
        }
        std::sort(entries.begin(), entries.end(), [](const VolumeEntry &a, const VolumeEntry &b) {
            if (a.time_key != b.time_key) return a.time_key < b.time_key;
            return a.path < b.path;
        });
    } else if (std::filesystem::exists(file_or_dir, ec)) {
        VolumeEntry e;
        e.path = file_or_dir;
        e.time_key = parse_volume_time_key(
            std::filesystem::path(file_or_dir).filename().string());
        entries.push_back(std::move(e));
    }
    if (entries.empty()) return false;

    entries_ = std::move(entries);
    active_ = -1;
    use_counter_ = 0;
    return true;
}

void VolumeSet::set_active(int i) {
    active_ = i;
    if (i >= 0 && i < size()) {
        entries_[static_cast<size_t>(i)].last_used = ++use_counter_;
    }
}

bool VolumeSet::decoded(int i) const {
    if (i < 0 || i >= size()) return false;
    return i == active_ || entries_[static_cast<size_t>(i)].data != nullptr;
}

void VolumeSet::pump(VolumeLoader &loader, const std::string &site_fallback, int want_idx) {
    if (entries_.empty() || loader.busy()) return;

    auto needs_decode = [this](int i) {
        return i >= 0 && i < size() && i != active_ &&
               !entries_[static_cast<size_t>(i)].data &&
               !entries_[static_cast<size_t>(i)].requested &&
               !entries_[static_cast<size_t>(i)].failed;
    };
    int target = -1;
    if (needs_decode(want_idx)) {
        target = want_idx;
    } else if (size() > 1) {
        const int next = (std::max(want_idx, 0) + 1) % size();
        if (needs_decode(next)) target = next;
    }
    if (target < 0) return;

    VolumeEntry &e = entries_[static_cast<size_t>(target)];
    if (loader.request(e.path, infer_site_id(e.path, site_fallback))) {
        e.requested = true;
    }
}

bool VolumeSet::store_result(LoadedVolume &&vol) {
    for (VolumeEntry &e : entries_) {
        if (e.path == vol.path) {
            e.requested = false;
            e.data = std::make_unique<LoadedVolume>(std::move(vol));
            return true;
        }
    }
    return false;
}

void VolumeSet::fail_request(const std::string &path) {
    for (VolumeEntry &e : entries_) {
        if (e.path == path) {
            e.requested = false;
            e.failed = true;
            return;
        }
    }
}

void VolumeSet::evict(size_t max_decoded) {
    const int n = size();
    if (n == 0) return;

    auto protect = [this, n](int i) {
        if (active_ < 0) return false;
        const int prev = (active_ + n - 1) % n;
        const int next = (active_ + 1) % n;
        return i == active_ || i == prev || i == next;
    };

    std::vector<int> evictable;
    for (int i = 0; i < n; ++i) {
        if (entries_[static_cast<size_t>(i)].data && !protect(i)) {
            evictable.push_back(i);
        }
    }
    if (evictable.size() <= max_decoded) return;
    std::sort(evictable.begin(), evictable.end(), [this](int a, int b) {
        return entries_[static_cast<size_t>(a)].last_used <
               entries_[static_cast<size_t>(b)].last_used;
    });
    const size_t to_evict = evictable.size() - max_decoded;
    for (size_t k = 0; k < to_evict; ++k) {
        VolumeEntry &e = entries_[static_cast<size_t>(evictable[k])];
        e.data.reset();
        for (size_t p = 0; p < kProductCount; ++p) {
            e.gpu_cache[p].clear();
            e.polar_cache[p].clear();
        }
    }
}

std::string VolumeSet::time_label(int i) const {
    if (i < 0 || i >= size()) return "";
    const VolumeEntry &e = entries_[static_cast<size_t>(i)];
    if (e.time_key > 0) {
        const uint64_t hms = e.time_key % 1000000ull;
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02llu:%02llu:%02llu",
                      static_cast<unsigned long long>(hms / 10000),
                      static_cast<unsigned long long>((hms / 100) % 100),
                      static_cast<unsigned long long>(hms % 100));
        return buf;
    }
    return std::filesystem::path(e.path).filename().string();
}
