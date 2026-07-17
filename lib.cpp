//
// Created by jim on 12 Jun 2026.
//


#ifndef CPPSCRIPT_LIB_CPP
#define CPPSCRIPT_LIB_CPP


#include <complex>
#include <sstream>
#include <deque>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

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


    for (char delim = ':'; std::getline(ss, directory, delim);)
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

hash compute_hash(const std::deque<std::string>& paths)
{
    DEBUG << __FN__ "Computing hash..." << Endl;

    XXH3_state_t* XXH3 = XXH3_createState();
    if (XXH3 == nullptr)
    {
        WARN << __FN__ "Hash Error: Could not initialize XXH3 state." << Endl;
        return hash{.error = 1};
    }

    if (XXH3_64bits_reset(XXH3) == XXH_ERROR)
    {
        WARN << __FN__ "Hash Error: Could not reset XXH3." << Endl;
        XXH3_freeState(XXH3);
        return hash{.error = 1};
    }

    constexpr size_t BUFFER_SIZE = 64 * 1024; // 64KB
    std::vector<char> buffer(BUFFER_SIZE);

    for (const std::string& path : paths)
    {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs.is_open())
        {
            WARN << __FN__ "Hash Error: Could not open file " << path << Endl;
            return hash{.error = 1};
        }

        while (ifs.read(buffer.data(), BUFFER_SIZE) || ifs.gcount() > 0)
        {
            if (XXH3_64bits_update(XXH3, buffer.data(), ifs.gcount()) == XXH_ERROR)
            {
                WARN << __FN__ "Hash Error: Failed to update hash for file " << path << Endl;
                XXH3_freeState(XXH3);
                return hash{.error = 1};
            }
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
        ERR << __FN__ "Source file: " << source << " does not exist." << Endl;
        exit(ERROR_ARGUMENTS);
    }

    std::ifstream ifs(source);
    std::ofstream ofs(dest);

    if (!ifs)
    {
        ERR << __FN__ "Failed to open " << source << "." << Endl;
        exit(ERROR_ARGUMENTS);
    }

    if (!ofs)
    {
        ERR << __FN__ "Failed to open " << dest << "." << Endl;
        exit(ERROR_CACHE);
    }

    bool first_line = true;
    for (std::string line; std::getline(ifs, line);)
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

void print_array(const std::deque<std::string>& arr, const std::string& name) noexcept
{
    auto arr_line = DEBUG;
    arr_line = std::move(arr_line) << "  " << name << " = { " << (!arr[0].empty() ? arr[0] : "NULL");
    for (int i = 1; i < arr.size(); ++i)
        arr_line = std::move(arr_line) << ", " << (!arr[i].empty() ? arr[i] : "NULL");
    std::move(arr_line) << " };" << Endl;
}

void switch_(auto*& ptr, auto* ptr1, auto* ptr2)
{
    if (ptr == ptr1)
        ptr = ptr2;
    else
        ptr = ptr1;
}

std::deque<std::string> split_output(int fd, char separator = ' ')
{
    std::deque<std::string> split;

    char buffer1[PIPE_BUF];
    char buffer2[PIPE_BUF];
    int w_begin = PIPE_BUF, w_end = PIPE_BUF;
    char* buffer = buffer1;
    char* prev_buffer = buffer2;

    ssize_t bytes_read = 0, prev_buf_size = 0, prev_buf_size_was = 0;
    while ((bytes_read = read(fd, buffer, PIPE_BUF)) > 0)
    {
        w_begin -= PIPE_BUF;
        for (int i = 0; i < bytes_read; ++i)
            if (buffer[i] == separator)
            {
                w_end = i;

                std::string tmp;
                if (w_begin < prev_buf_size)
                    tmp.append(prev_buffer + w_begin, prev_buf_size - w_begin);

                auto begin_fraction = w_begin > PIPE_BUF ? w_begin - PIPE_BUF : 0;
                tmp.append(buffer + begin_fraction, w_end - begin_fraction);

                w_begin = i + 1 + PIPE_BUF;

                if (!tmp.empty())
                    split.push_back(std::move(tmp));
            }


        // Switch buffers
        switch_(buffer, buffer1, buffer2);
        switch_(prev_buffer, buffer1, buffer2);

        // Switch sizes
        prev_buf_size_was = prev_buf_size;
        prev_buf_size = bytes_read;
    }

    {
        w_end = prev_buf_size;

        std::string tmp;
        if (w_begin < prev_buf_size_was)
            tmp.append(buffer + w_begin, prev_buf_size_was - w_begin);

        auto begin_fraction = w_begin > PIPE_BUF ? w_begin - PIPE_BUF : 0;
        tmp.append(prev_buffer + begin_fraction, w_end - begin_fraction);

        while (
            tmp.back() == '\0' ||
            tmp.back() == separator
        )
            tmp.pop_back();

        if (!tmp.empty())
            split.push_back(std::move(tmp));
    }

    return std::move(split);
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
        ERR << __FN__ "Path: " << cache_folder << " is neither a directory nor a link." << Endl;
        exit(ERROR_CACHE);
    }


    std::string source_filename = std::filesystem::path(source).filename();

    // A copy of the source file
    // Always with .cpp extension
    // Without shebang at the top, so g++ compilation doesn't fail
    std::string shebangless_source = cache_folder + "/" + source_filename + ".cpp";

    // If it exists try another name
    for (int i = 0; stat(shebangless_source.c_str(), &st) == 0 && i < 1000; ++i)
        shebangless_source = cache_folder + "/" + source_filename + "." + std::to_string(i) + ".cpp";

    remove_shebang(source, shebangless_source);


    // -I<path> makes gcc look for includes here and treat this folder
    // as if it were current directory.
    // This allows scripts to include files relative to their own directory
    std::string IClause = "-I" + Dirname(realpath(source));

    std::vector<char*> argv_dep = {
        const_cast<char*>("g++"),
        /* appname */
        const_cast<char*>("-MM"),
        IClause.data(),
        shebangless_source.data(),
        nullptr
    };

    // Capture stdout
    int stdout_pipe[2]{-1, -1};
    pipe(stdout_pipe);

    if (!Fork(false)) // g++ -MM
    {
        close(stdout_pipe[0]);

        if (stdout_pipe[1] >= 0) // Check if open
            dup2(stdout_pipe[1], STDOUT_FILENO);

        DEBUG << __FN__ "Resolving dependencies..." << Endl;


        execvpe(argv_dep[0], argv_dep.data(), envp.data());

        // This code is reached only if the execvp() call failed.
        ERR << __FN__ "[Compile] execvpe(g++ -MM) syscall failed." << Endl;
        ERR << __FN__ "  " << strerrorname_np(errno) << ": " << strerrordesc_np(errno) << Endl;

        print_array(PRINT_ARR(argv_dep));
        print_array(PRINT_ARR(envp));

        exit(ERROR_EXECVE);
    }

    // Close input side to enable reading
    close(stdout_pipe[1]);

    auto dependencies = split_output(stdout_pipe[0]);
    dependencies.pop_front(); // remove "main.cpp.o:" which is always the first element
    decltype(dependencies) filtered_dependencies;

    for (auto& dependency : dependencies)
    {
        if (dependency == "\\\n")
            continue;

        while (dependency.back() == '\n' ||
            dependency.back() == '\r' ||
            dependency.back() == '\t')
            dependency.pop_back();

        filtered_dependencies.push_back(dependency);
    }

    print_array(filtered_dependencies, "Dependencies");

    // Shebangless source's hash
    hash hash = compute_hash(filtered_dependencies);

    std::string output_name = std::to_string(hash.val);

    if (hash.error)
    {
        WARN << __FN__ "Could not compute file hash for " << source << "." << Endl;
        output_name = source_filename + ".bin"; // fall back to source name + .bin
    }

    // Prepend output file name with cache directory path to make it a valid path
    output_name = cache_folder + "/" + output_name;

    if (!hash.error && ::stat(output_name.c_str(), &st) == 0 && S_ISREG(st.st_mode))
    {
        // Is already compiled.
        unlink(shebangless_source.c_str());
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
        IClause.data(),
        shebangless_source.data(),
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
        DEBUG << __FN__ "Compiling the script..." << Endl;


        execvpe(argv[0], argv.data(), envp.data());

        // This code is reached only if the execvp() call failed.
        ERR << __FN__ "[Compile] execvpe(g++ <...>.ii -o <...>) syscall failed." << Endl;
        ERR << __FN__ "  " << strerrorname_np(errno) << ": " << strerrordesc_np(errno) << Endl;

        print_array(PRINT_ARR(argv));
        print_array(PRINT_ARR(envp));

        exit(ERROR_EXECVE);
    }

    unlink(shebangless_source.c_str());

    return std::move(output_name);
}

