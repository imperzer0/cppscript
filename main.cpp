#include <iostream>

#include "return_codes.h"
#include "lib.cpp"


void copy_argument(char*& dest, const char* src, int len)
{
    dest = new char[len + 1];
    strncpy(dest, src, len);
    dest[len] = 0;
}


int main(int argc, char* argv[], char* envp[])
{
    if (argc < 2) // If we have arguments
    {
        std::cerr << "Usage: " << argv[0] << " <script> {<arguments>}" << std::endl;
        std::cerr << std::endl;
        std::cerr << "  Arguments are optional." << std::endl;
        std::cerr << std::endl;
        return 1;
    }

    Log::Set_LogLevel(MainConfig::Instance().get_log_level());


    int envp_size = 0;
    for (; envp[envp_size] != nullptr; ++envp_size) { }

    std::string output = compile(argv[1], {envp, envp + envp_size}); // Compile the script

    // Run the binary
    run(output, {argv + 1, argv + argc}, {envp, envp + envp_size});

    // If the binary is bigger than 64MiB don't keep it in cache
    struct stat st{ };
    if (::stat(output.c_str(), &st) == 0 && st.st_size >= 64 * 1024 * 1024)
        rm(output); // Remove large files

    return ERROR_OK; // Exit Successfully
}
