# Small Shell Project - CS 340 at Oregon State University

This project is a simple shell implementation written in C for the CS 340 class at Oregon State University. The shell, named "smallsh," supports basic command execution, built-in commands (such as `cd` and `exit`), and job control for background processes.

## Features

- **Command Execution:** The shell can execute external commands by forking and using `execvp`.
- **Built-in Commands:** Supports built-in commands like `cd` for changing the current directory and `exit` for exiting the shell.
- **Job Control:** The shell can run commands in the background using the `&` operator.
- **Variable Expansion:** Supports variable expansion for `$!`, `$$`, `$?`, and `${parameter}`.

## Usage

To compile the small shell, use the provided source code and include the necessary headers:

```
gcc -o smallsh smallsh.c
```

Run the small shell:
```
./smallsh
```

You can also specify a file as input:
```
./smallsh input_file
```

## Built-in Commands

- **cd [directory]:** Change the current working directory. If no directory is specified, it goes to the home directory.
- **exit [status]:** Exit the shell. If a status is provided, it will be used as the exit status.

## Job Control

To run a command in the background, use the `&` operator:
```
command &
```

## Variable Expansion

The shell supports the following variable expansions:

- **$!:** Process ID of the most recent background command.
- **$$:** Process ID of the smallsh shell itself.
- **$?:** Exit status of the most recently executed foreground command.
- **${parameter}:** The value of the corresponding environment variable named `parameter`.

## Signals

The shell handles the `SIGTSTP` signal. If a background process is running, it will be stopped. To resume the process, use the `fg` command.

## Examples
```
$ ls -l            # Execute an external command
$ cd Documents     # Change directory
$ command &        # Run a command in the background
$ exit             # Exit the shell
```
## Course Information

- **Course:** Operating Systems I (CS 340)
- **University:** Oregon State University

## Contributors

- @hellogaray

Feel free to contribute to this project by submitting issues or pull requests.

Thank you for exploring the small shell project for CS 340 at Oregon State University! Happy coding!

