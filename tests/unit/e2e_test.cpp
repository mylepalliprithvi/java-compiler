#include <doctest/doctest.h>

// This test compiles every tests/programs/*.java sample with our own
// pipeline, runs the resulting .class on a real JVM, and diffs stdout
// against the sibling *.expected file. It needs a `java` on PATH at CMake
// configure time (see JAVA_EXECUTABLE in CMakeLists.txt) — if none was
// found, it's compiled out entirely rather than failing.
#ifdef JAVA_EXECUTABLE

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "codegen/ClassFileWriter.hpp"
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "sema/SemanticAnalyzer.hpp"

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string runCommand(const std::string& cmd) {
    std::array<char, 4096> buffer{};
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    REQUIRE(pipe != nullptr);
    size_t n;
    while ((n = fread(buffer.data(), 1, buffer.size(), pipe)) > 0) {
        result.append(buffer.data(), n);
    }
    pclose(pipe);
    return result;
}

// Compiles a .java file with our own pipeline and writes <ClassName>.class
// into outDir. Returns the class name on success.
bool compileJavaFile(const std::filesystem::path& srcPath, const std::filesystem::path& outDir,
                     std::string& classNameOut, std::string& errorOut) {
    std::string source = readFile(srcPath);

    jc::Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (!lexer.errors().empty()) {
        errorOut = "lex error: " + lexer.errors().front().message;
        return false;
    }

    jc::Parser parser(std::move(tokens));
    auto unit = parser.parseCompilationUnit();
    if (!unit || !parser.errors().empty()) {
        errorOut = unit ? "parse error: " + parser.errors().front().message : "parse failed";
        return false;
    }

    jc::SemanticAnalyzer sema;
    if (!sema.analyze(*unit)) {
        errorOut = "sema error: " + sema.errors().front().message;
        return false;
    }

    std::vector<uint8_t> bytes = jc::generateClassFile(*unit, sema.classTable());
    classNameOut = unit->classDecl.name;

    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    std::ofstream out(outDir / (classNameOut + ".class"), std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return true;
}

}  // namespace

TEST_CASE("end-to-end: sample programs compile and run correctly on a real JVM") {
    std::filesystem::path programsDir = std::filesystem::path(PROJECT_SOURCE_DIR) / "tests" / "programs";
    REQUIRE(std::filesystem::exists(programsDir));

    for (const auto& entry : std::filesystem::directory_iterator(programsDir)) {
        if (entry.path().extension() != ".java") continue;
        std::string stem = entry.path().stem().string();
        std::filesystem::path expectedPath = programsDir / (stem + ".expected");
        if (!std::filesystem::exists(expectedPath)) continue;

        SUBCASE(stem.c_str()) {
            std::filesystem::path outDir =
                std::filesystem::temp_directory_path() / ("jc_e2e_" + stem);

            std::string className, error;
            bool ok = compileJavaFile(entry.path(), outDir, className, error);
            REQUIRE_MESSAGE(ok, error);

            std::string cmd = "\"" JAVA_EXECUTABLE "\" -cp \"" + outDir.string() + "\" " +
                               className + " 2>&1";
            std::string actual = runCommand(cmd);
            std::string expected = readFile(expectedPath);
            CHECK(actual == expected);
        }
    }
}

#endif  // JAVA_EXECUTABLE
