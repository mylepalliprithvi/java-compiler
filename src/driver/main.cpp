
#include <iostream>

#include "core/Version.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: compiler <File.java> [-o outdir]\n";
        return 1;
    }
    std::cerr << "compiler " << jc::version() << ": pipeline not implemented yet\n";
    return 1;

}
