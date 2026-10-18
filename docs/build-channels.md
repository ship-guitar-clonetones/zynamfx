# Public build channel

This repository is the public GitHub distribution of zynamfx. Builds from this
tree identify themselves as public builds and link to the public project page.
The logo link and Settings footer both use the repository identity compiled into
the app.

## Building

```sh
python3 script/build-stadium-prototype.py [--install]
```

The repository URL and channel are passed to CMake as `T3K_FORK_REPO_URL` and
`T3K_FORK_BUILD_CHANNEL` (see `cmake/ForkIdentity.cmake`), then compiled as
`T3K_REPO_URL` and `T3K_BUILD_CHANNEL` for `plugin/ui/core/BuildIdentity.h`.
The Settings footer identifies this release as a public build and links to the
public GitHub project.

Keep public build identity values pointed at the public project. Do not add
private repository locations or private development details to this tree.
