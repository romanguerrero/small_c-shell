# small_c-shell
Provides limited shell functionality  
### Functionality
**Three built-in commands** 
- exit
- cd
- status  
**Executes other shell commands by creating new processes  
Supports input and output redirection  
Can run commands in foreground and background processes  
Implements custom signal handlers for SIGINT (^C) and SIGTSTP (^Z)**

### Quickstart
1. Download main.c
2. Compile
`gcc -g --std=gnu99 -o shell main.c`
3. Run
`./shell`
