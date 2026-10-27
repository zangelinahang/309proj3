#include "tools/workspace.h"

#include <exception>
#include <string>

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    reason->clear();

    if (user_path.empty()) {
        *reason = "empty path";
        return false;
    }

    if (user_path.find('\0') != std::string_view::npos) {
        *reason = "path contains NUL byte";
        return false;
    }

    try {
        fs::path p{std::string(user_path)};
        if (p.is_absolute() || p.has_root_path()) {
            *reason = "absolute paths are not allowed";
            return false;
        }

        fs::path normalized = p.lexically_normal();
        if (normalized.begin() != normalized.end() &&
            *normalized.begin() == fs::path("..")) {
            *reason = "path escapes workspace";
            return false;
        }

        if (normalized == fs::path(".")) {
            *out = root();
        } else {
            *out = root() / normalized;
        }
        return true;
    } catch (const std::exception& e) {
        *reason = std::string("invalid path: ") + e.what();
        return false;
    } catch (...) {
        *reason = "invalid path";
        return false;
    }
}
