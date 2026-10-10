//
// Created by jim on 10 Oct 2026.
//

#ifndef CPPSCRIPT_WRAPPERS_H
#define CPPSCRIPT_WRAPPERS_H

#include <string>


void Wait(pid_t pid);

pid_t Fork(bool wait = true);

std::string realpath(const std::string& path);

int mkdir_p(const std::string& path, mode_t mode);

std::string wordexp(std::string&& path);

void rm(const std::string& path);

std::string Dirname(const std::string& path);


#endif //CPPSCRIPT_WRAPPERS_H
