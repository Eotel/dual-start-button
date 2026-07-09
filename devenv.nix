{ pkgs, config, ... }:

let
  # pytest and PlatformIO live in the devenv-managed venv (created on shell
  # entry), so hooks work from any shell once `devenv shell` has run at least
  # once. pip supplies PlatformIO so local hooks run the same distribution CI
  # installs (pkgs.platformio-core also works with the compiler shim below).
  venvBin = "${config.devenv.state}/venv/bin";

  # PlatformIO's native env compiles host tests with `gcc`/`g++`. Inside the
  # devenv shell on darwin, /usr/bin/gcc (an xcrun shim) fails because
  # DEVELOPER_DIR points at the nix apple-sdk, which ships no tool named
  # `gcc`. Expose the nix compiler under the names the native platform uses.
  nativeCompilerShim = pkgs.runCommand "native-cc-shim" { } ''
    mkdir -p $out/bin
    ln -s ${pkgs.stdenv.cc}/bin/cc $out/bin/gcc
    ln -s ${pkgs.stdenv.cc}/bin/c++ $out/bin/g++
  '';

  # Shared shape of the pre-push hooks (tests + static analysis).
  prePushHook = {
    enable = true;
    language = "system";
    pass_filenames = false;
    always_run = true;
    stages = [ "pre-push" ];
  };
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
    uv.enable = true;
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
      # The hook's default resolves to a newer clang-tools than the shell's
      # pkgs.clang-tools (observed 22.x vs 21.x); pin so manual runs and the
      # hook cannot disagree on formatting.
      package = pkgs.clang-tools;
      types_or = [
        "c"
        "c++"
      ];
    };
    biome = {
      enable = true;
      args = [ "--no-errors-on-unmatched" ];
    };

    # --- hygiene (pre-commit stage) ---
    end-of-file-fixer.enable = true;
    trim-trailing-whitespace = {
      enable = true;
      # Preserve markdown hard line breaks (trailing double space).
      args = [ "--markdown-linebreak-ext=md" ];
      # The hook's default stages include pre-push and manual; pin to the
      # commit stage like the other formatters.
      stages = [ "pre-commit" ];
    };
    check-yaml.enable = true;
    check-json.enable = true;
    check-merge-conflicts.enable = true;
    actionlint.enable = true;
    ripsecrets.enable = true;

    # --- commit message (commit-msg stage) ---
    gitlint.enable = true;

    # --- fast host-runnable tests + C++ static analysis (pre-push stage) ---
    pytest-tools = prePushHook // {
      name = "pytest (tools)";
      entry = "${venvBin}/pytest tools/";
    };
    node-test-web-debug = prePushHook // {
      name = "node --test (web-debug)";
      # Glob form: node treats a bare directory argument as a module path.
      entry = "${pkgs.nodejs}/bin/node --test web-debug/*.test.js";
    };
    pio-test-native = prePushHook // {
      name = "pio test -e native";
      entry = "${venvBin}/pio test -e native -d firmware/pio";
    };
    pio-check = prePushHook // {
      name = "pio check (cppcheck)";
      entry = "${venvBin}/pio check -e native -e m5atom_lite -d firmware/pio --fail-on-defect medium --fail-on-defect high";
    };
  };

  # `devenv test` already runs the default-stage hooks on all files; add the
  # pre-push suite (tests + static analysis) so one command checks everything.
  enterTest = ''
    pre-commit run --all-files --hook-stage pre-push --show-diff-on-failure
  '';
}
