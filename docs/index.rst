.. cetlib documentation master file, created by
   sphinx-quickstart on Sun Jul  8 22:29:32 2018.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

*art_root_io* library
=====================

.. toctree::
   :maxdepth: 2

`Release notes <releaseNotes.html>`_

`Depends on <depends.html>`_



The art_root_io package contains utilities for interacting with ROOT from within an art module, service, or other kind of plugin. Some of the provided facilities include:

* RootOutput, an art output module that persists data products to ROOT files (hereafter art/ROOT files),
* RootInput, an art input source that can read the art/ROOT files, and
* TFileService, an art service that provides a simple interface for making ROOT objects, and persisting them to a bare ROOT file in an organized fashion.

Although not a member of the art suite, it is included as a member of the critic suite. 
The UPS product can be setup by itself, or it can be setup through the critic umbrella UPS product.



I/O Handling
------------

Output-file handling
~~~~~~~~~~~~~~~~~~~~

* `art/ROOT output file handling <io_handling/output_file_handling.html>`_
* `Output file renaming for ROOT files <io_handling/output_file_renaming.html>`_

Data products and ROOT dictionaries
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* `Data product dictionary how-to <io_handling/data_product_dict_howto.html>`_
* `Specifying ROOT compression for data products <io_handling/Specifying_ROOT_compression_for_data_products.html>`_
* `Facilitating schema evolution for data products <io_handling/Facilitating_schema_evolution_for_data_products.html>`_


Helper programs
---------------

The following programs are provided for reading information about art/ROOT files. 
For each program, the -h option may be specified for a printout of the available program options.

* **config_dumper**: this program will read an art/ROOT output file and print out configuration information for the process(es) that created that file.
* **file_info_dumper**: this program will read an art/ROOT output file and has the ability to print the list of events in the file, print the range of events, subruns, and runs that contributed to making the file, and provides access to the internal SQLite database, which can be saved to an external database.
* **count_events**: this program will read an art/ROOT output file and print out how many events are contained in that file.
* `product_sizes_dumper <helper_programs/Product_sizes_dumper.html>`_: this program will read and art/ROOT output file and print out information about the sizes of products.
* `sam_metadata_dumper <https://cdcvs.fnal.gov/redmine/projects/art/wiki/SAM_metadata_facilities#sam_metadata_dumper>`_: The sam_metadata_dumper application will read an art-ROOT format file, and extract the information for possible post-processing and upload to SAM.









