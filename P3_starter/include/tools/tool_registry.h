// include/tools/tool_registry.h
//
// YOU IMPLEMENT (src/tools/tool_registry.cpp). Spec §4.3 has a function-by-
// function table of the exact behavior.
//
// Maps tool names to Tool objects using a HASH TABLE YOU WRITE YOURSELF.
// std::unordered_map, std::map, std::set, std::unordered_set (and any other
// associative container) are forbidden in this class. std::vector, raw arrays,
// and std::unique_ptr are fine.
//
// The PUBLIC interface below is fixed. The PRIVATE section is yours: choose
// separate chaining or open addressing, choose a hash function and a
// max-load-factor threshold, and defend all three in your design log.

#pragma once
#include "tools/tool.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

class ToolRegistry {
public:
    ToolRegistry();
    ~ToolRegistry();

    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;

    bool add(std::unique_ptr<Tool> tool);
    Tool* find(std::string_view name) const;
    std::size_t size() const noexcept;
    void for_each(const std::function<void(const Tool&)>& fn) const;

    std::size_t bucket_count() const noexcept;
    double load_factor() const noexcept;
    std::size_t max_probe_length() const;
    static std::size_t hash(std::string_view key) noexcept;

private:
    using Bucket = std::vector<std::unique_ptr<Tool>>;

    static constexpr double kMaxLoadFactor = 0.75;
    std::vector<Bucket> buckets_;
    std::size_t size_ = 0;

    void rehash(std::size_t new_bucket_count);
};
