{ pkgs, ... }:

let
  # pytest and PlatformIO live in the devenv-managed venv (created on shell
  # entry). Hooks reference this project-relative path so they work from any
  # shell once `devenv shell` has run at least once. Upstream pip PlatformIO
  # is used instead of pkgs.platformio-core because the nixpkgs package
  # patches tool resolution and cannot find the host compiler for the native
  # test env; pip also matches what CI installs.
  venvBin = ".devenv/state/venv/bin";

  # PlatformIO's native env compiles host tests with `gcc`/`g++`. Inside the
  # devenv shell on darwin, /usr/bin/gcc (an xcrun shim) fails because
  # DEVELOPER_DIR points at the nix apple-sdk, which ships no tool named
  # `gcc`. Expose the nix compiler under the names the native platform uses.
  nativeCompilerShim = pkgs.runCommand "native-cc-shim" { } ''
    mkdir -p $out/bin
    ln -s ${pkgs.stdenv.cc}/bin/cc $out/bin/gcc
    ln -s ${pkgs.stdenv.cc}/bin/c++ $out/bin/g++
  '';
in
{
  # Toolchain for manual runs; hook entries reference the same packages, so
  # local hooks never depend on globally installed tools.
  packages = [
    pkgs.git
    pkgs.nodejs
    pkgs.biome
    pkgs.ruff
    pkgs.clang-tools
    pkgs.nixfmt-rfc-style
    pkgs.gitlint
    pkgs.pre-commit
    nativeCompilerShim
  ];

  languages.python = {
    enable = true;
    venv.enable = true;
    venv.requirements = ''
      platformio
      pytest
    '';
  };

  # Single configuration point for lint/format/commit-msg/pre-push checks.
  # devenv generates .pre-commit-config.yaml (gitignored) and installs the
  # hooks when entering the shell.
  git-hooks.hooks = {
    # --- format / lint (pre-commit stage) ---
    nixfmt-rfc-style.enable = true;
    ruff.enable = true;
    ruff-format.enable = true;
    clang-format = {
      enable = true;
      # Same clang-format as the shell's pkgs.clang-tools, so manual runs and
      # the hook can never disagree on formatting.
      package = pkgs.clang-tools;
      types_or = [
        "c"
        "c++"
      ];
    };
    biome = {
      enable = true;
      package = pkgs.biome;
      args = [ "--no-errors-on-unmatched" ];
    };

    # --- hygiene (pre-commit stage) ---
    end-of-file-fixer.enable = true;
    trim-trailing-whitespace = {
      enable = true;
      # Preserve markdown hard line breaks (trailing double space).
      args = [ "--markdown-linebreak-ext=md" ];
      stages = [ "pre-commit" ];
    };
    check-yaml.enable = true;
    check-json.enable = true;
    check-merge-conflicts.enable = true;
    actionlint.enable = true;
    ripsecrets.enable = true;

    # --- commit message (commit-msg stage) ---
    gitlint = {
      enable = true;
      package = pkgs.gitlint;
    };

    # --- fast host-runnable tests + C++ static analysis (pre-push stage) ---
    pytest-tools = {
      enable = true;
      name = "pytest (tools)";
      entry = "${venvBin}/pytest tools/";
      language = "system";
      pass_filenames = false;
      always_run = true;
      stages = [ "pre-push" ];
    };
    node-test-web-debug = {
      enable = true;
      name = "node --test (web-debug)";
      # Glob form: node treats a bare directory argument as a module path.
      entry = "${pkgs.nodejs}/bin/node --test web-debug/*.test.js";
      language = "system";
      pass_filenames = false;
      always_run = true;
      stages = [ "pre-push" ];
    };
    pio-test-native = {
      enable = true;
      name = "pio test -e native";
      entry = "${venvBin}/pio test -e native -d firmware/pio";
      language = "system";
      pass_filenames = false;
      always_run = true;
      stages = [ "pre-push" ];
    };
    pio-check = {
      enable = true;
      name = "pio check (cppcheck)";
      entry = "${venvBin}/pio check -e native -e m5atom_lite -d firmware/pio --fail-on-defect medium --fail-on-defect high";
      language = "system";
      pass_filenames = false;
      always_run = true;
      stages = [ "pre-push" ];
    };
  };

  # `devenv test` already runs the default-stage hooks on all files; add the
  # pre-push suite (tests + static analysis) so one command checks everything.
  enterTest = ''
    pre-commit run --all-files --hook-stage pre-push --show-diff-on-failure
  '';
}
