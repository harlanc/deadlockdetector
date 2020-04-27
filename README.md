# DeadLockDetector


## Introduction

This project is a dead lock detector for c/c++ multithread program.We can get the following results using the demo provided by this project :

![](https://github.com/harlanc/deadlockdetector/blob/master/stacktrace.jpg)

We can get the following information using dead lock detector:

- It can detect if it exists a deadlock in your project.
- It can record some useful logs, including the stacktrace where the deadlock is generated and also some accurate information like 
 "**Thead A apply lock AA;Lock AA is owned by thread B;Thread B apply lock BB;Lock BB is owned by thread A**"
 
## Dependencies

[spdlog](https://github.com/gabime/spdlog) is used for recording logs.Install it refer to the [official documents](https://github.com/gabime/spdlog#package-managers).

[backward-cpp](https://github.com/bombela/backward-cpp) is used for record stacktrace.Note that it needs the third party library [libdw](https://github.com/bombela/backward-cpp#libraries-to-read-the-debug-info)  to read the debug info.
 
## Platforms

This tool now can only be used on GNU/Linux because of the stacktrace library[backward-cpp]‘s limitation.
 
## How to use

### Run the demo

- Clone the codes 
- Install revelant dependencies.
- Run makefile in root folder of the project to generate the executable file.
- Run the executable file to get the log file which contains dead lock information.

### For your own project

You can copy *dead_lock_stub.h* and *backward.hpp* to your own project and then add the following line in the main function file:

    #define BACKWARD_HAS_DW 1
    
Add the following line in the file where lock/unlock function is called.

    #include "dead_lock_stub.h"
    
Add the following codes in your main function:

    DeadLockGraphic::getInstance().start_check();
    
Compile your codes to generate the executable file.Run it and if deadlock is detected,logs will be saved in the log file.



## Thanks

This project originated from this [blog](https://www.cnblogs.com/mumuxinfei/p/4365697.html).