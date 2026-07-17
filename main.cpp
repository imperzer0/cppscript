#include <iostream>

#include "return_codes.h"
#include "lib.cpp"


int main(int argc, char* argv[], char* envp[])
{
    if (argc < 2)
    {
        // Print help message
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

    // Perform automatic cache cleaning
    cache_autoclean(output);

    return ERROR_OK; // Exit Successfully
}
