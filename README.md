### Collection of obfuscation passes working with LLVM IR

Building and testing has been done using LLVM 9.
The pass for obfuscating constants (`llvm-pass-obfconst`) requires the C++ `boost` library. 

To build the out-of-tree obfuscating passes, simply execute following shell commands from the root directory:

```
mkdir build
cd build
cmake ..
make 
```

Each pass will be compiled into a subdirectory of `/build`.
To obfuscate a program, first generate a bitcode module, either in `.ll` or `.bc` format:

```
clang -S -emit-llvm <path to program source>
```

Then, each of the passes can be dynamically loaded with `opt` and used to obfuscate the program:

```
opt -load /build/llvm-pass-<pass-name>/<compiled pass> -<pass flag> -S <path to bitcode file> -o <output bitcode file>
```
For example, to apply Instructions substitution (MBA) pass, execute the following:

```
opt -load /build/llvm-pass-mba/libMbaPass.so -mba -S foo.ll -o foo_obfuscated.ll
```

Afterwards, use `clang` (or `clang++`) to obtain the obfuscated binary.

Following flags must be used to apply corresponding passes:

MBA (Substitution): `-mba`
Bogus Control Flow: `-bogus`
Constant obfuscation: `-obfconst`
String obfuscation: `-obfstring`

To use the String obfuscation pass, a `codec.bc` file needs to be present in the directory from which the `opt` tool is being executed. It can be generated by executing:

```
clang -c -emit-llvm <path to codec source> -o codec.bc
```

Two example implementations of codec are included in the `llvm-pass-obfstring` directory. The source code needs to contain functions named `encode` and `decode` and they need to have a single argument of type `unsigned char *`. 