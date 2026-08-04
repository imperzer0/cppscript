# CPPSCRIPT

## What is this for?

If you want to run C++ code without a need to create a CMake Project.
It is designed to function like a C++ script interpreter.

## Installation

 * Go to this project's GitHub releases;
 * Download ```cppscript``` executable;
 * Run:
   ```bash
   chmod +x cppscript
   chown root:root cppscript
   ```
 * Copy it to any reasonable folder. On my system I have it in `/usr/bin/` directory:
   ```bash
   sudo cp -fv cppscript /usr/bin/cppscript
   ```

## Usage

To use it you have to add a shebang to the top of your script file like this:
```c++
#!/usr/bin/cppscript

#include <iostream>

int main()
{
  std::cout << "Hello World!" << std::endl;
  return 0;
}
```

The shebang consists of two parts: ```#!``` and ```/path/to/cppscript/binary```

Alternatively, you can add this to the top of your file:
```bash
#if 0
exec /usr/bin/cppscript "$0" "$@"
#endif
```
Lines starting with `#` are treated like comments in sh,
but in C++ `#if 0` or `#if false` is a valid preprocessor directive.
It simply disables all the lines in between `#if` and `#endif` for C++,
but when `sh` reaches exec it replaces current process with correct script execution.

There is a caveat, though - it can't be used together with strace.
To make it work with strace you have to use the `#!`.

## Building

This is a normal CMake Project.

```bash
mkdir cmake-build-release;
cd cmake-build-release;
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ ..
make -j 8
```