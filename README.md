# Remembrall

A simple CLI task tracker for developers who constantly forget what they were
doing. Add tasks to your memory, peek at what you're currently remembering, and forget completed work.

## Table of Contents
- [Quick Start](#quick-start)
- [Features](#features)
- [Installation](#installation)
- [Usage](#usage)
- [Contributing](#contributing)
- [License](#license)

## Quick Start

1. Get Remembrall binary file on your system. Checkout how to [install](#installation) Remembrall below.

1. Start remembering what you were working on...

```sh
rmbrl add "implement new feature" # adds memory to your collection
```

```sh
rmbrl peek # prints most recent memory
```

```sh
rmbrl clear # forget most recent memory
```

Checkout [usage](#usage) for more details.

## Features

Core Features:

- Add tasks/reminders to your memory with simple commands
- Peek at your most recent item or view all remembered tasks
- Forget (clear) completed tasks individually or in bulk
- Persistent storage using SQLite3
- Tag items with project flag (e.g. --project project_name)

Technical Features:

- Zero external dependencies (SQLite3 included with source code)
- Single binary executable
- Fast C implementation
- Cross-platform compatibility
- Lightweight memory footprint
- Local-only data storage (no cloud sync)
- No configuration files or setup required

## Installation

### Build

To build `remembrall`, all you need is a C compiler...  

**NOTE**: First time building `remembrall` will take a little bit of time due to compiling `sqlite3.o`

```sh
git clone https://github.com/navazjm/remembrall
```

```sh
cd remembrall
```

#### POSIX

```sh
cc ./build/nob.c -o ./build/nob
```

```sh
./build/nob --install
```

#### Windows (MinGW/GCC)

```sh
gcc ./build/nob.c -o ./build/nob
```

```sh
build\nob.exe --install
```

#### Windows (MSVC)

```sh
cl ./build/nob.c /Fe:./build/nob.exe /Fo:./build/nob.obj
```

```sh
build\nob.exe --install
```

**IMPORTANT**: Windows users will need to manually add "%APPDATA%\rmbrl" to their PATH.

## Usage

**NOTE**: Flags support both `--flag value` and `--flag=value` syntax.

### Commands
| Command | Flags                | Description |
|---------|----------------------|-------------|
| `add`   | `--project`          | Add memory to your collection    |
| `peek`  | `--all`, `--project` | Show what you're currently remembering  |
| `clear` | `--all`, `--project` | Forget memories |

### Command Flags
| Flag        | Short | Supported Commands     | Description |
|-------------|-------|------------------------|-------------|
| `--project` | `-p`  | `add`, `peek`, `clear` | Tag and filter memories by project name |
| `--all`     | `-a`  | `peek`, `clear`        | Apply operation to all memories |

### Global Flags

| Flag        | Short | Description |
|-------------|-------|-------------|
| `--help`    | `-h`  | Show help information |
| `--version` | `-V`  | Show version information |
| `--verbose` | `-v`  | Enable verbose output |
| `--silent`  | `-s`  | Enable silent mode |
| `--dry-run` | `-n`  | Perform dry run without making changes |

### Examples

- Add memory to your collection

```sh
rmbrl add "task description"
```

- Show most recent memory

```sh
rmbrl peek
```

- Show all memories

```sh
rmbrl peek --all
```

- Forget most recent memory

```sh
rmbrl clear
```

- Forget all memories

```sh
rmbrl clear --all
```

#### Project Flag Examples

Can think of this like adding a tag to easily filter tasks by project.

```sh
rmbrl add --project lazypm "refactor filter proc"
```

```sh
rmbrl peek --project lazypm
```

```sh
rmbrl peek --all --project lazypm
```

```sh
rmbrl clear --project lazypm --all
```

## Database Location

`remembrall` stores its database file (`rmbrl.db`) in the standard application data
directory for your operating system

**NOTE**: The directory will be created automatically when you first run `remembrall`.
You can backup your data by copying the `rmbrl.db` file, or migrate to a new system by placing
your backup in the appropriate location.

### Windows

`%APPDATA%\rmbrl\rmbrl.db`

Example: `C:\Users\YourName\AppData\Roaming\rmbrl\rmbrl.db`

### macOS

`~/Library/Application Support/rmbrl/rmbrl.db`

Example: `/Users/YourName/Library/Application Support/rmbrl/rmbrl.db`

### Linux

`~/.local/share/rmbrl/rmbrl.db`

Example: `/home/YourName/.local/share/rmbrl/rmbrl.db`

## Contributing 

Want to contribute to Remembrall? Awesome, we would love your input ♥

If you have a feature request, start a [discussion](https://github.com/navazjm/remembrall/discussions),
and we can work together to incorporate it into Remembrall!

Encountered a defect? Please report it under [issues](https://github.com/navazjm/remembrall/issues).
Be sure to include detailed information to help us address the issue effectively.

Want to implement a feature request or fix a defect? Checkout our [contributing guide](./docs/contributing.md).

## License

Remembrall is licensed under [MIT](./LICENSE)

