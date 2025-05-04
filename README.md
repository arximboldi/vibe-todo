# TUI Todo App (C++)

A simple terminal-based Todo list application written in C++ using a value-oriented architecture.

## Features

*   Add new todo items.
*   Mark todo items as done/undone.
*   Remove todo items.
*   Navigate the list using keyboard.
*   Persists the todo list to disk automatically.
*   Cross-platform data storage location (Linux, macOS, Windows).

## Technologies Used

*   **C++17:** Core language.
*   **CMake:** Build system.
*   **FTXUI:** Terminal User Interface library.
*   **Lager:** Value-oriented state management (Redux/Elm-like).
*   **Immer:** Persistent/Immutable data structures (used by Lager).
*   **Spdlog:** Logging library.
*   **Nlohmann JSON:** JSON serialization/deserialization for persistence.
*   **Nix (optional):** For reproducible development environment via `shell.nix`.

## Building and Running

### Prerequisites

*   A C++17 compliant compiler (GCC >= 7, Clang >= 5, MSVC >= 19.14).
*   CMake (>= 3.17 recommended).
*   Git (for CMake's FetchContent).
*   An internet connection (for CMake to download dependencies).
*   (Linux/macOS) `ncurses` development libraries (usually `ncurses-devel` or `libncurses-dev`).
*   (Linux/macOS) `pkg-config`.

### Option 1: Standard CMake Build

1.  **Clone the repository:**
    #+end_srcbash
    git clone <your-repo-url>
    cd <your-repo-name>
    #+begin_src

2.  **Configure using CMake:**
    #+end_srcbash
    mkdir build
    cd build
    cmake ..
    #+begin_src
    *This step will download FTXUI, Lager, Immer, Spdlog, and Nlohmann JSON using FetchContent.*

3.  **Build the application:**
    #+end_srcbash
    cmake --build .
    # Or simply 'make' on Linux/macOS
    #+begin_src

4.  **Run the application:**
    *   Linux/macOS: `./tui_app`
    *   Windows: `.\Debug\tui_app.exe` or `.\Release\tui_app.exe`

### Option 2: Using Nix (Recommended for Reproducibility)

1.  **Install Nix:** Follow the instructions at [https://nixos.org/download.html](https://nixos.org/download.html).

2.  **Clone the repository:**
    #+end_srcbash
    git clone <your-repo-url>
    cd <your-repo-name>
    #+begin_src

3.  **Enter the Nix development shell:**
    #+end_srcbash
    nix-shell
    # Or =nix develop= if you adapt this to use Nix Flakes later
    #+begin_src
    *This command reads `shell.nix`, downloads the specified dependencies (CMake, GCC, etc.), and provides a shell where they are available.*

4.  **Configure using CMake (inside the Nix shell):**
    #+end_srcbash
    mkdir build
    cd build
    cmake ..
    #+begin_src

5.  **Build the application (inside the Nix shell):**
    #+end_srcbash
    cmake --build .
    #+begin_src

6.  **Run the application (inside the Nix shell):**
    #+end_srcbash
    ./tui_app
    #+begin_src

7.  **Exit the Nix shell:**
    #+end_srcbash
    exit
    #+begin_src

## Usage

*   **Input Field:** Type new todo text and press `Enter` to add.
*   **Todo List:**
    *   Use `Up`/`Down` arrow keys to navigate and select items.
    *   Press `Enter` on a selected item to toggle its done status (`[ ]`/`[x]`).
*   **Buttons:**
    *   `Add`: Adds the text from the input field (same as `Enter` in input).
    *   `Remove Sel.`: Removes the currently selected todo item.
    *   `Toggle Sel.`: Toggles the done status of the selected item (same as `Enter` in list).
    *   `Save`: Manually triggers saving the list to disk (though it might save automatically on changes or exit depending on implementation details not specified).
    *   `Quit`: Exits the application.
*   **Focus:** Use `Tab` / `Shift+Tab` (may depend on terminal) to move focus between the input field and the todo list.

## Data Storage

The todo list is saved as `todos.json` in a platform-specific configuration directory:

*   **Linux:** `$XDG_CONFIG_HOME/TuiTodoCpp/todos.json` (typically `~/.config/TuiTodoCpp/todos.json`)
*   **macOS:** `~/Library/Application Support/TuiTodoCpp/todos.json`
*   **Windows:** `%APPDATA%\TuiTodoCpp\todos.json` (typically `C:\Users\<YourUser>\AppData\Roaming\TuiTodoCpp\todos.json`)

The application will create this directory if it doesn't exist.
