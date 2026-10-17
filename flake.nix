{
  description = "Clay UI renderer backed by LovyanGFX";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    cspell-nix = {
      url = "github:kakkun61/cspell-nix";
      inputs = {
        nixpkgs.follows = "nixpkgs";
        flake-parts.follows = "flake-parts";
        treefmt-nix.follows = "treefmt-nix";
      };
    };

    # Clay and LovyanGFX are consumed as plain source trees. Neither ships a
    # Nix flake, and both are header/source libraries that we compile
    # ourselves, so a non-flake input is all we need.
    clay = {
      url = "github:nicbarker/clay";
      flake = false;
    };
    lovyangfx = {
      url = "github:lovyan03/LovyanGFX";
      flake = false;
    };
  };

  outputs =
    inputs@{
      flake-parts,
      treefmt-nix,
      cspell-nix,
      clay,
      lovyangfx,
      ...
    }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      imports = [
        treefmt-nix.flakeModule
        cspell-nix.flakeModule
      ];

      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "aarch64-darwin"
      ];

      perSystem =
        { config, pkgs, ... }:
        let
          makeFlags = [
            "CLAY_DIR=${clay}"
            "LOVYANGFX_DIR=${lovyangfx}"
          ];

          # The ESP32 Arduino core downloads prebuilt, dynamically linked
          # toolchains at run time. They expect a conventional FHS layout,
          # which NixOS does not provide, so arduino-cli is wrapped in an FHS
          # sandbox.
          arduinoCliFhs = pkgs.buildFHSEnv {
            name = "arduino-cli";
            targetPkgs =
              p: with p; [
                arduino-cli
                (python3.withPackages (pp: [ pp.pyserial ]))
                zlib
                libusb1
                ncurses
                udev
              ];
            runScript = "arduino-cli";
          };
        in
        {
          packages = {
            # The SDL based simulator of the demo application.
            simulator = pkgs.stdenv.mkDerivation {
              pname = "clay-lgfx-simulator";
              version = "0.1.0";
              src = ./.;
              nativeBuildInputs = [ pkgs.pkg-config ];
              buildInputs = [ pkgs.SDL2 ];
              inherit makeFlags;
              buildFlags = [ "sim" ];
              enableParallelBuilding = true;
              installPhase = ''
                runHook preInstall
                install -Dm755 build/sim/clay-lgfx-demo $out/bin/clay-lgfx-demo
                runHook postInstall
              '';
            };
            # The API documentation, generated with Doxygen.
            doc = pkgs.stdenv.mkDerivation {
              pname = "clay-lgfx-doc";
              version = "0.1.0";
              src = ./.;
              nativeBuildInputs = [
                pkgs.doxygen
                pkgs.perl
              ];
              # The Markdown filter Doxygen runs is a script; give it a store path interpreter.
              postPatch = "patchShebangs dev";
              buildFlags = [ "doc" ];
              installPhase = ''
                runHook preInstall
                mkdir -p $out
                cp -r build/doc/html $out/html
                runHook postInstall
              '';
            };
            # Everything nix can build: the simulator, the documentation and a
            # passing test run. ESP32 builds need arduino-cli and its
            # downloads, so they stay with `make esp32-all`.
            default = pkgs.linkFarm "clay-lgfx" {
              simulator = config.packages.simulator;
              doc = config.packages.doc;
              test = config.checks.unit-test;
            };
          }
          // pkgs.lib.optionalAttrs pkgs.stdenv.hostPlatform.isLinux {
            # arduino-cli wrapped so that the ESP32 toolchains it downloads run on
            # NixOS. Projects using this library can take it from here.
            arduino-cli = arduinoCliFhs;
          };

          checks.unit-test = pkgs.stdenv.mkDerivation {
            name = "clay-lgfx-unit-test";
            src = ./.;
            # The tests link LovyanGFX built for the SDL simulator.
            nativeBuildInputs = [ pkgs.pkg-config ];
            buildInputs = [ pkgs.SDL2 ];
            inherit makeFlags;
            # Building the test target runs the tests, so a failing test fails the check.
            buildFlags = [ "test" ];
            enableParallelBuilding = true;
            installPhase = "touch $out";
          };

          devShells.default = pkgs.mkShell {
            packages = [
              pkgs.gnumake
              pkgs.pkg-config
              pkgs.SDL2
              pkgs.doxygen
              # Runs dev/doxygen-markdown-filter.
              pkgs.perl
            ]
            ++ pkgs.lib.optionals pkgs.stdenv.hostPlatform.isLinux [ arduinoCliFhs ]
            ++ pkgs.lib.optionals pkgs.stdenv.hostPlatform.isDarwin [ pkgs.arduino-cli ];

            shellHook = ''
              export CLAY_DIR=${clay}
              export LOVYANGFX_DIR=${lovyangfx}

              # Headers of the ESP32 Arduino core, for editors. arduino-cli
              # downloads the core at run time, so it is looked up here rather
              # than pinned: the newest installed version wins.
              arduino_data="''${ARDUINO_DIRECTORIES_DATA:-$HOME/.arduino15}"
              esp32_core=$(ls -d "$arduino_data"/packages/esp32/hardware/esp32/* 2>/dev/null | sort -V | tail -n 1)
              if [ -n "$esp32_core" ]; then
                export ARDUINO_ESP32_DIR="$esp32_core"
                export ARDUINO_ESP32_SDK_DIR="$arduino_data/packages/esp32/tools/esp32-libs/$(basename "$esp32_core")"
              fi
              unset arduino_data esp32_core
            '';
          };

          # `nix fmt` formats everything; `nix flake check` verifies it.
          treefmt = {
            projectRootFile = "flake.nix";
            programs = {
              nixfmt.enable = true;
              clang-format.enable = true;
            };
            settings.formatter.clang-format.includes = [ "*.ino" ];
          };

          cspell.configFile = ./cspell.yaml;
        };
    };
}
