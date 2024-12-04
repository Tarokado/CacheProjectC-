# CacheProject_ECE485
## How to use
##WINDOWS:
- Compile: `g++ -o Cache_Project Cache_Project.cpp`
- Syntax: `.\Cache_Project.exe [mode(optional)] [trace_file_name]`.
           `.\Cache_Project.exe` to show a menu. 
   example: `.\Cache_Project.exe 0 trace\trace.txt`  
   `.\Cache_Project.exe 1 trace_data.txt`  
   `.\Cache_Project.exe 2 trace_evict.txt`
  
##MACOS/Linux:
- Compile: `g++ -o Cache_Project Cache_Project.cpp`
- Syntax: `./Cache_Project [mode(optional)] [trace_file_name]`.
           `.\Cache_Project.exe` to show a menu.    
   example: `./Cache_Project 0 trace/trace.txt`  
   `./Cache_Project 1 trace_data.txt`  
   `./Cache_Project 2 trace_evict.txt`
- _trace_file_name_: name of your trace file. Here we have already some _.txt_ file to make sure you can use them to test our system.
- _mode_: mode _0_  Displays statistics summary and responses to commands from trace file.  
          mode _1_ Display mode 0 and the L2 communication from trace file.
          mode _2_ Demo multiple trace file with mode 1.
   mode default is mode _0_
_ Result will be printed in _store_history.txt_ and console as well.
