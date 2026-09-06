![Logo|128](logo-small.png)

---

Jinx is a "meta-build-system" designed for bootstrapping operating systems. Primarily used by hobby OSDev projects such as [Gloire](https://codeberg.org/Ironclad/Gloire) to aid in the creation of an image distribution. Jinx utilises a lightweight Debian "container" to provide a consistent build environment on every host system.

## Table of Contents

Links to different points in this documentation:
- [Acquisition](<#acquisition>)
- [Basic Project Structure](<#basic-project-structure>)
- [Commands](<#commands>)
	- [`init`](<#init>)
	- [`update`](<#update>)
	- [`build`](<#build>)
	- [`rebuild`](<#rebuild>)
	- [`revbump`](<#revbump>)
	- [`regenerate`](<#regenerate>)
	- [`install`](<#install>)
	- [`dry-run`](<#dry-run>)
	- [`download`](<#download>)
	- [`run-in`](<#run-in>)
	- [`rebuild-cache`](<#rebuild-cache>)
- [Rebuild Behavior](<#rebuild-behavior>)
- [Jinxfile](<#jinxfile>)
- [Environment Variables](<#environment-variables>)
- [Recipes](<#recipes>)
	- [Properties](<#properties>)
		- [`version`](<#version>)
		- [`revision`](<#revision>)
		- [`source_dir`](<#source_dir>)
		- [`from_source`](<#from_source>)
		- [`from_host_source`](<#from_host_source>)
		- [`tarball_url`](<#tarball_url>)
		- [`tarball_blake2b`](<#tarball_blake2b>)
		- [`tarball_sha256`](<#tarball_sha256>)
		- [`tarball_sha512`](<#tarball_sha512>)
		- [`zip_url`](<#zip_url>)
		- [`zip_blake2b`](<#zip_blake2b>)
		- [`zip_sha256`](<#zip_sha256>)
		- [`zip_sha512`](<#zip_sha512>)
		- [`git_url`](<#git_url>)
		- [`commit`](<#commit>)
		- [`shallow`](<#shallow>)
		- [`deps`](<#deps>)
		- [`builddeps`](<#builddeps>)
		- [`binpkgdeps`](<#binpkgdeps>)
		- [`hostdeps`](<#hostdeps>)
		- [`hostrundeps`](<#hostrundeps>)
		- [`imagedeps`](<#imagedeps>)
		- [`imagerundeps`](<#imagerundeps>)
		- [`allow_network`](<#allow_network>)
		- [`cross_compile`](<#cross_compile>)
		- [`bootstrap_pkg`](<#bootstrap_pkg>)
		- [`clean_workdirs`](<#clean_workdirs>)
		- [`conf_files`](<#conf_files>)
		- [`source_*`](<#source_>)
	- [Functions](<#functions>)
		- [`early_prepare()`](<#early_prepare>)
		- [`prepare()`](<#prepare>)
		- [`configure()`](<#configure>)
		- [`build()`](<#build-1>)
		- [`package()`](<#package>)
	- [Provided Variables](<#provided-variables>)
	- [Host Recipes](<#host-recipes>)
	- [Patches](<#patches>)

## Acquisition

Ensure the following prerequisites have been acquired:
- `bash` (the `jinx` script itself is written in bash).
- `awk`.
- `find` and `xargs` from a `findutils` package.
- `git`.
- `grep`.
- `gzip`.
- `perl`.
- `sed`.
- `tar`.
- `wget`.
- `xzcat` from an `xz` package, or equivalent.
- `zstd`.
- `sha256sum` (or `sha256`) and `chroot` from a `coreutils` package, or equivalent.
- `free` from a `procps` package, or equivalent.
- `unshare`, `flock` and `mount` from a `util-linux` package, or equivalent.

`perl` and `xzcat` are not used by Jinx itself: they are what the `debootstrap` Jinx installs on its first run needs in order to resolve and decompress the `.deb`s of the base container image. It unpacks them with `ar`, but Jinx ships a stand-in for the two flags it uses and pins `debootstrap` to that, so `binutils` is not needed.

>[!warning]
>It is ***imperative*** that jinx be run under Linux, as the container environment relies on non-POSIX Linux features (user namespaces, mount namespaces, `chroot`). You ***will*** run into issues on other systems.

Download the `jinx` executable script from https://github.com/Mintsuki/Jinx. Ensure that it is marked as executable with `chmod +x jinx`. Optionally, install it system-wide with `make install` (it accepts the standard `PREFIX` and `DESTDIR` variables). Finally, verify that it is functional with `jinx help` to display the help output.

## Basic Project Structure

Jinx enforces **out-of-tree** builds. There are two distinct directories involved:

1. The **source directory** holds the project's recipes (each with their own patches) and the `Jinxfile`.
2. The **build directory** is initialised by `jinx init` and is where build artifacts, packages, and the `.jinx-parameters` file live.

A typical source directory looks as follows:

```
example-source/
|-- Jinxfile
|-- host-recipes/
|   \-- <name>/
|       |-- recipe
|       \-- patches/  # optional
\-- recipes/
    \-- <name>/
        |-- recipe
        \-- patches/  # optional
```

- `Jinxfile` -> Jinx configuration file. Place required version/snapshot variables, optional global variables and helper functions referenced by recipes here. See [Jinxfile](<#jinxfile>).

- `host-recipes/` -> Recipes that produce results destined for usage on the host system. Typically for cross-compiler toolchain building. See [Host Recipes](<#host-recipes>).

- `recipes/` -> Generic recipes for most purposes. Built inside the container, and may also act as a source provider for other recipes (host or non-host) through the [`from_source`](<#from_source>) property. See [Recipes](<#recipes>).

Each recipe lives in its own directory under `recipes/` or `host-recipes/`. The directory contains a file named `recipe` (the shell-sourced recipe contents) and, optionally, a `patches/` subdirectory holding patches that should be applied to the recipe's sources. See [Patches](<#patches>).

After running `jinx init`, the build directory has the following layout (some entries are created lazily as builds happen):

```
example-build/
|-- .jinx-parameters
|-- builds/        # configure/build/package output for normal recipes
|-- host-builds/   # configure/build/package output for host recipes
|-- host-pkgs/     # host XBPS package files and the host XBPS repository index
\-- pkgs/          # XBPS package files and the XBPS repository index
```

Additionally, the **source directory** will gain a few extra entries during use:

```
example-source/
|-- .jinx-cache/    # XBPS, debootstrap, container image cache, build lock
|-- sources/        # downloaded/cloned sources for recipes/
\-- host-sources/   # downloaded/cloned sources for host-recipes/
```

Normal recipes and host recipes get fully separate source trees, mirroring the `recipes/` vs `host-recipes/` split. A recipe named `foo` in `recipes/` extracts to `sources/foo/`, while a recipe with the same name in `host-recipes/` extracts to `host-sources/foo/` - so the two can have independent inline sources without colliding.

Within `sources/` (and identically within `host-sources/`), every source recipe owns a small family of entries:

```
sources/
|-- <name>/               # the tree builds are run against
|-- <name>-clean/         # snapshot taken after the static patches, before the working patch
|-- <name>-workdir/       # editable copy; `jinx regen` diffs it against <name>-clean/
|-- <name>.version        # version the tree was fetched for
|-- <name>.revision       # revision a normal recipe last prepared the tree for
|-- <name>.host-revision  # revision a host recipe last prepared the tree for
|-- <name>.patched        # stamp: patches have been applied
\-- <name>.prepared       # stamp: prepare() has been run
```

The stamp files are what make source preparation happen exactly once: a mismatching `<name>.version` (or a bumped `revision`) wipes the whole family and forces a fresh fetch, patch and prepare. A local package (one using [`source_dir`](<#source_dir>)) keeps its tree wherever the recipe points, so only the stamp files show up here - Jinx never patches, copies or deletes the tree itself.

>[!note]
>In-tree builds are explicitly forbidden. Running `jinx init` from a directory that already contains a `Jinxfile` will fail.

>[!tip]
>Multiple build directories may share the same source directory. This is convenient for keeping side-by-side builds for different architectures, configurations, or experiments. They cannot be *driven* side by side, though: they share `sources/` and the cache, and Jinx serialises them with a lock (see [Commands](<#commands>)).

## Commands

A brief summary for each command can be found within the usage output of `jinx help`:

```
usage: jinx <command> [args...]

   help|--help        Displays this message
   version|--version  Prints the version
   init               Initialises a build directory
   update             Rebuild outdated package(s); use '-b' to also build never-built ones
   build              Builds package(s), does incremental builds
   rebuild            Rebuilds package(s)
   revbump            Bumps the revision of all (transitive) dependents of package(s)
   regenerate|regen   Regenerates patch for package(s) and re-runs prepare step
   install            Installs package(s) (use '-f' to reinstall)
   dry-run            Prints all packages that need to be built in order (space separated)
   download           Downloads pre-built package(s) from remote repo
   run-in             Runs a command in a container prepared with a recipe's hostdeps
   rebuild-cache      Rebuilds Jinx cache

example: jinx update '*'
```

>[!tip]
>For all commands that take recipe names, an argument of `'*'` (or any shell glob) can be used in place of an explicit name; Jinx expands it against the relevant recipes directory. This is great for "full" distributions.

>[!tip]
>For commands that operate on recipes ([`build`](<#build>), [`rebuild`](<#rebuild>), [`revbump`](<#revbump>), [`regenerate`](<#regenerate>), [`update`](<#update>), [`dry-run`](<#dry-run>), [`download`](<#download>), [`run-in`](<#run-in>)), prefix a name with `host:` to refer to a host recipe instead of a normal one. For example, `jinx build host:gcc` builds `host-recipes/gcc/`, while `jinx build gcc` builds `recipes/gcc/`. Glob expansion respects the prefix: `jinx build 'host:*'` expands against `host-recipes/`. The `install` command, by contrast, only operates on normal recipes.

>[!note]
>Every command except `help`, `version`, `init` and [`dry-run`](<#dry-run>) holds an exclusive lock on `<cache-dir>/jinx.lock` for the whole run, and refuses to start while another Jinx holds it. Jinx shares `sources/`, `builds/` and `pkgs/` between runs and is not concurrency-safe - a second run would delete the first one's build tree. Since the cache directory defaults to `<source-dir>/.jinx-cache`, this also serialises build directories that share a source directory, which is exactly what their shared `sources/` requires.

#### `init`

Initialises a build directory. Jinx autodetects the roles of the current directory and the single `<dir>` argument from where a `Jinxfile` is found:

- If `<dir>` contains a `Jinxfile`, then `<dir>` is the **source** directory and the **current directory** is the build directory (which must not already be initialised, nor itself be a source directory - in-tree builds are rejected).
- Otherwise, if the **current directory** contains a `Jinxfile`, then the current directory is the **source** and `<dir>` is a **new** build directory to create (it must not already exist; intermediate parents are created as needed).

If neither location has a `Jinxfile`, `init` errors out. Any further arguments of the form `KEY=VALUE` are written into the build directory's `.jinx-parameters` prefixed with `JINX_`.

Usage:

```sh
jinx init <dir> [KEY=VALUE]...
```

Examples:

```sh
# <dir> is the source; the current directory becomes the build directory
mkdir build && cd build
../source/jinx init ../source ARCH=x86_64

# the current directory is the source; <dir> is created as the build directory
cd source
./jinx init ../build ARCH=x86_64
```

This produces a `.jinx-parameters` file in the build directory containing at minimum `JINX_INIT_MAJOR_VER`, `JINX_SOURCE_DIR`, and `JINX_ARCH` (which defaults to `$(uname -m)` if not overridden via `ARCH=...`). Any extra arguments become `JINX_<KEY>="<VALUE>"`. In the first form `JINX_SOURCE_DIR` is recorded exactly as passed (a relative path stays relative to the build directory, resolved when Jinx runs); in the second form it is recorded as an absolute path.

`JINX_INIT_MAJOR_VER` records the major version of the `jinx` that initialised the directory. Every subsequent `jinx` invocation refuses to operate on the build directory if this value is missing or doesn't match the running `jinx`'s major version. Recovery is to delete the build directory and run `jinx init` again to start over; there is no in-place reinit because previously built artifacts may be incompatible too.

>[!note]
>To "deinitialise" a build directory, simply remove `.jinx-parameters`. There is no separate `deinit` command.

#### `update`

```sh
jinx update [-b] [package(s)...]
```

Rebuilds the specified package(s) when they are **out of date**. A package is considered out of date when `pkgs/` already holds an XBPS file for that exact `name` but the filename does not match the recipe's current `version_revision`. The name has to match in full: a built `foo-bar` does not make `foo` count as previously built. Packages that have never been built are *skipped* by default - `update` is for keeping an existing set of built packages in sync with the recipes, not for introducing new ones.

The `-b` flag (mnemonic: "build") restores the older behavior: never-built packages are also built. Transitive dependencies needed to satisfy an outdated target are always built regardless of `-b`, since the target's build would otherwise fail.

If invoked with no recipe arguments (other than `-b`), behaves as `update [-b] '*'`. Accepts the `host:` prefix (e.g. `jinx update 'host:*'`) to update host recipes, which are tracked by their own `host-pkgs/<name>-<version>_<revision>.<host-arch>.xbps` file exactly like normal recipes; the default `'*'` covers normal recipes only (host recipes are still pulled in transitively as host dependencies).

#### `build`

Followed by as many arguments as recipes to build. By default each argument refers to a normal recipe in `recipes/`; prefix a name with `host:` (e.g. `host:gcc`) to refer to a host recipe in `host-recipes/`. `build` will *not* attempt to build host recipes unless they are specified explicitly with `host:` or pulled in as a dependency of a to-be-built recipe.

Builds are **incremental**: the recipe's build directory is preserved across invocations whenever a matching XBPS package file already exists, so `make` (or whichever build tool the recipe uses) can do its own incremental work. Unlike [`update`](<#update>), `build` will re-invoke the recipe's `build()` and `package()` functions even when nothing has changed.

#### `rebuild`

Forces a rebuild of the specified package(s) by removing the build directory first. This causes the recipe's [`configure()`](<#configure>) function to be re-run on the next build. Accepts the `host:` prefix to rebuild a host recipe.

#### `revbump`

Bumps the [`revision`](<#revision>) of every recipe that depends, directly or transitively, on the given package(s), so that a subsequent [`update`](<#update>) rebuilds them. Accepts globs and the `host:` prefix like the other package commands. The dependency graph spans both namespaces (`deps`/`builddeps` resolve in `recipes/`, `hostdeps`/`hostrundeps` in `host-recipes/`), so a single `revbump` reaches dependents in both `recipes/` and `host-recipes/` regardless of whether the target is a normal or a host recipe.

The given package(s) are treated as the changed input and are themselves left untouched - only their dependents are bumped. Recipe files are edited in place, preserving the existing indentation and quoting of the `revision=` line; a recipe that does not have exactly one literal `revision=` line is reported rather than guessed at, and a target with no dependents is a no-op.

This automates step 2 of the [reverse-dependency workflow](<#reverse-dependencies-are-not-automatically-rebuilt>) for the entire transitive closure: bump the library, `jinx revbump <library>`, then `jinx update '*'`.

#### `regenerate`

- Alias: `regen`

Regenerates `<dir>/<name>/patches/jinx-working-patch.patch` from any in-place modifications made to `sources/<name>-workdir/` (`host-sources/<name>-workdir/` for a host recipe), then re-runs the [`prepare()`](<#prepare>) step. Use this when iterating on patches: edit the working copy, run `regen`, then `rebuild` the recipe. Accepts the `host:` prefix (e.g. `jinx regen host:gcc`) to regen a host recipe.

The patch is the diff between `<name>-clean/` and `<name>-workdir/`, and the workdir then replaces the tree that builds run against. If the two no longer differ, `regen` **deletes** an existing `jinx-working-patch.patch` instead (and removes `patches/` if that leaves it empty) - this is how a working patch is dropped once its changes have landed upstream or graduated into a static patch.

The recipe's sources must have been fetched and patched by an earlier build; `regen` on a recipe that was never built fails with `cannot regenerate non-built package`.

Only valid on source recipes (i.e. recipes that declare their own sources); attempting to `regen` a recipe that uses [`from_source`](<#from_source>) or [`from_host_source`](<#from_host_source>) is an error - the error message points at the right command for regenerating the actual source recipe.

#### `install`

```sh
jinx install [-f] <sysroot> <package(s)>
```

Installs the specified package(s) into the given system root directory. Each package is built first if it has not been built yet (unless Jinx is invoked as root, in which case it errors out instead of attempting a build).

The `-f` flag **forces** the installation, removing any pre-existing version of the package from the sysroot beforehand. After installation, Jinx checks for file conflicts between the installed packages; if any duplicate paths are detected, it lists them and exits with an error.

>[!note]
>`install` is the only command allowed to run as root, since populating a sysroot may require root privileges depending on file ownership/permissions.

>[!warning]
>The `install` command requires a "sysroot" directory as the first positional argument before the package list. Example: `jinx install sysroot/ base`.

#### `dry-run`

Prints, on a single line space-separated, the topological order of packages that would be built to satisfy the given target(s) (or `'*'` - every normal recipe - if no target is given). Already-built packages are omitted.

Normal and host packages share a **single** topological order and interleave wherever the dependencies demand it; host ones are printed with a `host:` prefix, and `host:` targets are accepted as roots. The graph spans both namespaces exactly like the builds do: `hostdeps`, `hostrundeps` and `source_hostdeps` are edges into `host-recipes/`, while `deps`, `builddeps` and `source_deps` are edges into `recipes/`, and either kind of recipe may have either kind of edge. [`binpkgdeps`](<#binpkgdeps>) are deliberately *not* edges here - not creating a build-order edge is the entire point of that property.

`dry-run` builds nothing and writes nothing, and it is the one recipe command that does not take the build lock, so it can be run while another Jinx is busy in the same directory.

This is useful for scripting (for example, building one package at a time in CI to keep memory low) and for quickly inspecting what an `update` will do without actually building anything.

#### `download`

```sh
jinx download <package(s)>
```

Fetches pre-built XBPS files for the specified package(s) and their transitive dependencies from the URL set in `JINX_REPO_URL` (Jinxfile variable). Only files that are missing from `pkgs/` are fetched; when everything the targets need is already there, Jinx says so and does nothing. Each downloaded file's SHA256 is verified against the repository's `index.plist`, and the local repo index is updated.

Accepts the `host:` prefix to fetch host packages instead: `host:` targets are resolved against `host-recipes/`, fetched from `JINX_HOST_REPO_URL` (a separate Jinxfile variable), and dropped into `host-pkgs/` (and walked through `hostdeps`/`hostrundeps`/`source_hostdeps` rather than `deps`/`builddeps`/`source_deps`/`binpkgdeps`). Normal and `host:` targets can be mixed in a single invocation; each group uses its own repo URL and its own arch repodata. Note that the transitive walk stays within one namespace: `jinx download foo` does **not** automatically also fetch foo's `hostdeps` - run `jinx download 'host:*'` (or list specific host packages) to populate `host-pkgs/`.

The two URLs are kept separate so that target and host repos can coexist even in native builds (where `JINX_ARCH == $(uname -m)` and a single flat directory couldn't disambiguate `${arch}-repodata` files for the two namespaces). A typical layout is to publish `<repo-root>/target/` and `<repo-root>/host/` and point each variable at the appropriate subdirectory.

Errors out if `JINX_REPO_URL` is not set when normal packages are requested, or if `JINX_HOST_REPO_URL` is not set when `host:` packages are requested.

#### `run-in`

```sh
jinx run-in <recipe> <command> [args...]
```

Prepares a container as if it were going to build `<recipe>` (sysroot populated with `deps`, host packages from `hostdeps`/`hostrundeps`, Debian packages from `imagedeps`), then runs the given command inside it from the recipe's build directory. Network access is enabled. Accepts the `host:` prefix (e.g. `jinx run-in host:gcc bash`) to drop into a host recipe's container instead - the recipe is looked up in `host-recipes/` and the command runs from `host-builds/<name>/`, always through the cross-compile flow.

The recipe's sources are fetched, patched and prepared first if that has not happened yet, and the build directory is created if it is missing, so `run-in` works on a recipe that has never been built. The recipe argument is glob-expanded like everywhere else, in which case the command is run once per match.

Useful for interactive debugging of build failures, running utilities against a populated sysroot, or one-off tasks inside the same environment a recipe sees.

#### `rebuild-cache`

Purges and re-creates `.jinx-cache/`. This re-downloads XBPS, re-runs `debootstrap`, and rebuilds the base container image. Run this if the cache becomes corrupt; otherwise, Jinx automatically refreshes the cache when its version, the Debian snapshot, or the base packages list change.

## Rebuild Behavior

Jinx identifies built packages purely by their XBPS filename, `<name>-<version>_<revision>.<arch>.xbps`. When this file is present in the build directory's `pkgs/`, the recipe is considered already built; when it is missing (because `version` was bumped, `revision` was bumped, or the file was deleted), Jinx rebuilds the recipe.

### What triggers a rebuild

A recipe is rebuilt under any of these conditions:

1. You explicitly run [`build`](<#build>) or [`rebuild`](<#rebuild>) on it. `build` always re-runs the recipe's `build()` + `package()` stages (skipping `configure()` if the build directory still exists from a previous run); `rebuild` additionally removes the build directory first so `configure()` reruns from scratch.
2. You run [`update`](<#update>) (or `update '*'`) and the recipe's current `name-version_revision.xbps` is missing.
3. The recipe's `version` or `revision` changed since it was last built. The expected XBPS filename no longer matches any existing artifact, so the file appears "missing" - `update` will then rebuild it, and `do_pkg`'s internal `.revision` marker mismatch will additionally clean the unpacked source tree, forcing a fresh fetch/patch/prepare for the next build (the `version` axis is handled separately by `.version`). Host recipes behave identically: `do_host_pkg` uses a `.host-revision` marker and the `host-pkgs/<name>-<version>_<revision>.<host-arch>.xbps` filename in exactly the same way.

### Dependency walking

All build-driving commands walk **downward** through the dependency tree, rebuilding anything whose XBPS file is missing along the way - host deps use their own `host-pkgs/<name>-<version>_<revision>.<host-arch>.xbps` file, just as normal recipes use `pkgs/`:

- [`build <pkg>`](<#build>) / [`rebuild <pkg>`](<#rebuild>) iterate each direct dep and host-dep of `<pkg>`, recursing so that any missing artifact in the closure is rebuilt before `<pkg>` itself.
- [`update <pkg>`](<#update>) performs the same walk, gated on `<pkg>` itself being missing; the walk uses one topological sort over `<pkg>`'s entire transitive closure.
- [`dry-run`](<#dry-run>) performs the same walk in preview mode and prints what *would* be rebuilt, in build order.

Topological sorting is required for correctness: before any recipe builds, every transitive dep of it must already have its XBPS file in `pkgs/` (or, for host deps, `host-pkgs/`), because `prepare_container` uses `xbps-install` to populate the build sysroot from `pkgs/` and the container's `/usr/local` from `host-pkgs/`. Sorting deps-first ensures that is always the case.

### Reverse dependencies are *not* automatically rebuilt

If you bump a library's `version` or `revision`, Jinx will rebuild that library, but **not** packages that depend on it. The dependents' XBPS files still exist with their original versions, so all the commands above (including `update '*'`) consider them already built. Jinx does not track which artifacts were built against which others. This applies symmetrically to host recipes (which are now ordinary XBPS packages too): bumping a host recipe does not rebuild its host-dependents.

When a library's ABI/soname changes and its dependents need recompiling, the conventional workflow (matching xbps-src and PKGBUILD-style systems) is:

1. Bump the affected library's `version` or `revision`.
2. Bump `revision=` on every affected reverse-dependency recipe to mark them as needing a rebuild too. [`jinx revbump <library>`](<#revbump>) does this automatically for the whole transitive closure.
3. Run `jinx update '*'`. The topological sort will rebuild the library first, then each dependent in the right order.

If you only do step 1, dependents keep their existing XBPS files until you delete them manually, run `jinx rebuild <dep>` for each, or bump their `revision`.

## Jinxfile

When building **any** recipe, Jinx sources the `Jinxfile` from the source directory. Variables and functions defined within this file will be visible to recipes, allowing the definition of default compiler flags, helper functions, and so on. The `Jinxfile` is sourced by bash, so any valid bash syntax is accepted (POSIX shell syntax is a subset, so older POSIX-style Jinxfiles continue to work unchanged).

A *bare minimum* `Jinxfile` is as follows:

```sh
#! /bin/sh

# Minimum Jinxfile for Jinx 0.10.x.

# *Required.* Ensures the project's expected major version matches the running jinx.
JINX_MAJOR_VER=0.10

# *Required.* Pins the Debian snapshot used for the container's base image.
# See https://snapshot.debian.org/ for available snapshot identifiers.
JINX_DEBIAN_SNAPSHOT=20251101T000000Z

# Any further logic, helper functions, or variable definitions will *also* be
# visible to every recipe.
```

The full set of Jinx-recognised variables in a `Jinxfile`:

| Variable                | Required | Purpose                                                                                                       |
| ----------------------- | -------- | ------------------------------------------------------------------------------------------------------------- |
| `JINX_MAJOR_VER`        | yes      | Major version of Jinx the project targets. Must match the running `jinx` binary.                              |
| `JINX_DEBIAN_SNAPSHOT`  | yes      | Debian snapshot timestamp used to bootstrap the container's base image.                                       |
| `JINX_BASE_PACKAGES`    | no       | Extra Debian packages to install into the base container image (added to Jinx's default set).                 |
| `JINX_CMAKE_PLATFORM`   | no       | Path (relative to the source directory) of a CMake platform file to drop into the container's CMake modules.  |
| `JINX_REPO_URL`         | no       | URL for the [`download`](<#download>) command to fetch pre-built target packages from.                        |
| `JINX_HOST_REPO_URL`    | no       | URL for the [`download`](<#download>) command to fetch pre-built host packages from (when `host:` is used).   |

`JINX_ARCH` is **not** read from the `Jinxfile`; it is set in `.jinx-parameters` (defaulting to `$(uname -m)`, overridable via `jinx init <src> ARCH=...`).

The base container image is a `minbase` Debian `sid` snapshot with `locales` and the following packages installed: `bash`, `bzip2`, `ca-certificates`, `curl`, `diffutils`, `findutils`, `gawk`, `git`, `grep`, `gzip`, `lzip`, `netbase`, `patch`, `procps`, `sed`, `tar`, `unzip`, `xz-utils`, `zstd`. Anything else a recipe needs at build time belongs in its [`imagedeps`](<#imagedeps>); `JINX_BASE_PACKAGES` is for tools that every recipe should have, including during source preparation, where per-recipe `imagedeps` do not apply and [`source_imagedeps`](<#source_>) is the only other way in.

>[!note]
>`JINX_CMAKE_PLATFORM` copies the file into `usr/share/cmake-*/Modules/Platform/` of the base image, a directory that only exists once CMake is installed there - list `cmake` in `JINX_BASE_PACKAGES` alongside it.

>[!note]
>Helper functions referenced by recipes will be run inside the container, and will *not* run on the host. The `Jinxfile` itself, however, is sourced both on the host and inside the container on every Jinx invocation, so any *top-level* code (outside of function bodies) runs in both contexts - keep top-level code portable and put host- or container-specific work inside helper functions.

## Environment Variables

Several environment variables can be set when invoking `jinx` to alter its behaviour:

- `JINX_PARALLELISM`: Override the `parallelism` value passed to recipes. By default, Jinx auto-tunes this before every container step from the number of online CPUs and the RAM available at that moment (roughly one job per ~2 GiB available, capped by `nproc`), so a step starting while the machine is under memory pressure gets fewer jobs.
- `JINX_CACHE_DIR`: Override the cache location. Defaults to `<source-dir>/.jinx-cache`. The build lock lives here, so runs sharing a cache directory are serialised against each other.
- `JINX_CLEAN_WORKDIRS`: When set to `yes`, Jinx removes the recipe's build directory and downloaded sources after a successful package build, on a per-recipe basis (a recipe may opt out via [`clean_workdirs=no`](<#clean_workdirs>)).
- `JINX_NATIVE_MODE`: When set to `yes`, Jinx mounts the in-progress sysroot directly as the container root for every recipe that leaves [`cross_compile`](<#cross_compile>) unset. This is intended for native (host arch == target arch) builds. Such a recipe gets no Debian image (so its [`imagedeps`](<#imagedeps>) do not apply), no host packages under `/usr/local` (its [`hostdeps`](<#hostdeps>) are still built, just not installed), and no `/sysroot` - everything it uses comes from the sysroot that *is* its container root. Host recipes and the source-preparation stages always take the regular cross flow regardless of this variable.
- `JINX_NATIVE_LANG`: When `JINX_NATIVE_MODE=yes` and the container is the sysroot, controls the value of `LANG` inside the container. Defaults to `C`.

>[!note]
>`TERM` and `COLORTERM` are forwarded into the build container when they are set, so build tools keep their coloured output.

## Recipes

In the Jinx build system, packages are specified by shell scripts called "recipes". There are several types of recipes, but all follow the same sort of system.

### Properties

Defined within the recipe, there are a number of properties that determine how Jinx will handle building the recipe. The full list is below.

>[!note]
>"Properties", as they are referred to in this documentation, are just shell variables that Jinx makes use of. Recipes are sourced by bash, so any valid bash syntax for defining these variables will be accepted (POSIX shell syntax is a subset and continues to work unchanged).

#### `version`

- **Required.**

Specifies the version of this recipe. This **should** be set to the version number of the source this package is for (e.g. `version=2.69` for `autoconf-2.69`). Jinx uses the `version` property internally to determine whether to rebuild a package after a version bump (this is why bumping `version` (or `revision`) automatically invalidates a previous build).

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
version=2.4.0
# ...
```

#### `revision`

- **Required.**

Used together with `version` to identify the built XBPS package file (`<name>-<version>_<revision>.<arch>.xbps`). Bump `revision` when the recipe's build logic or patches change without a version change, so that the package is rebuilt and reinstalled.

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
revision=1
# ...
```

#### `source_dir`

- **Optional.**
- Mutually exclusive with [`tarball_url`](<#tarball_url>) (setting both is an error).
- May be combined with [`git_url`](<#git_url>) to enable *managed-clone mode* (see below).

Tells Jinx to use the source directory specified in the recipe (interpreted relative to the source directory containing `Jinxfile`), marking the recipe as a "local package". This is useful for building code that lives in the same project as the recipes.

>[!warning]
>Files *outside* of the source directory will be **inaccessible** within the build container. Place all sources for local packages somewhere inside the source directory.

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
source_dir="test/"
# ...
```

>[!tip]
>This works particularly well for building the kernel from a project directory, especially for OSDev projects.
>
>For example:
>
>```sh
>#! /bin/sh
># recipes/kernel/recipe
>
>version=0
>revision=1
>source_dir="kernel" # Our kernel project is within the `kernel/` directory.
># ...
>```

##### Managed clones (`source_dir` + `git_url`)

If a recipe sets **both** `source_dir` and [`git_url`](<#git_url>), Jinx treats it as a *managed clone*. This is intended for repositories maintained by the same people working on the distribution: Jinx clones the repository **once** into `source_dir`, then leaves it entirely alone and uses it exactly like a normal `source_dir` local package (in place, never patched, never auto-cleaned).

The behavior is:

- On the first build, if `source_dir` does not yet exist, Jinx performs a **full** clone of `git_url` into it and checks out [`commit`](<#commit>) (which remains **required**, as the reproducible starting point). [`shallow`](<#shallow>) has no effect here - managed clones are always full clones so you have history to work with.
- On every later build, an **existing** `source_dir` is used as-is. Jinx never re-clones it, never checks out `commit` again, never patches it, and never deletes it (neither `clean_sources` nor [`JINX_CLEAN_WORKDIRS`](<#environment-variables>) touch a local package's tree). It is your working tree to develop in - commit, branch, and push with `git` directly.
- To start over from a fresh clone, delete the `source_dir` directory; the next build re-clones it.

Because it is a local package, a managed clone cannot carry a `patches/` directory's effect (patches are skipped, as with any local package) - manage changes with git instead. This works for both normal recipes and host recipes.

Example:

```sh
#! /bin/sh
# recipes/kernel/recipe

version=1.0.0
revision=1
source_dir="kernel"                                   # cloned-into / worked-in here
git_url="https://github.com/example/kernel.git"       # cloned from here, once
commit="28257019ce04f784337cb9c3125abb4d02cef14d"     # initial checkout point
# ...
```

#### `from_source`

- **Optional**.
- Mutually exclusive with [`from_host_source`](<#from_host_source>).

Specifies another recipe in `recipes/` to draw build sources from. The referenced recipe provides the tarball/git URL, checksums, `version`, any [`early_prepare()`](<#early_prepare>)/[`prepare()`](<#prepare>) callbacks, and the `source_*` properties that govern the source-preparation environment. Patches likewise belong to the source recipe and are applied automatically - a consumer recipe that sets `from_source` is not permitted to have its own `patches/` directory.

The consumer's resulting XBPS filename uses the **source recipe's `version`** combined with the **consumer's own `revision`**: e.g. `recipes/foo` with `from_source="foo-shared"` and `revision=3`, where `foo-shared` declares `version=1.2`, builds as `foo-1.2_3.<arch>.xbps`. Don't declare your own `version` on a consumer recipe - it would be ignored.

This works for both normal recipes (in `recipes/`) and host recipes (in `host-recipes/`): in either case, the lookup goes to `recipes/`.

When neither `from_source` nor `from_host_source` is set, a recipe provides its own source information inline - `tarball_url`, `git_url`, `source_dir`, etc., declared directly in the recipe itself.

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
from_source="test-shared-sources" # Refers to recipes/test-shared-sources.
# ...
```

#### `from_host_source`

- **Optional**.
- Mutually exclusive with [`from_source`](<#from_source>).

Like [`from_source`](<#from_source>) but resolves against `host-recipes/` instead of `recipes/`. Use this when several host recipes share a common source tree - point each of them at a single `host-recipes/<name>` that declares the tarball/git URL and `early_prepare()`/`prepare()` callbacks. As with `from_source`, the consumer recipe cannot carry its own `patches/` directory; patches live with the source.

Example:

```sh
#! /bin/sh
# host-recipes/bootstrap-gcc/recipe

# ...
from_host_source="gcc-host" # Refers to host-recipes/gcc-host.
# ...
```

#### `tarball_url`

- **Optional**.

Specifies a URL to download a tarball from, to use as the source for this recipe. The archive is downloaded into `sources/`, verified against a checksum, unpacked with `tar` and then deleted. Mutually exclusive with [`git_url`](<#git_url>), [`source_dir`](<#source_dir>) and [`zip_url`](<#zip_url>) - for `.zip` archives, use [`zip_url`](<#zip_url>).

>[!warning]
>A method of checksum validation **must** be specified. This can be done by setting [`tarball_blake2b`](<#tarball_blake2b>), [`tarball_sha256`](<#tarball_sha256>), or [`tarball_sha512`](<#tarball_sha512>). You can set the value of any of these to `"?"` and Jinx will fill the recipe in for you on first download.

>[!tip]
>The [`version`](<#version>) property can be embedded within this property for the sake of convenience.
>
>For example:
>```sh
>#! /bin/sh
># recipes/xtrans/recipe
>
>version=1.6.0
>tarball_url="https://www.x.org/archive/individual/lib/xtrans-${version}.tar.gz"
># ...
>```

>[!note]
>An archive that fails its checksum is left behind in `sources/` and reused as-is on the next run. If upstream really did replace the file, delete the stale archive by hand before retrying.

#### `tarball_blake2b`

- **Required** for [`tarball_url`](<#tarball_url>) checksum (one of `tarball_blake2b`, `tarball_sha256`, `tarball_sha512`).

Specifies a BLAKE2B checksum for verifying the tarball. Setting it to `"?"` makes Jinx auto-fill the value into the recipe on first download.

Example:

```sh
#! /bin/sh
# recipes/xtrans/recipe

# ...
tarball_blake2b="446035fb78ec796c1534f36dc687b40fbe6227d47a623039314117a85cc4b3e37971934790932e46a6dc362de70dfb58ccd1fae43518461789ce8854e27adba8"
# ...
```

#### `tarball_sha256`

- **Required** for [`tarball_url`](<#tarball_url>) checksum (one of `tarball_blake2b`, `tarball_sha256`, `tarball_sha512`).

Specifies a SHA256 checksum for verifying the tarball. Setting it to `"?"` makes Jinx auto-fill the value into the recipe on first download.

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
tarball_sha256="97efeda496274082e4ed0edf641a7ce5559d4b030fd6b16547e2f13c6d9d00d5"
# ...
```

#### `tarball_sha512`

- **Required** for [`tarball_url`](<#tarball_url>) checksum (one of `tarball_blake2b`, `tarball_sha256`, `tarball_sha512`).

Specifies a SHA512 checksum for verifying the tarball. Setting it to `"?"` makes Jinx auto-fill the value into the recipe on first download.

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
tarball_sha512="172acc1bc70350b1f7e46063e98c4a5ce4dd3c245a7e7bd383d8fce4a44ea0d46057b55af0aa279cda1d4b1413e924377ccce9562cef9e83eb6e30fc136a383c"
# ...
```

#### `zip_url`

- **Optional**.
- Mutually exclusive with [`tarball_url`](<#tarball_url>), [`git_url`](<#git_url>) and [`source_dir`](<#source_dir>) (setting any of those alongside it is an error).

Exactly like [`tarball_url`](<#tarball_url>), except that the downloaded archive is unpacked with `unzip` instead of `tar`. Checksums come from [`zip_blake2b`](<#zip_blake2b>), [`zip_sha256`](<#zip_sha256>) or [`zip_sha512`](<#zip_sha512>), which behave exactly like their `tarball_*` counterparts, `"?"` auto-fill included.

`unzip` ships in the container's base image, so a `zip_url` recipe needs nothing extra to unpack its source.

Example:

```sh
#! /bin/sh
# recipes/test/recipe

version=1.0.0
# ...
zip_url="https://example.com/test-${version}.zip"
zip_sha256="97efeda496274082e4ed0edf641a7ce5559d4b030fd6b16547e2f13c6d9d00d5"
# ...
```

#### `zip_blake2b`

- **Required** for [`zip_url`](<#zip_url>) checksum (one of `zip_blake2b`, `zip_sha256`, `zip_sha512`).

Specifies a BLAKE2B checksum for verifying the zip archive. Setting it to `"?"` makes Jinx auto-fill the value into the recipe on first download.

#### `zip_sha256`

- **Required** for [`zip_url`](<#zip_url>) checksum (one of `zip_blake2b`, `zip_sha256`, `zip_sha512`).

Specifies a SHA256 checksum for verifying the zip archive. Setting it to `"?"` makes Jinx auto-fill the value into the recipe on first download.

#### `zip_sha512`

- **Required** for [`zip_url`](<#zip_url>) checksum (one of `zip_blake2b`, `zip_sha256`, `zip_sha512`).

Specifies a SHA512 checksum for verifying the zip archive. Setting it to `"?"` makes Jinx auto-fill the value into the recipe on first download.

#### `git_url`

- **Optional**.

URL to a Git repository that will be cloned to be used as the source for this recipe. Requires a [`commit`](<#commit>) property to specify the commit to check out.

If combined with [`source_dir`](<#source_dir>), the recipe becomes a *managed clone* - cloned once into `source_dir` and thereafter used in place as a local package. See [`source_dir`](<#source_dir>). Cannot be combined with [`tarball_url`](<#tarball_url>).

Example:

```sh
#! /bin/sh
# recipes/libgcc-binaries/recipe

# ...
git_url="https://codeberg.org/osdev/libgcc-binaries.git"
# ...
```

#### `commit`

- **Required** for [`git_url`](<#git_url>) (including in managed-clone mode).
- Must be a 40-character lowercase hex string.

Specifies the commit to check out from a Git repository specified by [`git_url`](<#git_url>). Branch names and tags are not accepted; this must be a full commit hash so that builds remain reproducible. In [managed-clone mode](<#source_dir>) it is the commit checked out on the initial clone only; afterwards the working tree is yours to move around.

Example:

```sh
#! /bin/sh
# recipes/libgcc-binaries/recipe

# ...
commit="28257019ce04f784337cb9c3125abb4d02cef14d"
# ...
```

#### `shallow`

- **Optional**.
- `yes`/`no`. Default: `yes`.

Controls whether [`git_url`](<#git_url>) clones are shallow. By default, Jinx performs a shallow `--revision=<commit> --depth=1` clone of the requested commit. Set `shallow=no` to perform a full clone instead, which is useful when the recipe's `prepare()` or `build()` needs git history (e.g. `git describe` for version stamping). Has no effect in [managed-clone mode](<#source_dir>), which always performs a full clone.

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
git_url="https://example.com/test.git"
commit="0123456789abcdef0123456789abcdef01234567"
shallow=no
# ...
```

#### `deps`

- **Optional.**
- Space-separated list of recipes.

Specifies normal-recipe dependencies that must be built before this recipe will work. `deps` are also recorded as **runtime** dependencies in the produced XBPS metadata, so they are pulled in when this package is installed.

Dependencies in `deps` must **only** be normal recipes (i.e. files in `recipes/`).

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
deps="thing1 thing2"
# ...
```

#### `builddeps`

- **Optional.**
- Space-separated list of recipes.

Specifies normal-recipe dependencies that must be built before this recipe will work, but that are **not** recorded as runtime dependencies in the resulting XBPS package. Use this for build-only requirements (e.g. compilers, code generators, header-only libraries that don't ship into the final package).

As with `deps`, only normal recipes are valid here.

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
builddeps="autoconf gettext-host"
# ...
```

#### `binpkgdeps`

- **Optional.**
- Space-separated list of recipes.
- Normal recipes only.

Specifies normal-recipe **runtime** dependencies that are recorded in the produced XBPS metadata exactly like [`deps`](<#deps>), but that create **no build-order edge**: Jinx will not build them before this recipe, and [`dry-run`](<#dry-run>) does not order them. This exists for the single case `deps` cannot express - a runtime dependency whose build-order edge would close a cycle, i.e. `a` needs `b` at run time while `b` needs `a` to build.

The entries are real XBPS run-dependencies, so they still end up in a sysroot whenever this package is installed into one. That is why, when building a recipe, Jinx first builds any missing `binpkgdeps` of the packages in its dependency closure: `xbps-install` has to be able to resolve them while populating the build sysroot.

Listing the same recipe in both `binpkgdeps` and `deps`/`builddeps` is an error, and each entry must name a recipe in `recipes/`. Entries whose recipe sets [`bootstrap_pkg=yes`](<#bootstrap_pkg>) are dropped from the metadata, just as with `deps`. [`revbump`](<#revbump>) does not follow `binpkgdeps` edges either; [`download`](<#download>) does.

Example:

```sh
#! /bin/sh
# recipes/a/recipe

# ...
binpkgdeps="b" # 'a' needs 'b' at run time, but 'b' needs 'a' to build.
# ...
```

#### `hostdeps`

- **Optional**.
- Space-separated list of recipes.

Specifies host recipes that must be built before this recipe will work; the host package(s) are installed into the build container's `/usr/local` for `configure()`/`build()`/`package()` (cross compilers, code generators, etc.). `hostdeps` are **not** recorded as runtime dependencies of the resulting XBPS package - use [`hostrundeps`](<#hostrundeps>) for that. Must **only** be host recipes (i.e. files in `host-recipes/`).

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
hostdeps="gcc binutils"
# ...
```

#### `hostrundeps`

- **Optional**.
- Space-separated list of recipes.

Like `hostdeps` for the purposes of build-time setup (built first and installed into the container's `/usr/local`), but **additionally** recorded as XBPS runtime dependencies of the produced host package - so any future host package that consumes this one will automatically pull them in. Use these for host tools that this recipe will itself invoke at run time when consumed elsewhere; listing a dependency under `hostrundeps` makes also listing it under `hostdeps` redundant. Must **only** be host recipes.

Example:

```sh
#! /bin/sh
# host-recipes/automake/recipe

# ...
hostrundeps="autoconf" # automake invokes autoconf at runtime, so consumers of automake also need autoconf
# ...
```

#### `imagedeps`

- **Optional**.
- Space-separated list of Debian packages.

Specifies Debian packages that must be installed into the container environment before this recipe is built. This could be something like `build-essential` for a recipe that needs to compile C code, or `meson` for a recipe that builds with Meson.

Each unique combination of `imagedeps` produces a cached container image under `.jinx-cache/sets/`, so reusing the same set across many recipes is cheap. The list is sorted and de-duplicated first, then installed one package at a time into nested `sets/<pkg1>/<pkg2>/.../.image` directories, which means recipes whose sorted sets share a prefix also share the images cached for that prefix.

>[!note]
>`imagedeps` are packages of the *container image*. Under [`JINX_NATIVE_MODE=yes`](<#environment-variables>), a recipe that leaves [`cross_compile`](<#cross_compile>) unset runs with the sysroot itself as its container root, so no image - and therefore no `imagedeps` - is involved; such recipes have to get their build tools from `deps`/`builddeps` instead.

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
imagedeps="build-essential patchelf"
# ...
```

#### `imagerundeps`

- **Optional**.
- Space-separated list of Debian packages.
- Host recipes only.

The image-level counterpart of [`hostrundeps`](<#hostrundeps>): Debian packages this host package needs when it is *used*, rather than only when it is built. Like [`imagedeps`](<#imagedeps>), they are installed into this recipe's own build image - a host build routinely runs what it just built - but **additionally** any recipe that pulls this host recipe in through [`hostdeps`](<#hostdeps>)/[`hostrundeps`](<#hostrundeps>), directly or transitively, has them appended to its own `imagedeps` as its container image is assembled, so consumers do not have to know what the tool needs to run. Just as with `hostrundeps` and `hostdeps`, listing a package under `imagerundeps` makes also listing it under `imagedeps` redundant.

The packages are sorted and de-duplicated together with the `imagedeps` of whichever recipe's image they end up in, so they take part in the image-set caching exactly like a directly-listed one. As with `imagedeps`, a recipe running under [`JINX_NATIVE_MODE=yes`](<#environment-variables>) that leaves [`cross_compile`](<#cross_compile>) unset gets no image at all, so `imagerundeps` do not apply there either.

Example:

```sh
#! /bin/sh
# host-recipes/rust/recipe

# ...
imagerundeps="libssl3t64" # the host rustc loads Debian's libssl at run time, so its consumers need it too
# ...
```

#### `allow_network`

- **Optional**.
- `yes`/`no`. Default: `no`.

If `yes`, the build container is given access to the network. Otherwise, the container is run with `--net` and is fully isolated from the network.

>[!warning]
>This must not be relied on for security when building untrusted recipes. The container is **not** a virtual machine.

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
allow_network=yes
# ...
```

#### `cross_compile`

- **Optional**.
- `yes`, or unset. Default: unset.

When [`JINX_NATIVE_MODE=yes`](<#environment-variables>) is set, Jinx normally builds non-host recipes by mounting the sysroot directly as the container root. Setting `cross_compile=yes` on a recipe forces Jinx to use the cross-compile flow (separate sysroot mounted at `/sysroot`) regardless of `JINX_NATIVE_MODE`. This is the right choice for recipes that fundamentally need a cross-toolchain, such as `binutils` or `gcc` targeting a non-host architecture.

Has no effect when `JINX_NATIVE_MODE` is not set. Host recipes and the source-preparation stages always use the cross-compile flow anyway.

>[!warning]
>Set this to `yes` or leave it out entirely; `cross_compile=no` is *not* the same as leaving it unset. Container setup only tests whether the property is non-empty, so `no` selects the cross-compile flow there while the rest of the run still treats the recipe as native - leaving the build container without its sysroot and without its host packages.

Example:

```sh
#! /bin/sh
# host-recipes/gcc/recipe

# ...
cross_compile=yes
# ...
```

#### `bootstrap_pkg`

- **Optional**.
- `yes`/`no`. Default: `no`.

When `yes`, Jinx will not record this recipe as a runtime dependency of any other package, nor install it into a sysroot via `jinx install`. Use this for packages that exist purely to bootstrap the build environment (e.g. minimal early libc/runtime headers used to build the real toolchain) and that should not appear in a finished image.

Example:

```sh
#! /bin/sh
# recipes/mlibc-headers/recipe

# ...
bootstrap_pkg=yes
# ...
```

#### `clean_workdirs`

- **Optional**.
- `yes`/`no`. Default: `yes` (i.e., follows `JINX_CLEAN_WORKDIRS`; this property is the per-recipe opt-out).

Per-recipe override for the [`JINX_CLEAN_WORKDIRS`](<#environment-variables>) environment variable. Setting `clean_workdirs=no` keeps this recipe's build directory and downloaded sources around even when `JINX_CLEAN_WORKDIRS=yes` is set globally. Handy for recipes you iterate on frequently.

Example:

```sh
#! /bin/sh
# recipes/kernel/recipe

# ...
clean_workdirs=no
# ...
```

#### `conf_files`

- **Optional**.
- Space-separated list of absolute paths.

Marks files shipped by this recipe as configuration files in the XBPS metadata. XBPS then leaves a locally modified copy alone when the package is upgraded, installing the packaged version next to it (as `<file>.new-<version>`) instead of overwriting the local one.

Paths are absolute, exactly as they appear in the installed system - that is, the path a file ends up at under `dest_dir`, with `dest_dir` itself stripped off. Works the same way for normal and host recipes.

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
conf_files="/etc/test/test.conf /etc/test/policy.conf"
package() {
	DESTDIR="${dest_dir}" make install # installs ${dest_dir}/etc/test/test.conf
}
# ...
```

#### `source_*`

- **Optional**.

Recipes may also provide `source_deps`, `source_imagedeps`, `source_hostdeps`, and `source_allow_network` properties. These have effect in two situations:

1. **Within the recipe itself**, they fully replace `deps`/`imagedeps`/`hostdeps`/`allow_network` during the source-preparation stages ([`early_prepare()`](<#early_prepare>) and [`prepare()`](<#prepare>)).
2. **When this recipe is referenced by another recipe via [`from_source`](<#from_source>)**, they fully replace the consuming recipe's corresponding properties (`deps`, `imagedeps`, `hostdeps`, `allow_network`) for the duration of the source-preparation stages.

In both cases, the replacement is unconditional: an unset `source_*` means *empty* during source prep, not "inherit from the regular property". If you set any `source_*`, set every `source_*` you need.

`source_imagedeps` covers all of source preparation, fetching included, and is the only way to add a package to that container: a recipe's plain `imagedeps` are not consulted there. `source_allow_network`, on the other hand, only governs the [`prepare()`](<#prepare>) stage: fetching and patching (and therefore [`early_prepare()`](<#early_prepare>), which runs with the fetch) always have the network enabled, since that is how sources are downloaded in the first place.

This split is essential when source preparation needs different tools/network access than the actual build (for example, fetching submodules requires network and `git`, but the build itself does not).

### Functions

Within a Jinx recipe, the recipe may specify the logic for different stages of package building as named functions. Functions may reference the recipe's properties or globally defined variables and functions from the [`Jinxfile`](<#jinxfile>), using bash syntax (POSIX shell syntax also works).

```mermaid
flowchart LR
	A("early_prepare()") --> P
	P[apply patches] --> B
	B("prepare()") --> C
	C("configure()") --> D
	D("build()") --> E
	E("package()")
```

> [!note]
>No function is technically ***required*** to exist for a build to succeed; however, without functionality, the recipe does nothing. This is normal for recipes that exist purely to provide sources for another recipe (referenced via [`from_source`](<#from_source>)) - they only declare a download URL and rely on the consuming recipe to do the actual work.

> [!warning]
> Aside from [`early_prepare()`](<#early_prepare>) and [`prepare()`](<#prepare>), these functions all run within the **build directory** for the recipe. The build directory does not contain the sources by default, so reference the source directory through the `source_dir` variable that Jinx sets during these stages, e.g. `${source_dir}/configure`. It is recommended that only the `early_prepare()` and `prepare()` steps modify the source directory.

#### `early_prepare()`

The very first bit of recipe logic to run. `early_prepare()` is invoked **after** sources have been fetched (Git clone or tarball extraction) but **before** any patches are applied. It runs from the source directory.

Use it for tasks that must run on a pristine, unpatched source tree. For example, downloading additional vendored sources that the project's own build scripts expect, or removing files that should not be patched.

This function is run with the `source_*` dependencies (see [`source_*`](<#source_>)) when the recipe is referenced as a source. It shares the container with the source fetch, which always has network access, so downloads work here without setting `source_allow_network`.

Example:

```sh
#! /bin/sh
# recipes/gcc/recipe

# ...
early_prepare() {
	./contrib/download_prerequisites
}
# ...
```

#### `prepare()`

Runs after [`early_prepare()`](<#early_prepare>) and after patches have been applied. Like `early_prepare()`, it runs from the source directory.

This is an optimal place to do any further preparation of sources that a [patch](<#patches>) is unable to do, such as running `autoreconf`, regenerating Makefiles, etc.

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
source_imagedeps="autoconf-archive"
prepare() {
	autoreconf -fvi
}
# ...
```

#### `configure()`

Following the [`prepare()`](<#prepare>) stage, `configure()` is intended for getting the sources ready for building. As the name suggests, the `configure()` stage is most suited to projects that require a `./configure` invocation before builds. This stage runs only when the recipe's build directory does not yet exist: on the first build, after [`rebuild`](<#rebuild>), after a `version` or `revision` bump (which causes the next build command to wipe the build directory before re-running), or after `JINX_CLEAN_WORKDIRS=yes` cleared it at the end of the previous build.

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
configure() {
	CFLAGS="$TARGET_CFLAGS" \
	LDFLAGS="$TARGET_LDFLAGS" \
	"${source_dir}"/configure \
		--prefix="${prefix}"
}
# ...
```

>[!tip]
>Reference the internal `prefix` variable during `configure()`. For normal recipes, `prefix` defaults to `/usr`; for host recipes it defaults to `/usr/local`. You can of course pass any other value if your project demands it.

#### `build()`

Run on every build. The `build()` function contains the logic used to compile the recipe.

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
build() {
	make -j${parallelism}
}
# ...
```

>[!tip]
>Referencing the internal `parallelism` variable yields the pre-calculated optimal number of parallel jobs. It can be overridden at invocation time with the [`JINX_PARALLELISM`](<#environment-variables>) environment variable.

#### `package()`

Final stage in a recipe. After all other logic has been run, `package()` installs the build results into the recipe's package directory, which Jinx then turns into an XBPS package - in `pkgs/` for normal recipes, in `host-pkgs/` for host recipes. The location to install the build results into is given by the `dest_dir` variable.

Example:

```sh
#! /bin/sh
# recipes/test/recipe

# ...
package() {
	DESTDIR="${dest_dir}" make install
}
# ...
```

>[!tip]
>The `package()` function is also a good place for post-install actions like binary stripping or generating wrapper scripts.

>[!note]
>After `package()` returns, Jinx removes any `*.la` libtool files from under `${dest_dir}${prefix}`, since they generally cause more trouble than they solve.

### Provided Variables

In addition to the recipe's own [Properties](<#properties>) and anything the [Jinxfile](<#jinxfile>) defines, Jinx populates the following variables that recipe functions can read:

| Variable | Description |
|---|---|
| `name` | The package name. Inferred from the recipe's directory basename (e.g. `recipes/xz/recipe` produces `name=xz`). Recipes must not override it: Jinx rejects a recipe that assigns a `name` other than its own directory's. |
| `recipe_dir` | Absolute path to the recipe's own directory (the one containing the `recipe` file and the optional `patches/` subdirectory). Useful for shipping additional files alongside the recipe and referencing them from `configure()`/`build()`/`package()`. |
| `source_dir` | Path to the unpacked source tree (e.g. `/base_dir/sources/<name>` for normal recipes, `/base_dir/host-sources/<name>` for host recipes, inside the container). |
| `prefix` | Install prefix. `/usr` for normal recipes, `/usr/local` for host recipes. |
| `sysroot` | Path to the populated sysroot in the container (`/sysroot`). Not present under [`JINX_NATIVE_MODE=yes`](<#environment-variables>) for a recipe that leaves [`cross_compile`](<#cross_compile>) unset, since there the container root *is* the sysroot. |
| `dest_dir` | Where `package()` should `make install` to. Jinx packs this into an XBPS file: `pkgs/` for normal recipes, `host-pkgs/` for host recipes. |
| `parallelism` | Suggested `make -j` value. From `JINX_PARALLELISM`, or auto-tuned per step from the CPU count and the RAM available when the step starts. |
| `base_dir` | The project's source directory. `/base_dir` inside the container; absolute host path outside. |
| `build_dir` | The build directory. `/build_dir` inside the container; absolute host path outside. |
| `JINX_ARCH` | Target architecture (`x86_64`, `riscv64`, ...). |

Recipes should treat these as read-only.

### Host Recipes

Located in `host-recipes/`, host recipes describe packages that should be built for the host system, as opposed to the target system. The recipes are still built within the container. Host recipes are valuable for building cross-compiler toolchains, code generators, and other tools used during the build of normal recipes.

Host recipes can declare their own sources inline (`tarball_url`, `zip_url`, `git_url`, `source_dir`, `early_prepare()`, `prepare()`, `source_*` properties, etc.), exactly the same way normal recipes do. Alternatively, they can pull sources from another recipe via [`from_source`](<#from_source>) (resolving against `recipes/`) or [`from_host_source`](<#from_host_source>) (resolving against `host-recipes/`, useful when several host recipes need to share a single source tree).

A host recipe may also list [`deps`](<#deps>)/[`builddeps`](<#builddeps>). Those name normal recipes, are built for the target, and are installed into the `/sysroot` its container sees - this is how a cross-toolchain gets the target headers and libraries it is built against. They are *not* recorded as dependencies of the resulting host package; only [`hostrundeps`](<#hostrundeps>) are, with [`imagerundeps`](<#imagerundeps>) doing the equivalent for the Debian packages a consumer's container image needs in order to run this one. [`binpkgdeps`](<#binpkgdeps>) is a normal-recipe property and has no effect in `host-recipes/`, while [`conf_files`](<#conf_files>) behaves identically for both.

Host recipes are built into XBPS packages just like normal recipes, kept in `host-pkgs/` with its own XBPS repository index. The package filename is `<name>-<version>_<revision>.<host-arch>.xbps`, where `<host-arch>` is the build machine's architecture (`uname -m`) - host tools are native to the build machine, so this is independent of `JINX_ARCH` (the target arch). When a recipe needs host dependencies, Jinx `xbps-install`s them on the fly into the build container's `/usr/local` during container preparation, exactly the way normal `deps` are installed into the sysroot; `hostrundeps` are recorded as the host package's XBPS run-dependencies. Host recipes get a `.host-revision` marker that is the exact counterpart of a normal recipe's `.revision` (it records the `revision` the source tree was last prepared for and forces a source re-clean when the recipe's `revision` changes - the `version` axis is handled separately by `.version` - gated off for `from_source`/`from_host_source` consumers in the same way). Consequently a `version`/`revision` bump on a host recipe triggers a rebuild, and [`update`](<#update>)/[`dry-run`](<#dry-run>)/[`build`](<#build>)/[`rebuild`](<#rebuild>) treat host recipes by their XBPS file identically to normal recipes.

>[!warning]
>Linking against dynamic shared libraries *may* have unexpected behaviour when the resulting binary is run from the host: it could try to load a library that is not installed on the host. Static linking, or relying solely on libraries provided by `imagedeps`, is the safer route.

>[!note]
>Outputs from host recipes are XBPS package files under `host-pkgs/` (relative to the build directory), e.g. `host-pkgs/limine-<version>_<revision>.<host-arch>.xbps`. Jinx installs them into the container automatically when building dependents. To run a host tool directly on the host, extract the package first (an XBPS file is a zstd-compressed tar archive), for example: `mkdir -p limine-host && zstdcat host-pkgs/limine-*.xbps | tar -x -C limine-host && ./limine-host/usr/local/bin/limine bios-install testos.iso`.

### Patches

Jinx provides a fairly reasonable way to patch existing software sources for the needs of the target system. Patches live next to the recipe that owns the source: place patch files under `<dir>/<name>/patches/`, where `<dir>` is `recipes/` or `host-recipes/` (whichever holds the recipe) and `<name>` matches the recipe's directory name.

Patches always belong to the **source recipe** - i.e. the recipe that declares the actual `tarball_url`/`git_url`/`source_dir`. Consumer recipes that pull sources via [`from_source`](<#from_source>) or [`from_host_source`](<#from_host_source>) **cannot have their own `patches/`** directory; attempting to do so is a hard error. If you need a patch, add it to the source recipe's `patches/` (where it applies to every consumer that shares the source).

Every patch is applied with `patch -p1` from the root of the source tree, in this order:

1. All files in `<dir>/<name>/patches/` other than `jinx-working-patch.patch`, in the order returned by shell glob expansion (lexicographic).
2. A snapshot of the source tree is then saved as `sources/<name>-clean/` (`host-sources/<name>-clean/` for a host recipe) - the "clean reference" used by [`regenerate`](<#regenerate>) to compute the working patch.
3. `<dir>/<name>/patches/jinx-working-patch.patch` is applied last, if present.

Steps 1 and 3 happen *before* the [`prepare()`](<#prepare>) stage but *after* [`early_prepare()`](<#early_prepare>).

>[!tip]
>The intended workflow is:
>1. Run `jinx build <recipe>` once to pull and prepare sources.
>2. Edit `sources/<recipe>-workdir/` directly to iterate on changes.
>3. Run `jinx regen <recipe>` to fold those edits into `<dir>/<recipe>/patches/jinx-working-patch.patch`.
>4. Run `jinx rebuild <recipe>` to verify.
>
>Static patches that you don't want `regen` to fold into can be dropped into `<dir>/<recipe>/patches/` under any name other than `jinx-working-patch.patch`. Undoing the edits and running `regen` once more deletes the working patch again.

>[!tip]
>If you'd rather author patches outside Jinx's `regen` workflow, the standard `diff` command works:
>
>```sh
>diff -urN --no-dereference binutils/ binutils-modified/ > recipes/binutils/patches/0001-my-change.patch
>```
>
>Here, `binutils/` is the original, unmodified version of the source, and `binutils-modified/` contains the changes you want to turn into a patch.
