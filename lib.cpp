//
// Created by jim on 12 Jun 2026.
//


#ifndef CPPSCRIPT_LIB_CPP
#define CPPSCRIPT_LIB_CPP


#include <complex>
#include <sstream>
#include <sys/stat.h>

#include "config.hpp"
#include "wrappers.cpp"

#include <xxhash.h>

#include "constants.h"
#include "Log.hpp"
#define XXH_STATIC_LINKING_ONLY


bool is_available_in_path(const std::string& executable)
{
    const char* pathenv = std::getenv("PATH");
    if (!pathenv)
        return false;

    std::string path_str(pathenv);
    std::stringstream ss(path_str);
    std::string directory;

    char delim = ':';

    while (std::getline(ss, directory, delim))
    {
        if (directory.empty())
            continue; // Skip "" directories

        std::string executable_path(realpath(directory.c_str()));
        executable_path += "/";
        executable_path += executable;

        std::string potential_path(realpath(executable_path.c_str()));

        struct stat st;
        if (!::stat(potential_path.c_str(), &st) &&
            st.st_mode & S_IFREG && st.st_mode & S_IEXEC) // Is it a regular executable file
            return true;
    }

    return false;
}

std::string get_best_available_ld()
{
    constexpr const char* Better_LDs[]{"mold", "lld"}; // Ordered from the best to the worst
    for (const char* Better_LD : Better_LDs)
    {
        if (is_available_in_path(Better_LD))
            return Better_LD;
    }

    return ""; // Fall back to gcc's ld
}

struct hash
{
    uint64_t val;
    int error; // 0-Success   1-Error
};

hash get_file_hash(const std::string& path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open())
    {
        WARN << "Could not open file " << path << Endl;
        return hash{.error = 1};
    }

    XXH3_state_t* XXH3 = XXH3_createState();
    if (XXH3 == nullptr)
    {
        WARN << "Could not initialize XXH3 state." << Endl;
        return hash{.error = 1};
    }

    if (XXH3_64bits_reset(XXH3) == XXH_ERROR)
    {
        WARN << "Could not reset XXH3." << Endl;
        XXH3_freeState(XXH3);
        return hash{.error = 1};
    }

    constexpr size_t BUFFER_SIZE = 64 * 1024; // 64KB
    std::vector<char> buffer(BUFFER_SIZE);

    while (ifs.read(buffer.data(), BUFFER_SIZE) || ifs.gcount() > 0)
    {
        if (XXH3_64bits_update(XXH3, buffer.data(), ifs.gcount()) == XXH_ERROR)
        {
            XXH3_freeState(XXH3);
            return hash{.error = 1};
        }
    }

    hash sum{ };
    sum.error = 0;
    sum.val = XXH3_64bits_digest(XXH3);
    XXH3_freeState(XXH3);
    return sum;
}

// Copies file from source to dest and removes shebang from the top
void remove_shebang(const std::string& source, const std::string& dest)
{
    struct stat st{ };
    if (::stat(source.c_str(), &st) < 0) // if source does not exist
    {
        ERR << "Source file: " << source << " does not exist." << Endl;
        exit(ERROR_ARGUMENTS);
    }

    std::ifstream ifs(source);
    std::ofstream ofs(dest);

    if (!ifs)
    {
        ERR << "Failed to open " << source << "." << Endl;
        exit(ERROR_ARGUMENTS);
    }

    if (!ofs)
    {
        ERR << "Failed to open " << dest << "." << Endl;
        exit(ERROR_CACHE);
    }

    std::string line;
    bool first_line = true;

    while (std::getline(ifs, line))
    {
        if (first_line)
        {
            first_line = false;

            if (line.starts_with("#!"))
                continue; // Do not copy this line - it's shebang
        }

        ofs << line << std::endl;
    }

    ifs.close();
    ofs.close();
}

#define PRINT_ARR(arr) arr, #arr

void print_array(const std::vector<char*>& arr, const std::string& name) noexcept
{
    auto arr_line = DEBUG;
    arr_line = std::move(arr_line) << "  " << name << " = { " << (arr[0] != nullptr ? arr[0] : "NULL");
    for (int i = 1; i < arr.size(); ++i)
        arr_line = std::move(arr_line) << ", " << (arr[i] != nullptr ? arr[i] : "NULL");
    std::move(arr_line) << " };" << Endl;
}

