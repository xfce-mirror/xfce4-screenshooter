[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://gitlab.xfce.org/apps/xfce4-screenshooter/-/blob/master/COPYING)

# xfce4-screenshooter

Xfce4-screenshooter allows you to capture the entire screen, the active
window or a selected region. You can set the delay that elapses
before the screenshot is taken and the action that will be done with
the screenshot: save it to a file, copy it to the clipboard, open
it using another application, host it on imgur.com (a free online image
hosting service) or use your creativity with custom action scripts.

----

### Homepage

[Xfce4-screenshooter documentation](https://docs.xfce.org/apps/xfce4-screenshooter/start)

### Changelog

See [NEWS](https://gitlab.xfce.org/apps/xfce4-screenshooter/-/blob/master/NEWS) for details on changes and fixes made in the current release.

### Source Code Repository

[Xfce4-screenshooter source code](https://gitlab.xfce.org/apps/xfce4-screenshooter)

### Download a Release Tarball

[Xfce4-screenshooter archive](https://archive.xfce.org/src/apps/xfce4-screenshooter)
    or
[Xfce4-screenshooter tags](https://gitlab.xfce.org/apps/xfce4-screenshooter/-/tags)

### Installation

From source code repository: 

    % cd xfce4-screenshooter
    % ./autogen.sh
    % make
    % make install

From release tarball:

    % tar xf xfce4-screenshooter-<version>.tar.bz2
    % cd xfce4-screenshooter-<version>
    % ./configure
    % make
    % make install

You need several dependencies to build. The correct way to install dependencies depends on your distribution.
On XUbuntu 22.04 this should be enough:

    sudo apt install --yes build-essential xfce4-dev-tools libpackagekit-glib2-dev libxfce4panel-2.0-dev libxfce4ui-2-dev libxml2-dev libexo-2-dev libsoup2.4-dev 

### Reporting Bugs

Visit the [reporting bugs](https://docs.xfce.org/apps/xfce4-screenshooter/bugs) page to view currently open bug reports and instructions on reporting new bugs or submitting bugfixes.

