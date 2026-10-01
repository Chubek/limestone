# AGENTS.md -- Limestone

Limestone is a late-stage compiler framework. It offers several intermediate languages with APIs in C++. Each language is a vertical slice in the pipeline that begins with instruction selection, goes on to interleaved register allocation, instruction selection, and tuning. The result, a low-level machine-adjacent IR, is further rewritten and a assembly code is generated. 

The horizontal slice in all this is Metacode, and it's .isa files that define attributes and features of several MPU, MCU, GPU and virtual devices. Metacode acts as an oracle and a middleware, supplying each stage with information on the target architecture.

Users can opt in to generate a Sandstone binary instead of a symbolic assembly file. Sandstone is Limestone's sister framework, and along with Yellowstone -- the linker; and Direstone -- the high-level intermediate presentation, they make the CCWeave compiler infrastructure stack.

Limestone has a benchmarking and linting tool. Furthermore, Limestone has a metatracing JIT builder language, called TraceML, which is compiled to C.


