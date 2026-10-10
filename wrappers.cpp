//
// Created by jim on 12 Jun 2026.
//



#ifndef CPPSCRIPT_WRAPPERS_CPP
#define CPPSCRIPT_WRAPPERS_CPP

#include "wrappers.h"

#include <filesystem>
#include <string.h>
#include <unistd.h>
#include <wordexp.h>
#include <libgen.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "Log.hpp"
#include "return_codes.h"

void Wait(pid_t pid)
{
    if (pid > 0)
    {
        INFO << "Waiting for child..." << Endl;

        int status;
        pid_t waited_pid = ::waitpid(pid, &status, 0);

        if (waited_pid == -1)
            ERR << "waitpid() syscall failed." << Endl;

        if (WIFEXITED(status))
        {
            auto logtype = (WEXITSTATUS(status) == 0 ? INFO : ERR);
            std::move(logtype) << "Child exited with status " << WEXITSTATUS(status) << "." << Endl;
        }
        else if (WIFSIGNALED(status))
        {
            ERR << "Child was terminated by signal "
                << WTERMSIG(status) << " - " << strsignal(WTERMSIG(status)) << "." << Endl;
            exit(ERROR_CHILD_DIED);
        }

        INFO << "Resuming parent process..." << Endl;
    }
}

// fork() and wait for child pid
pid_t Fork(bool wait)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        ERR << "fork() syscall failed." << Endl;
        exit(ERROR_FORK);
    }

    if (wait) Wait(pid);

    return pid;
}

std::string realpath(const std::string& path)
{
    char* resolved_path = static_cast<char*>(malloc(PATH_MAX));
    if (!::realpath(path.c_str(), resolved_path))
    {
        if (errno)
        {
            DEBUG << "realpath() failed for: " << path << Endl;
            DEBUG << "  Error: " << strerrorname_np(errno) << ": " << strerrordesc_np(errno) << "." << Endl;
        }
        free(resolved_path);
        return { };
    }

    resolved_path[PATH_MAX - 1] = 0; // Terminate the string
    std::string res = resolved_path;
    free(resolved_path);

    return std::move(res);
}

// mkdir -p functionality
int mkdir_p(const std::string& path, mode_t mode)
{
    char tmp[PATH_MAX];
    char* p = nullptr;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path.c_str());
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/') tmp[len - 1] = 0;

    for (p = tmp + 1; *p; ++p)
    {
        if (*p == '/')
        {
            *p = 0;
            if (mkdir(tmp, mode) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    return (mkdir(tmp, mode) != 0 && errno != EEXIST) ? -1 : 0;
}

std::string wordexp(std::string&& path)
{
    wordexp_t wexp;

    if (wordexp(path.c_str(), &wexp, 0))
    {
        WARN << "wordexp() expansion failed for: " << path << Endl;
        return std::move(path);
    }

    std::string res = wexp.we_wordv[0];
    wordfree(&wexp);

    return std::move(res);
}

void rm(const std::string& path)
{
    if (rmdir(path.c_str()) == 0)
        return;

    if (errno == ENOTDIR)
        unlink(path.c_str());
}

// Returns the directory
// Example: /foo/bar/1.cpp -> /foo/bar
std::string Dirname(const std::string& path)
{
    std::string tmp = path;
    char* result = dirname(tmp.data());

    std::string res(result);
    return std::move(res);
}

#endif //CPPSCRIPT_WRAPPERS_CPP
