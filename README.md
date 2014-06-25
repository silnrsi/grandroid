grandroid
=========

Android Graphite support

Android currently does not support Graphite font. This project provides a cheap way for
applications to add Graphite font support. It provides the graphite library and loader
libraries to integrate it into the application while letting the application use standard
system library components. Currently this project supports Android 2.2-4.4 (API levels
8-19) with a view to maintaining support for future API levels.

A simple example application is provided along with the fonts used. The fonts are released
under the Open Font License (http://scripts.sil.org/ofl).

Due to the complexities of adding after market
support for rendering, this library has to do some rather intricate techniques in order to
integrate the library into the underlying system.

To use this system, I do not recommend trying to build the .so files from scratch. Instead,
download grandroid-debug.apk from the latest release of this repository and extract the
libs/ subtree and merge it with your project libs/.
Then copy grwebapp/src/org/sil/palaso/Graphite.java into your project and
look at grwebapp/src/org/sil/palaso/grandroid/Grandroid.java for an example of how to
use the libraries.


