//
// Created by jim on 10 Oct 2026.
//

#ifndef CPPSCRIPT_LIB_H
#define CPPSCRIPT_LIB_H

#include <vector>

// Checks whether a given file is in any of the directories of $PATH
bool is_available_in_path(const std::string& executable);

// Compiles source and returns binary file path
std::string compile(const std::string& source, std::vector<char*> envp);

void run(const std::string& binary, std::vector<char*> argv, std::vector<char*> envp);

// Removes old cache entries
void cache_autoclean(const std::string& last_file);


#endif //CPPSCRIPT_LIB_H
