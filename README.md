grandroid
=========

Android Graphite support

Android currently does not support Graphite font. This project provides a cheap way for
applications to add Graphite font support. It provides the graphite library and loader
libraries to integrate it into the application while letting the application use standard
system library components. Currently this project supports Android 4.0.4-4.3 (API levels
15-18) with a view to extending the range in both directions.

A simple example application is provided along with the fonts used. The fonts are released
under the Open Font License (http://scripts.sil.org/ofl).

Due to the complexities of adding after market
support for rendering, this library has to do some rather intricate techniques in order to
integrate the library into the underlying system.


