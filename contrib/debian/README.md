
Debian
====================
This directory contains files used to package cashd/cash-qt
for Debian-based Linux systems. If you compile cashd/cash-qt yourself, there are some useful files here.

## cash: URI support ##


cash-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install cash-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your cash-qt binary to `/usr/bin`
and the `../../share/pixmaps/cash128.png` to `/usr/share/pixmaps`

cash-qt.protocol (KDE)
