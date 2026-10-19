{
  description = "Panda CANnectivity firmware development";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { nixpkgs, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in
    {
      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          python = pkgs.python3.withPackages (ps: with ps; [
            west
            pyelftools
            pyyaml
            pykwalify
            packaging
            colorama
            jinja2
            ply
            psutil
            pyserial
            intelhex
            jsonschema
            setuptools
            canopen
            patool
            pylink-square
            requests
            semver
            tqdm
            reuse
            anytree
          ]);
        in
        {
          default = pkgs.mkShellNoCC {
            packages = with pkgs; [
              python
              gcc-arm-embedded
              cmake
              ninja
              dtc
              git
              dfu-util
              usbutils
              can-utils
              iproute2
              bash
              coreutils
              shellcheck
              nixfmt
            ];

            # Use Nix's complete ARM toolchain, without downloading a Zephyr SDK.
            ZEPHYR_TOOLCHAIN_VARIANT = "gnuarmemb";
            GNUARMEMB_TOOLCHAIN_PATH = "${pkgs.gcc-arm-embedded}";
            PYTHONNOUSERSITE = "1";
            hardeningDisable = [ "all" ];
          };
        });

      formatter = forAllSystems (system: nixpkgs.legacyPackages.${system}.nixfmt);
    };
}