// Compiles source and returns binary file path
std::string compile(const std::string& source, std::vector<char*> envp)
{
    // Make sure envp ends with NULL
    // Necessary for compilations below
    if (envp.back() != nullptr)
        envp.push_back(nullptr);


    // wordexp performs shell-like path expansion
    // Mostly to expand ~ into /home/user
    std::string cache_folder = wordexp(MainConfig::Instance().get_cache_folder_path());

    struct stat st{ };
    if (::stat(cache_folder.c_str(), &st) < 0) // if it does not exist
        mkdir_p(cache_folder.c_str(), S_IRWXG | S_IRWXU | S_IROTH);
    else if (!S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) // if it is not a dir and not a symlink
    {
        ERR << "Path: " << cache_folder << " is neither a directory nor a link." << Endl;
        exit(ERROR_CACHE);
    }


    std::string source_filename = std::filesystem::path(source).filename();

    // A copy of the source file
    // Always with .cpp extension
    // Without shebang at the top, so g++ compilation doesn't fail
    std::string raw_source = cache_folder + "/" + source_filename + ".cpp";

    // If it exists try another name
    for (int i = 0; stat(raw_source.c_str(), &st) == 0 && i < 1000; ++i)
        raw_source = cache_folder + "/" + source_filename + "." + std::to_string(i) + ".cpp";

    remove_shebang(source, raw_source);


    // raw_source + resolved dependencies
    std::string resolved_source = cache_folder + "/" + source_filename + ".ii";

    // If it exists try another name
    for (int i = 0; stat(resolved_source.c_str(), &st) == 0 && i < 1000; ++i)
        resolved_source = cache_folder + "/" + source_filename + "." + std::to_string(i) + ".ii";


    // -I<path> makes gcc look for includes here and treat this folder
    // as if it were current directory.
    // This allows scripts to include files relative to their own directory
    std::string IClause = "-I" + Dirname(realpath(source));

    std::vector<char*> argv_dep = {
        const_cast<char*>("g++"),
        /* appname */
        const_cast<char*>("-E"),
        IClause.data(),
        raw_source.data(),
        const_cast<char*>("-o"),
        resolved_source.data()
    };

    auto extra_argv_dep = MainConfig::Instance().get_preprocessing_flags();

    if (argv_dep.back() == nullptr)
        argv_dep.pop_back();

    for (auto& arg : extra_argv_dep)
        argv_dep.push_back(arg.data());

    // Make sure argv ends with NULL
    if (argv_dep.back() != nullptr)
        argv_dep.push_back(nullptr);


    if (!Fork()) // g++ -E
    {
        DEBUG << "Resolving dependencies..." << Endl;


        execvpe(argv_dep[0], argv_dep.data(), envp.data());

        // This code is reached only if the execvp() call failed.
        ERR << "[Compile] execvpe(g++ -E) syscall failed." << Endl;
        ERR << "  " << strerrorname_np(errno) << ": " << strerrordesc_np(errno) << Endl;

        print_array(PRINT_ARR(argv_dep));
        print_array(PRINT_ARR(envp));

        exit(ERROR_EXECVE);
    }

    // Can be deleted - we don't need it anymore
    unlink(raw_source.c_str());


    // Shebangless source's hash
    hash hash = get_file_hash(resolved_source);

    std::string output_name = std::to_string(hash.val);

    if (hash.error)
    {
        WARN << "Could not compute file hash for " << source << "." << Endl;
        output_name = source_filename + ".bin"; // fall back to source name + .bin
    }

    // Prepend output file name with cache directory path to make it a valid path
    output_name = cache_folder + "/" + output_name;

    if (!hash.error && ::stat(output_name.c_str(), &st) == 0 && S_ISREG(st.st_mode))
    {
        // Is already compiled.
        unlink(resolved_source.c_str());
        return output_name;
    }

    // Is not yet compiled

    // Use a faster linker
    std::string ld = get_best_available_ld();
    if (!ld.empty())
        ld = "-fuse-ld=" + ld;

    std::vector<char*> argv = {
        const_cast<char*>("g++") /* appname */,
        const_cast<char*>("-O0"),
        const_cast<char*>("-g0"),
        resolved_source.data(),
        const_cast<char*>("-o"),
        output_name.data(),
        ld.empty() ? nullptr : ld.data()
    };

    auto extra_argv = MainConfig::Instance().get_cxx_flags();

    if (argv.back() == nullptr)
        argv.pop_back();

    for (auto& arg : extra_argv)
        argv.push_back(arg.data());

    // Make sure argv ends with NULL
    if (argv.back() != nullptr)
        argv.push_back(nullptr);

    if (!Fork()) // g++ <...>.ii -o <...>
    {
        DEBUG << "Compiling the script..." << Endl;


        execvpe(argv[0], argv.data(), envp.data());

        // This code is reached only if the execvp() call failed.
        ERR << "[Compile] execvpe(g++ <...>.ii -o <...>) syscall failed." << Endl;
        ERR << "  " << strerrorname_np(errno) << ": " << strerrordesc_np(errno) << Endl;

        print_array(PRINT_ARR(argv));
        print_array(PRINT_ARR(envp));

        exit(ERROR_EXECVE);
    }

    unlink(resolved_source.c_str());

    return std::move(output_name);
}

void run(const std::string& binary, std::vector<char*> argv, std::vector<char*> envp)
{
    DEBUG << "Running compiled script..." << Endl;

    // Make sure argv ends with NULL
    if (argv.back() != nullptr)
        argv.push_back(nullptr);

    // Remove NULL
    if (envp.back() == nullptr)
        envp.pop_back();

    envp.push_back(const_cast<char*>("PARENT_APP_NAME=" APPNAME));
    envp.push_back(const_cast<char*>("PARENT_APP_VERSION=" APP_VERSION));

    // Make sure envp ends with NULL
    if (envp.back() != nullptr)
        envp.push_back(nullptr);

    if (!Fork()) // Fork Environment below
    {
        execvpe(binary.c_str(), argv.data(), envp.data());

        // This code is reached only if the execvp() call failed.
        ERR << "[Run] execvpe() syscall failed." << Endl;
        ERR << "  " << strerrorname_np(errno) << ": " << strerrordesc_np(errno) << Endl;

        print_array(PRINT_ARR(argv));
        print_array(PRINT_ARR(envp));

        exit(ERROR_EXECVE);
    }
}

#endif //CPPSCRIPT_LIB_CPP
