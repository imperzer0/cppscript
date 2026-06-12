//
// Created by jim on 12 Jun 2026.
//



#ifndef CPPSCRIPT_WRAPPERS_CPP
#define CPPSCRIPT_WRAPPERS_CPP

#include <filesystem>
#include <string.h>
#include <unistd.h>
#include <wordexp.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "Log.hpp"
#include "return_codes.h"

pid_t Fork()
{
    pid_t pid = fork();
    if (pid < 0)
    {
        ERR << "fork() syscall failed." << Endl;
        exit(ERROR_FORK);
    }

    if (pid > 0)
    {
        int status;
        pid_t waited_pid = ::waitpid(pid, &status, 0);

        if (waited_pid == -1)
            WARN << "waitpid() syscall failed." << Endl;

        if (WIFEXITED(status))
            INFO << "Child exited normally with status " << WEXITSTATUS(status) << "." << Endl;
        else if (WIFSIGNALED(status))
            WARN << "Child was terminated by signal "
                << WTERMSIG(status) << " - " << strsignal(WTERMSIG(status)) << "." << Endl;

        INFO << "Resuming parent process..." << Endl;
    }

    return pid;
}

std::string realpath(const std::string& path)
{
    char* resolved_path = static_cast<char*>(malloc(PATH_MAX));
    if (!::realpath(path.c_str(), resolved_path))
    {
        if (errno)
        {
            WARN << "realpath() failed for: " << path << Endl;
            WARN << "  Error: " << strerrorname_np(errno) << ": " << strerrordesc_np(errno) << "." << Endl;
        }
        free(resolved_path);
        return new char[1]{ };
    }

    resolved_path[PATH_MAX - 1] = 0; // Terminate the string
    std::string res = resolved_path;
    free(resolved_path);

    return std::move(res);
}

int mkdir_p(const std::string& path, mode_t mode)
{
    char tmp[PATH_MAX];
    char* p = nullptr;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path.c_str());
    len = strlen(tmp);
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;

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

#endif //CPPSCRIPT_WRAPPERS_CPP