void run(const std::string& binary, std::vector<char*> argv, std::vector<char*> envp)
{
    DEBUG << __FN__ "Running compiled script..." << Endl;

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
        ERR << __FN__ "[Run] execvpe() syscall failed." << Endl;
        ERR << __FN__ "  " << strerrorname_np(errno) << ": " << strerrordesc_np(errno) << Endl;

        print_array(PRINT_ARR(argv));
        print_array(PRINT_ARR(envp));

        exit(ERROR_EXECVE);
    }
}

#define A_WEEK 604800 // in seconds
#define AN_HOUR  3600 // in seconds
#define LAST_SCAN ".last_scan"
// Removes old cache entries
void cache_autoclean(const std::string& last_file)
{
    DEBUG << __FN__ "Performing Automatic Cache Management..." << Endl;

    // If the binary is bigger than 64MiB don't keep it in cache
    struct stat st{ };
    if (::stat(last_file.c_str(), &st) == 0 &&
        st.st_size >= 64 * 1024 * 1024)
        rm(last_file); // Remove very large files immediately after execution

    if (!Fork())
    {
        // wordexp performs shell-like path expansion
        // Mostly to expand ~ into /home/user
        std::string cache_folder = wordexp(MainConfig::Instance().get_cache_folder_path());
        int cachedirfd = ::open(cache_folder.c_str(), O_DIRECTORY);

        // Remove files that haven't been accessed in a week
        struct statx stx{ };
        if (::statx(cachedirfd, LAST_SCAN, AT_STATX_SYNC_AS_STAT | AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT, STATX_ALL, &stx) == 0 &&
            stx.stx_atime.tv_sec && time(nullptr) - stx.stx_atime.tv_sec <= AN_HOUR)
            exit(0);

        DEBUG << __FN__ "Scanning cache folder: " << cache_folder << "..." << Endl;

        DIR* cache_dirstream = opendir(cache_folder.c_str());

        if (cache_dirstream == nullptr)
        {
            ERR << __FN__ "Couldn't open \"" << cache_folder << "\". "
                << strerrorname_np(errno) << ": " << strerrordesc_np(errno) << Endl;
            exit(ERROR_CACHE);
        }


        errno = 0;
        for (dirent* entry = nullptr; (entry = readdir(cache_dirstream)) != nullptr;)
        {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            // Remove files that haven't been accessed in a week
            if (::statx(cachedirfd, entry->d_name, AT_STATX_SYNC_AS_STAT | AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT, STATX_ALL, &stx) == 0 &&
                stx.stx_atime.tv_sec && time(nullptr) - stx.stx_atime.tv_sec > A_WEEK)
                rm(cache_folder + "/" + entry->d_name);

            errno = 0;
        }

        // Create

        timespec current_time[2];

        // UTIME_NOW tells the VFS layer to fetch the current system clock
        current_time[0].tv_nsec = UTIME_NOW; // Updates Access Time (atim)
        current_time[1].tv_nsec = UTIME_NOW; // Updates Modification Time (mtim)

        // The tv_sec fields are ignored when tv_nsec is set to UTIME_NOW,
        // but initializing them to 0 is standard practice to prevent garbage data.
        current_time[0].tv_sec = 0;
        current_time[1].tv_sec = 0;

        // Mark last scan date
        creat((cache_folder + "/" + LAST_SCAN).c_str(), S_IRUSR | S_IRGRP | S_IROTH);

        // Execute the system call
        if (utimensat(cachedirfd, LAST_SCAN, current_time, 0) == -1)
        {
            WARN << __FN__ "utimensat() failed. "
                << strerrorname_np(errno) << ": " << strerrordesc_np(errno) << Endl;
        }

        exit(0);
    }
}

#endif //CPPSCRIPT_LIB_CPP
