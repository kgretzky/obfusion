# Obfusion - C++ X86 Code Obfuscation Library

This library handles obfuscation of assembled X86 machine code in order
to make it harder to read and analyze during the reverse engineering process.

Should work very well with obfuscating shellcode that is later embedded with
executable files. If shellcode is known to security products, the obfuscation
process should make it bypass any signature detection scans.

This is a follow-up to the research I did on obfuscation of x86 instructions
that I documented on my blog:

[X86 Shellcode Obfuscation - Part 1](https://breakdev.org/x86-shellcode-obfuscation-part-1/)

[X86 Shellcode Obfuscation - Part 2](https://breakdev.org/x86-shellcode-obfuscation-part-2/)

[X86 Shellcode Obfuscation - Part 3](https://breakdev.org/x86-shellcode-obfuscation-part-3/)

Library was initially compiled with MSVS2008, so there should be no compatibility
issues even if you try to compile it using newer versions of Visual Studio.

Makefiles for Linux are coming soon(ish).

# Examples

See `examples/` directory to learn how to implement this library in your own projects.

# Demo

Here is the disassembled sample shellcode that spawns `calc.exe` in original form: [original shellcode](https://raw.githubusercontent.com/kgretzky/obfusion/master/examples/res/exec_calc.lst)

And here is the disassembly of the same sample shellcode after the 3-pass obfuscation process: [obfuscated shellcode](https://raw.githubusercontent.com/kgretzky/obfusion/master/examples/res/output.lst)

# How to compile

#### Windows

Open the `.sln` solution in Visual Studio (at least version 2008) and click `Build Solution`

#### Linux

Navigate to project directory and type:

```
cmake .
make
sudo make install
```

# External libraries

Hacker Disassembler Engine 32/64
Copyright (c) 2006-2009, Vyacheslav Patkov.
All rights reserved.

# Contact

E-mail: kuba -at- breakdev.org

# License

Library is released under GNU/GPL version 3.0

Copyright (c) 2016 Kuba Gretzky