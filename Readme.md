# BitLoupe
BitLoupe is a high-precision scientific calculator.
Current stable version: 1.0.
It features a syntax-highlighted scrollable display and is designed to be fully used via keyboard. Some distinctive
features are auto-completion of functions and variables, a formula book, and quick
insertion of constants from various fields of knowledge. It is available for Windows, macOS,
and Linux in a number of languages.

![capture.png](https://bitbucket.org/repo/dR7BnG/images/3654665019-capture.png)

## Building
To build BitLoupe, you need:

- A C++17-capable compiler
- [Qt](http://qt.io) 6.x (Core, Widgets, Help, Network)
- [CMake](http://cmake.org) 3.16 or later

To build BitLoupe in a dedicated build directory and install it, run the following
commands from the root of the source directory:

    mkdir build
    cd build
    cmake ../src
    make install

When building against a Qt version that is not the system default Qt installation,
point CMake towards the Qt installation to use by setting `CMAKE_PREFIX_PATH` or
`Qt6_DIR` when running CMake.

Example (Homebrew on macOS):

    brew install qt
    mkdir build
    cd build
    cmake ../src -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
    make

You can customize the build using the following variables. These are specified when
running CMake, in the form `cmake ../src -Dvariable=value`.

- **PORTABLE_BITLOUPE**: Set this to `on` to have the application settings stored
  in the same location as the executable, e.g. for running from a USB drive without
  requiring installation.
- **CMAKE_INSTALL_PREFIX**: Change the installation prefix for BitLoupe.
- **HTML_DOCS_DIR**: Change the path to the HTML manual that's embedded in the binary
  by the build. By default, a bundled prebuilt copy is used to minimize dependencies.

## SoC register catalog

The **View > SoC Regs** panel reads `socregs.conf` from the directory containing
the BitLoupe executable. Set `BITLOUPE_SOC_CONFIG` to use another catalog file.
Register file paths may be absolute or relative to the catalog file.
For compatibility with existing catalogs, a single SoC object and comma-separated
derivative strings such as `"cyt4bb, cyt4bf"` are also accepted.

At startup, BitLoupe compiles the catalog and its register JSON files into
`socregs.db` in the `data/` folder beside the executable (`conf/` is reserved for
user-editable configuration). The database stores an MD5
fingerprint of the catalog and every referenced register file. It is regenerated
when any source changes. SoC, derivative, register search, and register-detail
lookups use only the cached SQLite database after initialization. Set
`BITLOUPE_SOC_CACHE` to override the database path.

```json
{
  "supported_socs": [
    {
      "silicon_name": "Infineon Traveo2",
      "derivatives": ["cyt4bb", "cyt4bf"],
      "registers_file": "registers/TVIIBH4M.json"
    }
  ]
}
```

A derivative can override the SoC-level register file:

```json
{
  "name": "cyt4bf",
  "registers_file": "registers/cyt4bf.json"
}
```

The register file contains a `registers` array. Each register may contain `bitfields`,
and each bitfield may contain `sub_bitfields` or `enum_values`. The panel search field
accepts a case-insensitive regular expression and searches names and descriptions at
all three levels. Table #1 lists matching registers. Selecting a register opens Panel
#2 with silicon, derivative, page, address, and bitfield/sub-bitfield details.
The `Actual` column extracts each parent bitfield from the current editor value. It
updates as the expression changes, even when live-result previews are disabled. When
the editor is empty, it uses the latest calculated `ans`; sub-bitfield rows display
`--` in this column. Panel #2 shows the complete evaluated value in hexadecimal and
green, highlights parent-bitfield Actual values in green, wraps descriptions, reserves
at least 40% of the table viewport for the description column, and exposes each complete
description as a hover tooltip. When a parent Actual value equals a sub-bitfield or enum
value, that child row's Sub-bitfield and Desc cells are highlighted in green.

The default Registers shortcuts are configured in `settings.conf`: `F9` toggles the
Registers panel and `Ctrl+Shift+F` opens the panel and focuses its Search field.
Pressing `Escape` anywhere inside the Registers panel returns focus to the calculator
input without closing or resetting the selected register details.
The Derivative field is a read-only display listing every derivative configured for
the selected SoC (e.g. `cyt4bb, cyt4bf`); registers are filtered by SoC only.
BitLoupe restores the most recently selected SoC, search expression,
register, bitfield/sub-bitfield row, and Panel #1/Panel #2 splitter position at the
next startup. Panel #2 begins with Reg, Addr, and Ref page fields; SoC selection and
the derivative display remain in Panel #1.

## Building the manual
Building the HTML manual is normally not necessary because a prebuilt copy is included
with the BitLoupe source. For more information, see the [manual's README](doc/src/README.md).

## Contributing
- Report bugs or request features in the
  [issue tracker](https://bitbucket.org/heldercorreia/bitloupe/issues).
- Add or improve a [translation](https://www.transifex.com/projects/p/bitloupe/).
- Send a message to the [forum](https://groups.google.com/group/bitloupe).
- Follow the news on the [blog](http://bitloupe.blogspot.com).

## License
This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with this program; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
MA 02110-1301, USA.

