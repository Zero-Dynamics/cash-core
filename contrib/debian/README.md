
Debian
====================
This directory contains files used to package odyncashd/odyncash-qt
for Debian-based Linux systems. If you compile odyncashd/odyncash-qt yourself, there are some useful files here.

## odyncash: URI support ##


odyncash-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install odyncash-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your odyncash-qt binary to `/usr/bin`
and the `../../share/pixmaps/odyncash128.png` to `/usr/share/pixmaps`

odyncash-qt.protocol (KDE)
