#include <iostream>

#include "return_codes.h"
#include "lib.cpp"

int main(int argc, char** argv)
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

    char** args = new char*[argc];

    for (int i = 1; i < argc; ++i)
    {
        int len = strlen(argv[i]);
        args[i - 1] = new char[len + 1];
        strncpy(args[i - 1], argv[i], len);
        args[i - 1][len] = 0;
    }

    args[argc - 1] = nullptr;

    std::string output = compile(args[0]);

    delete[] args[0];
    args[0] = new char[output.length() + 1];
    strncpy(args[0], output.c_str(), output.length());
    args[0][output.length()] = '\0';

    run(args[0], args);

    struct stat st{ };
    if (::stat(args[0], &st) == 0 && st.st_size >= 32 * 1024 * 1024)
        rm(args[0]); // Remove large files

    return ERROR_OK;
}
