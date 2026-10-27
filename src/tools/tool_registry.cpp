#include "tools/tool_registry.h"

#include <cstdint>
#include <utility>

ToolRegistry::ToolRegistry() : buckets_(16) {}
ToolRegistry::~ToolRegistry() = default;

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    std::uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : key) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return static_cast<std::size_t>(h);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (!tool) {
        return false;
    }

    const std::string name = tool->name();
    if (find(name) != nullptr) {
        return false;
    }

    const double next_load = static_cast<double>(size_ + 1) /
                             static_cast<double>(buckets_.size());
    if (next_load > kMaxLoadFactor) {
        rehash(buckets_.size() * 2);
    }

    const std::size_t index = hash(name) % buckets_.size();
    buckets_[index].push_back(std::move(tool));
    ++size_;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    if (buckets_.empty()) {
        return nullptr;
    }

    const std::size_t index = hash(name) % buckets_.size();
    for (const auto& tool : buckets_[index]) {
        if (tool->name() == name) {
            return tool.get();
        }
    }
    return nullptr;
}

std::size_t ToolRegistry::size() const noexcept {
    return size_;
}

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const auto& bucket : buckets_) {
        for (const auto& tool : bucket) {
            fn(*tool);
        }
    }
}

std::size_t ToolRegistry::bucket_count() const noexcept {
    return buckets_.size();
}

double ToolRegistry::load_factor() const noexcept {
    if (buckets_.empty()) {
        return 0.0;
    }
    return static_cast<double>(size_) / static_cast<double>(buckets_.size());
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const auto& bucket : buckets_) {
        if (bucket.size() > longest) {
            longest = bucket.size();
        }
    }
    return longest;
}

void ToolRegistry::rehash(std::size_t new_bucket_count) {
    std::vector<Bucket> new_buckets(new_bucket_count);

    for (auto& bucket : buckets_) {
        for (auto& tool : bucket) {
            const std::size_t index = hash(tool->name()) % new_bucket_count;
            new_buckets[index].push_back(std::move(tool));
        }
    }

    buckets_ = std::move(new_buckets);
}
