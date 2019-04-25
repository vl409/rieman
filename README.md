Rieman - X11 desktop pager
==========================

The desktop pager is a progam that:

 * configures layout of virtual desktops
 * displays virtual desktops and windows on it
 * switches between desktops/windows per user requests

 ![Default theme](doc/s1.png?raw=true "Default theme") | ![Transparent theme](doc/s2.png?raw=true "Transparent theme")

INSTALLATION
------------

### Dependencies

You will need build tools:

 * c99 compiler (gcc and clang tested)
 * gnu make
 * pkg-config to detect libraries

 and some libraries:

 * X11        - for X11, as a core dependency
 * xcb        - almost all work with X11 and NETWM spec, many of them:
 * xlib-xcb
 * xcb-util
 * xcb-ewmh
 * xcb-icccm
 * cairo      - 2D drawing
 * fontconfig - find font by name
 * freetype2  - work with fonts itself
 * libxml2    - configuration

also, you will need some fonts, for example:

 * media-fonts/droid

 fonts from the package are mentioned in a default configuration file;
 feel free to use anything you like/have available on your system.

### Configuring:

```
$ ./auto/configure [--prefix=] [--help] ...
```

Various additional options are accepted, like compiler options and data
directories, refer to the builtin help for details.

### Building:

```
$ make
```

### Installing

```
$ make install
```

this will install software to directories, previously configured.
To remove, just run
```
$ make uninstall
```

you will be prompted before deleting anything.

RTFM:
-----

The rieman comes with a man page:

```
$ man rieman
```
