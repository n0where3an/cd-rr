# cd-rr

CD-R eRRoR coRRectoR: error correction program for CD, based on 16-bit
Solomon-Reed code.

- **rrenc** reads all files that are to be written to the disc (including XCD
  images), computes redundant data with a 16-bit Solomon-Reed code and
  prepares a paramfile for mode2cdmaker (`m2cdm/`).
- **rrdec** reads all files from the disc, including the redundant data,
  writes them to disk and recomputes damaged sectors, putting them back in
  place.

The project continues the discussions on the
[doom9.org](http://forum.doom9.org/showthread.php?t=93048) and
[sharereactor.ru](http://forum.sharereactor.ru/showthread.php?t=59777) forums.
Project page: <https://sourceforge.net/projects/cd-rr/>.

Command-line tools for Linux and Windows; interface and documentation in
English and Russian. Documentation (mostly in Russian) is in [doc/](doc/),
usage examples are in [example/](example/).

## Build

Requires CMake >= 3.16 and a C99 compiler.

```sh
cmake -S . -B build
cmake --build build              # build/rrenc, build/rrdec
ctest --test-dir build           # end-to-end protect/damage/recover test
cmake --install build --prefix /usr/local
```

Options: `-DCMAKE_BUILD_TYPE=Debug`, `-DCDRR_SANITIZE=ON` (ASan + UBSan),
`-DCDRR_WERROR=ON` (warnings as errors; recommended for development and CI).

The bundled `m2cdm/` (Mode2 CD Maker, used for XCD images) has its own
makefiles and is not part of the CMake build.

## License

cd-rr is licensed under the GNU Affero General Public License v3.0 or later,
see [LICENSE](LICENSE).

Third-party code keeps its own license:

| Path | License |
| ---- | ------- |
| `m2cdm/` | GPL-2.0-or-later, see [m2cdm/COPYING.txt](m2cdm/COPYING.txt) |
| `getopt.c`, `getopt1.c`, `getopt.h` (Windows build only) | LGPL-2.1-or-later (GNU C Library) |
