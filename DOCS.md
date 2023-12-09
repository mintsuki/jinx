# Jinx

Jinx (Jinx Is Not Xbstrap) is a meta-build-system for bootstrapping operating
system distributions inspired by xbstrap (https://github.com/managarm/xbstrap).

The goal is to create an easier, fully deterministic, and reproducible
build-system that makes porting software easier by isolating dependencies
better.

## How Jinx works

Jinx acts as a frontend for an arch container, interfaced thru a thinner layer
than docker. Sources and dependencies are moved in and out to ensure completely
reproducible builds, since no leftover data can influence future builds, and
portability, as no packages are needed to build apart of the container. 

This design has the downside of being not very portable between systems, so far
only Linux systems are supported.

## Available commands

Jinx has the following commands, which can be invoked using

```bash
./jinx <command> <arguments>
```

- `sysroot`: Generates a sysroot with all the built packages on the `sysroot`
folder.
- `host-build <package>`: Builds a host package, it can be rebuilt by using
`host-rebuild`, else it having finished will be cached. Dependencies are
automatically built.
- `build <package>`: Does the same as `host-build` for a non-host package.
- `build-all`: Builds all the non-host packages, including dependencies.
- `clean`: Cleans all the state of Jinx, along with built packages.
- `regenerate <package>`: When Jinx fetches a package and starts building it, it
creates 3 versions, `sources/<package>-workdir`, `sources/<package>-clean`, and
`sources/<package>`. Changes can be applied to `<package>-workdir`, and
`regenerate <package>` will copy them to a patch under `patches/<package>`, to
be applied automatically every time its needed.

## Setting up Jinx

Here are some instruction on how to setup jinx for a project.

### Fetching Jinx

Jinx is distributed as a shell script, taking it from the repository will
be enough, as it will download all it needs itself.

### Setting up directories

Jinx separates its packages in directories. 4 directories are needed to
be present in the same directory as the script:

- `host-recipes`: Recipes for tools used by the host machine.
- `patches`: Patches to be applied automatically when fetching.
- `recipes`: Recipes for packages to be shipped on the final distribution.
- `source-recipes`: Recipes to fetch the sources of host and non-host recipes.

Jinx additionally allows to have a `jinx-config` file in the same path as
the rest of the project, this file can be used for specifing variables to be
used in recipes, an example is:

```bash
common_cflags="-O2 -pipe"
cool_path=${dest_dir}/usr/share/really_cool_path
```

### Writing recipes

All package recipes, be it host or non-host, follow the same format:

```bash
name=example               # Name of the package.
from_source=example        # Name of the source under 'source-recipes' to use.
revision=1                 # Revision, used for versioning by the user.
imagedeps="meson ninja"    # Dependencies needed by the container.
hostdeps="gcc"             # Dependencies under 'host-recipes'
deps="mlibc"               # Dependencies under 'recipes'


# Building step, should do the bulk of the work and compile source, or generate
# install artifacts.
build() {
    ...
}

# Installing step, should move the generated files into their final resting
# place.
install() {
    ...
}
```

For host recipes, `hostrundeps` is available for a role similar to `deps`, but
using host packages instead of non-host recipes.

A few variables are available for use internally by the script:

- `${source_dir}`: Path of the current source.
- `${prefix}`: Default prefix.
- `${dest_dir}`: Destination of the final executables.

Source recipes have the format:

```bash
name=example
version=1.16.5
source_method=tarball
tarball_url="https://ftp.gnu.org/gnu/automake/automake-${version}.tar.gz"

# Should do the steps or processing required to bring the package to a
# configurable state, for example, doing 'autoreconf' for an autotools package.
regenerate() {
    ...
}
```

They accept the same `hostdeps` and `deps` as any other package.

## Troubleshooting

### `unshare` issues

`unshare` is a Linux-specific system call used internally for managing
containers, the use of this system call can give issues for users using
hardened Linux kernels, this can be fixed by issuing the following command:

```bash
sysctl -w kernel.unprivileged_userns_clone=1 # Might need sudo
```
