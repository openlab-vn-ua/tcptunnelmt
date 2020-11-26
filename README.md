# tcptunnel

tcptunnel(mt) is a simple TCP port forwarder.
The implementation based on tcptunnel project from http://www.vakuumverpackt.de/tcptunnel/.
The goal of rewrite is to provide ability of multithread operation.
This implementation is command-line compatible with original (provides superset of options).
Also, some features added  (like pipe timeout) to support more stable operation on long runs.

## Help

```
$ tcptunnel --help
TCP/IP connection tunneling utility
Usage: tcptunnel [options]

Options:
  --version
  --help

  --local-port=PORT    local port to listen at
  --remote-port=PORT   remote port to connect to
  --remote-host=HOST   remote host to connect to
  --bind-address=IP    bind address to listen at
  --client-address=IP  only accept connections from this IP address
  --buffer-size=BYTES  buffer size [default 32K]
  --fork               fork-based concurrency (same as --concurency=fork)
  --log                turns on log    (same as --log-level=1 --log-data=bin)
  --stay-alive         stay alive after first request   (turns on concurency)
  --log-level=LEVEL    logging: 0=off,1=brief,2=full              [default 0]
  --concurency=MODEL   concurency model: none,fork,threads     [default none]
  --pipe-timeout=N     pipe data transfer timeout in sec (0=none) [default 0]
  --log-data=MODE      dump pipe data mode: none,hex,bin       [default:none]

Example:
tcptunnel --remote-port=80 --local-port=9980 --remote-host=acme.com --stay-alive --log-level=2
```

## Building

### For Unix

```
$ git clone https://github.com/openlab-vn-ua/tcptunnelm.git
$ cd tcptunnelmt
$ ./configure
$ make
$ ./tcptunnel --version
$ ./tcptunnel --help
```

### For Mac OS X

You will need the [Command Line Tools for Xcode](https://developer.apple.com/xcode/) to build tcptunnel under Mac OS X.

```
$ uname -mrs
Darwin 12.5.0 x86_64

$ git clone https://github.com/openlab-vn-ua/tcptunnelmt.git
$ cd tcptunnelmt
$ ./configure --prefix=/usr/bin
$ make
$ ./tcptunnel --version
$ ./tcptunnel --help
$ file tcptunnel
tcptunnel: Mach-O 64-bit executable x86_64
```

### For Mac OS X (Homebrew)

TBD

### For Windows (Cygwin)

You will need the Cygwin environment for Windows from http://www.cygwin.com/ with the following additional packages installed:

* gcc
* git
* make

```
$ uname -a
CYGWIN_NT-6.1-WOW64 computer 1.7.25(0.270/5/3) 2013-08-31 20:39 i686 Cygwin

$ git clone git://github.com/openlab-vn-ua/tcptunnelmt.git
$ cd tcptunnelmt
$ ./configure
$ make
$ ./tcptunnel --version
$ ./tcptunnel --help
$ file tcptunnel.exe
tcptunnel.exe: PE32 executable (console) Intel 80386, for MS Windows
```

### For Windows (MinGW32)

You will need MinGW32 to cross-compile tcptunnel. 
Please see http://www.mingw.org/ for more details. 
If you are using a Debian-based distribution then you will need to install the following packages:

* mingw32
* mingw32-binutils
* mingw32-runtime

```
$ apt-get install mingw32 mingw32-binutils mingw32-runtime
$ git clone https://github.com/openlab-vn-ua/tcptunnelmt.git
$ cd tcptunnelmt
$ ./configure
$ make -f Makefile.MinGW32
$ file tcptunnel.exe
tcptunnel.exe: PE32 executable (console) Intel 80386, for MS Windows
```

Note:
The MinGW32-based version does not support the fork-based concurrent client handling.
If you need this feature under Windows, then you should use the Cygwin-based version.

## ChangeLog

See [ChangeLog](https://raw.github.com/openlab-vn-ua/tcptunnelmt/master/ChangeLog).

## License

This software released under the GPL
The initial version was written by Clemens Fuchslocher, released under the GPL.
