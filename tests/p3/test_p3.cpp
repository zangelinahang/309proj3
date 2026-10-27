#include "../mini_test.h"
#include "../test_helpers.h"

#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue schema_;
};

class CountingTool : public Tool {
public:
    explicit CountingTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    ~CountingTool() override { ++destroyed; }
    std::string name() const override { return n_; }
    std::string description() const override { return "counting tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
    static int destroyed;
private:
    std::string n_;
    JsonValue schema_;
};

int CountingTool::destroyed = 0;

bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

std::size_t weak_hash(std::string_view s) {
    if (s.empty()) return 0;
    return static_cast<unsigned char>(s[0]) + s.size();
}

}  // namespace

TEST(reg_empty_table) {
    ToolRegistry r;
    CHECK(r.size() == 0);
    CHECK(r.bucket_count() >= 1);
    CHECK(r.bucket_count() <= 64);
    CHECK(r.load_factor() == 0.0);
    CHECK(r.max_probe_length() == 0);
    CHECK(r.find("missing") == nullptr);
}

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(reg_null_rejected) {
    ToolRegistry r;
    std::unique_ptr<Tool> empty;
    CHECK(!r.add(std::move(empty)));
    CHECK(r.size() == 0);
}

TEST(reg_duplicate_new_tool_destroyed) {
    CountingTool::destroyed = 0;
    {
        ToolRegistry r;
        REQUIRE(r.add(std::make_unique<CountingTool>("same")));
        CHECK(!r.add(std::make_unique<CountingTool>("same")));
        CHECK(CountingTool::destroyed == 1);
    }
    CHECK(CountingTool::destroyed == 2);
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("alpha")));
    REQUIRE(r.add(std::make_unique<NamedTool>("beta")));
    REQUIRE(r.add(std::make_unique<NamedTool>("gamma")));

    int alpha = 0;
    int beta = 0;
    int gamma = 0;
    int total = 0;
    r.for_each([&](const Tool& tool) {
        ++total;
        if (tool.name() == "alpha") ++alpha;
        if (tool.name() == "beta") ++beta;
        if (tool.name() == "gamma") ++gamma;
    });

    CHECK(total == 3);
    CHECK(alpha == 1);
    CHECK(beta == 1);
    CHECK(gamma == 1);
}

TEST(reg_hash_uses_order) {
    CHECK(ToolRegistry::hash("ab") != ToolRegistry::hash("ba"));
    CHECK(ToolRegistry::hash("tool_1234") != ToolRegistry::hash("tool_1243"));
}

TEST(reg_10000_tools) {
    ToolRegistry r;
    Tool* first = nullptr;

    for (int i = 0; i < 10000; ++i) {
        const std::string name = "tool_" + std::to_string(i);
        auto tool = std::make_unique<NamedTool>(name);
        Tool* raw = tool.get();
        REQUIRE(r.add(std::move(tool)));
        if (i == 0) first = raw;
        CHECK(r.load_factor() <= 1.0);
    }

    for (int i = 0; i < 10000; ++i) {
        const std::string name = "tool_" + std::to_string(i);
        CHECK(r.find(name) != nullptr);
    }

    CHECK(r.find("tool_0") == first);
    CHECK(r.size() == 10000);
    CHECK(r.max_probe_length() <= 64);

    std::printf("registry metrics: buckets=%zu load=%.6f max_probe=%zu\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length());
}

TEST(reg_alternative_hash) {
    ToolRegistry r;
    for (int i = 0; i < 10000; ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>("tool_" + std::to_string(i))));
    }

    std::vector<int> chains(r.bucket_count(), 0);
    for (int i = 0; i < 10000; ++i) {
        const std::string name = "tool_" + std::to_string(i);
        ++chains[weak_hash(name) % chains.size()];
    }

    int weak_longest = 0;
    for (int count : chains) {
        if (count > weak_longest) weak_longest = count;
    }

    CHECK(weak_longest > static_cast<int>(r.max_probe_length()));
    std::printf("hash comparison: FNV max_chain=%zu weak_hash max_chain=%d\n",
                r.max_probe_length(), weak_longest);
}

TEST(ws_accepts_normal_paths) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;

    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));
    CHECK(out == ws.root() / "notes" / "todo.txt");

    REQUIRE(ws.resolve("./notes//todo.txt", &out, &reason));
    CHECK(out == ws.root() / "notes" / "todo.txt");

    REQUIRE(ws.resolve("new/dir/file.txt", &out, &reason));
    CHECK(out == ws.root() / "new" / "dir" / "file.txt");

    REQUIRE(ws.resolve("my..notes.txt", &out, &reason));
    CHECK(out == ws.root() / "my..notes.txt");
}

TEST(ws_normalizes_dotdot_inside_root) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;

    REQUIRE(ws.resolve("a/../b", &out, &reason));
    CHECK(out == ws.root() / "b");

    REQUIRE(ws.resolve("a/..", &out, &reason));
    CHECK(out == ws.root());

    REQUIRE(ws.resolve(".", &out, &reason));
    CHECK(out == ws.root());
}

TEST(ws_rejects_empty) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    CHECK(!ws.resolve("", &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_rejects_nul) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    const std::string with_nul = std::string("notes") + '\0' + "/x";
    CHECK(!ws.resolve(std::string_view(with_nul.data(), with_nul.size()), &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_rejects_absolute) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    CHECK(!ws.resolve("/etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;

    CHECK(!ws.resolve("..", &out, &reason));
    CHECK(!reason.empty());
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
    CHECK(!ws.resolve("a/../../b", &out, &reason));
    CHECK(!reason.empty());
    CHECK(!ws.resolve("notes/../../ws/notes/todo.txt", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_left_to_right) {
    double v = 0;
    std::string err;
    REQUIRE(eval("10 - 4 - 3", &v, &err));
    CHECK(v == 3);
    REQUIRE(eval("100 / 10 / 5", &v, &err));
    CHECK(v == 2);
}

TEST(calc_parentheses) {
    double v = 0;
    std::string err;
    REQUIRE(eval("(2 + 3) * 4", &v, &err));
    CHECK(v == 20);
    REQUIRE(eval("2 * (3 + 4) - 5", &v, &err));
    CHECK(v == 9);
}

TEST(calc_negative_numbers) {
    double v = 0;
    std::string err;
    REQUIRE(eval("-3 * -3", &v, &err));
    CHECK(v == 9);
}

TEST(calc_decimal_division) {
    double v = 0;
    std::string err;
    REQUIRE(eval("7 / 2", &v, &err));
    CHECK(v == 3.5);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / (2 - 2)", &v, &err));
    CHECK(err == "division by zero");
}

TEST(calc_additional_expressions) {
    double v = 0;
    std::string err;
    REQUIRE(eval("3.5 + 2.5 * 2", &v, &err));
    CHECK(v == 8.5);
    REQUIRE(eval("(8 - 3) * (2 + 4) / 5", &v, &err));
    CHECK(v == 6);
}

TEST(calc_long_expression) {
    std::string expr = "1";
    for (int i = 0; i < 20000; ++i) expr += "+1";
    double v = 0;
    std::string err;
    REQUIRE(eval(expr, &v, &err));
    CHECK(v == 20001);
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
