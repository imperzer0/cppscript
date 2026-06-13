//
// Created by jim on 12 Jun 2026.
//


#ifndef CPPSCRIPT_LIB_CPP
#define CPPSCRIPT_LIB_CPP


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

// Compiles source and returns binary file path
std::string compile(const std::string& source)
{
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
    std::string compilable_source = cache_folder + "/" + source_filename + ".cpp";

    remove_shebang(source, compilable_source);


    // Shebangless source's hash
    hash hash = get_file_hash(compilable_source);

    std::string output_name = std::to_string(hash.val);

    if (hash.error)
    {
        WARN << "Could not compute file hash for " << source << "." << Endl;
        output_name = source_filename + ".bin"; // fall back to source name + .bin
    }

    // Prepend output file name with cache directory path to make it a valid path
    output_name = cache_folder + "/" + output_name;

    if (!hash.error && ::stat(output_name.c_str(), &st) == 0 && S_ISREG(st.st_mode))
        return output_name; // Is already compiled.

    // Is not yet compiled

    // Use a faster linker
    std::string ld = get_best_available_ld();
    if (!ld.empty())
        ld = "-fuse-ld=" + ld;

    // -I<path> makes gcc look for includes here and treat this folder
    // as if it were current directory.
    // This allows scripts to include files relative to their own directory
    std::string IClause = "-I" + Dirname(realpath(source));

    char* argvp[] = {
        const_cast<char*>("g++") /* appname */,
        const_cast<char*>("-O0"),
        const_cast<char*>("-g0"),
        IClause.data(),
        compilable_source.data(),
        const_cast<char*>("-o"),
        output_name.data(),
        ld.empty() ? nullptr : ld.data(),
        nullptr
    };

    if (!Fork()) // Fork Environment below
    {
        DEBUG << "Compiling the script..." << Endl;


        execvp(argvp[0], argvp);

        // This code is reached only if the execvp() call failed.
        ERR << "execvp() syscall failed." << Endl;
        ERR << "  " << strerrorname_np(errno) << ": " << strerrordesc_np(errno) << Endl;
        exit(ERROR_EXECVE);
    }

    unlink(compilable_source.c_str());

    return std::move(output_name);
}

void run(const std::string& binary, char* argv[], char* envp[])
{
    DEBUG << "Running compiled script..." << Endl;

    char* envp_modified[256]{ };

    envp_modified[0] = const_cast<char*>("PARENT_APP_NAME=" APPNAME);
    envp_modified[1] = const_cast<char*>("PARENT_APP_VERSION=" APP_VERSION);

    for (int i = 2; i < 256; ++i) // skip first 2
    {
        if (envp[i] == nullptr)
            break;

        envp_modified[i] = envp[i];
    }

    if (!Fork()) // Fork Environment below
    {
        execvpe(binary.c_str(), argv, envp_modified);

        // This code is reached only if the execvp() call failed.
        ERR << "execvpe() syscall failed." << Endl;
        ERR << "  " << strerrorname_np(errno) << ": " << strerrordesc_np(errno) << Endl;
        exit(ERROR_EXECVE);
    }
}

#endif //CPPSCRIPT_LIB_CPP
