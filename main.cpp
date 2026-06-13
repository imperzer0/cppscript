#include <iostream>

#include "return_codes.h"
#include "lib.cpp"


void copy_argument(char* dest, const char* src, int len)
{
    dest = new char[len + 1];
    strncpy(dest, src, len);
    dest[len] = 0;
}


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

    char** args = new char*[argc]; // A copy of script's arguments

    for (int i = 1; i < argc; ++i) // Copy each argument
        copy_argument(args[i - 1], argv[i], strlen(argv[i]));

    args[argc - 1] = nullptr; // Last argument should be NULL

    std::string output = compile(args[0]); // Compile the script

    // Free initial string
    delete[] args[0];

    // Replace it with compiled binary name
    copy_argument(args[0], output.c_str(), output.length());

    // Run the binary
    run(args[0], args);

    // If the binary is bigger than 64MiB don't keep it in cache
    struct stat st{ };
    if (::stat(args[0], &st) == 0 && st.st_size >= 64 * 1024 * 1024)
        rm(args[0]); // Remove large files

    return ERROR_OK; // Exit Successfully
}
