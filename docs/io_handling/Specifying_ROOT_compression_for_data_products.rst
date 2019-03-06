Specifying ROOT compression for data products
=============================================

It is possible to specify whether and how ROOT should compress data products. 
This is done in the classes_def.xml file, specifically the line identifying the wrapped data product (art::Wrapper<...>). 
For instance, to deactivate compression for the data product ds50::CompressedV172X, you would need a line:::

    <class name="art::Wrapper<ds50::CompressedV172x>" compression="0"/>

Notes:

* Specifying the compression level attribute on an item that is not art::Wrapper<...> will have no effect.
* If the compression level is not specified, it defaults to the level specified for the file as a whole. This may be specified using the parameter compressionLevel to the ROOTOutput module, and currently this defaults to 7, slightly higher than the compression level one would get invoking zip from the command line.


Specifying the compression level.
---------------------------------


The compression attribute is a combined numeric value:

    (<root compression algorithm> * 100) + <root compression level>


The currently supported compression levels range from 0 to 9. A level of 0 means no compression, and a level of 9 means maximum compression.

The currently supported (ROOT 5.34) compression algorithms are:

* 0: use the root global compression setting from R__ZipMode, which defaults to zlib, and is set with R__SetZipMode(int mode),
* 1: zlib,
* 2: lzma (from package xz),
* 3: old method, Chernyaev-Smirnov variant of zlib.


Examples:


* Specify the default algorithm at level 9:::

        <class name="art::Wrapper<art::MyProd>" compression="9"/>


* lzma compression at level 6:::

        <class name="art::Wrapper<art::MyProd>" compression="206"/>

* Disable compression:::

        <class name="art::Wrapper<art::MyProd>" compression="0"/>




















