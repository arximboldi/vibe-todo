# shell.nix
# Use =nix-shell= (or =nix develop= if using flakes) to enter the environment.
let
  # Pin nixpkgs to a known stable revision from nixos-24.05 branch (example)
  # Find recent revisions at: https://status.nixos.org/
  nixpkgsRev = "bf3287dac860542719fe7554e21e686108716879"; # nixos-24.11 as of 04-05-2025

  # Fetch nixpkgs source archive using fetchTarball
  nixpkgsSrc = builtins.fetchTarball "https://github.com/NixOS/nixpkgs/archive/${nixpkgsRev}.tar.gz";

  # Import nixpkgs from the fetched source
  pkgs = import nixpkgsSrc {};

in
pkgs.mkShell {
  # The build inputs needed to build the project
  buildInputs = with pkgs; [
    # Core build tools
    cmake
    gcc13 # Or clang16, need C++17 support
    pkg-config
    git # Needed for FetchContent

    # Libraries that might be needed by dependencies (FTXUI -> ncurses)
    ncurses
    # zlib
  ];

  # Optional: Set environment variables if needed
  # shellHook = ''
  #   export SOME_VAR="value"
  # '';
}
