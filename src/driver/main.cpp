#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "ast/AstPrinter.hpp"
#include "codegen/ClassFileWriter.hpp"
#include "core/Version.hpp"
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "sema/SemanticAnalyzer.hpp"

namespace {

void printUsage() {
    std::cerr << "usage: compiler <File.java> [-o outdir] [--dump-tokens] [--dump-ast]\n";
}

std::string readFile(const std::string& path, bool& ok) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        ok = false;
        return "";
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    ok = true;
    return ss.str();
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string inputPath;
    std::string outDir = ".";
    bool dumpTokens = false;
    bool dumpAst = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-o") {
            if (i + 1 >= argc) {
                printUsage();
                return 1;
            }
            outDir = argv[++i];
        } else if (arg == "--dump-tokens") {
            dumpTokens = true;
        } else if (arg == "--dump-ast") {
            dumpAst = true;
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "compiler " << jc::version() << ": unknown option '" << arg << "'\n";
            printUsage();
            return 1;
        } else {
            inputPath = arg;
        }
    }

    if (inputPath.empty()) {
        printUsage();
        return 1;
    }

    bool readOk = false;
    std::string source = readFile(inputPath, readOk);
    if (!readOk) {
        std::cerr << "compiler: cannot read '" << inputPath << "'\n";
        return 1;
    }

    jc::Lexer lexer(source);
    std::vector<jc::Token> tokens = lexer.tokenize();

    if (dumpTokens) {
        for (const auto& t : tokens) {
            std::cout << t.line << ":" << t.col << " " << jc::tokenTypeName(t.type);
            if (!t.text.empty()) std::cout << " '" << t.text << "'";
            std::cout << "\n";
        }
    }

    if (!lexer.errors().empty()) {
        for (const auto& e : lexer.errors()) {
            std::cerr << inputPath << ":" << e.line << ":" << e.col << ": error: " << e.message
                       << "\n";
        }
        return 1;
    }

    jc::Parser parser(std::move(tokens));
    std::unique_ptr<jc::CompilationUnit> unit = parser.parseCompilationUnit();

    if (!parser.errors().empty()) {
        for (const auto& e : parser.errors()) {
            std::cerr << inputPath << ":" << e.line << ":" << e.col << ": error: " << e.message
                       << "\n";
        }
        return 1;
    }

    if (dumpAst) {
        std::cout << jc::printAst(*unit) << "\n";
    }

    jc::SemanticAnalyzer sema;
    bool semaOk = sema.analyze(*unit);
    if (!semaOk) {
        for (const auto& e : sema.errors()) {
            std::cerr << inputPath << ":" << e.line << ":" << e.col << ": error: " << e.message
                       << "\n";
        }
        return 1;
    }

    std::vector<uint8_t> classBytes = jc::generateClassFile(*unit, sema.classTable());

    std::filesystem::path outPath =
        std::filesystem::path(outDir) / (unit->classDecl.name + ".class");
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(outDir), ec);

    std::ofstream out(outPath, std::ios::binary);
    if (!out) {
        std::cerr << "compiler: cannot write '" << outPath.string() << "'\n";
        return 1;
    }
    out.write(reinterpret_cast<const char*>(classBytes.data()),
              static_cast<std::streamsize>(classBytes.size()));

    return 0;
}
