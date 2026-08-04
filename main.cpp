#include <iostream>

#include "return_codes.h"
#include "lib.cpp"

void print_help(const char* appname)
{
    // Print help message
    std::cerr << "Usage: " << appname << " <script> {<arguments>}" << std::endl;
    std::cerr << std::endl;
    std::cerr << "  Arguments are optional." << std::endl;
    std::cerr << std::endl;
}

int main(int argc, char* argv[], char* envp[])
{
    if (argc < 2)
    {
        // Try running cling if it's installed.
        if (is_available_in_path("cling"))
            execvpe("cling", std::vector{const_cast<char*>("cling")}.data(), envp);

        print_help(argv[0]);
        return 1;
    }

    // Print help on --help or -h
    if (argc == 2 && (!strcmp(argv[2], "--help") || !strcmp(argv[2], "-h")))
    {
        print_help(argv[0]);
        return 1;
    }

    Log::Set_LogLevel(MainConfig::Instance().get_log_level());


    int envp_size = 0;
    for (; envp[envp_size] != nullptr; ++envp_size) { }

    std::string output = compile(argv[1], {envp, envp + envp_size}); // Compile the script

    // Run the binary
    run(output, {argv + 1, argv + argc}, {envp, envp + envp_size});

    // Perform automatic cache cleaning
    cache_autoclean(output);

    return ERROR_OK; // Exit Successfully
}
