# my-shell

`my-shell` is a small Unix-like shell written in C. It supports external commands, a focused set of built-ins, command history, output redirection, and background jobs.

## Requirements

- A C17-compatible compiler such as Clang or GCC
- GNU Readline development headers and library

On macOS, install Readline with Homebrew if it is not already available:

```sh
brew install readline
```

If Homebrew's Readline is not found automatically, supply its compiler and linker flags, for example:

```sh
make CFLAGS="-Wall -Wextra -Wpedantic -std=c17 -I$$(brew --prefix readline)/include" \
     LDLIBS="-L$$(brew --prefix readline)/lib -lreadline"
```

## Build and run

```sh
make
./src/shell
```

The shell reads and writes history only when `HISTFILE` is set:

```sh
export HISTFILE="$PWD/HISTFILE.txt"
./src/shell
```

## Supported commands

Built-ins: `cd`, `echo`, `exit`, `history`, `jobs`, `pwd`, and `type`.

External commands are resolved through `PATH`. Append `&` to run an external command in the background. Use `>`, `>>`, `1>`, or `2>` for output redirection.

## Development

```sh
make test   # build and run a non-interactive smoke test
make clean  # remove generated executables
```

`src/pipeline_demo.c` is an independent process-pipe example used by the test target; pipeline execution is not yet part of the interactive shell.

## License

This project is available under the MIT License. See [LICENSE](LICENSE).
