:orphan:

Output file renaming for ROOT files
===================================


The RootOutput module and TFileService offer the ability to rename output files upon close to reflect information gleaned during the job. 
By using format specifiers in the fileName, one can include dynamic information in the final output filename:


Output File Format Specifiers.
------------------------------




+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| specifier                               |  meaning                                                                                              |
+=========================================+=======================================================================================================+
| %#                                      |  Sequence number [1]_ (i.e. the 3rd file written to this output stream will have sequence number 3).  |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %ifb                                    |  Base name of input file without extension.                                                           |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %ifd                                    | Fully-resolved path of input file without file name.                                                  |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %ife                                    | Extension of input file.                                                                              |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %ifn                                    | Base name of input file with extension.                                                               |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %ifp                                    | Fully-resolved path with file name of input file.                                                     |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %ifs%<match>%<format>%[ig]%             | Regex-based substitutions of input file (ECMAScript with % delimiter). literal % characters forbidden |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %l                                      | Label of output module.                                                                               |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %p                                      | art's process name from FHiCL or command-line                                                         |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %r                                      | Lowest run # of run records written to this file [1]_                                                 |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %R                                      | Highest run # of run records written to this file [1]_                                                |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %s                                      | Lowest subrun # of subrun records written to this file [1]_ .                                         |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %S                                      | Highest subrun # of run records written to this file [1]_                                             |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %S                                      | Highest subrun # of run records written to this file [1]_                                             |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %to                                     | Time [2]_ of file-open.                                                                               |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %tc                                     | Time [2]_ of file-close.                                                                              |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %ts                                     | Start time [2]_ of SubRun corresponding to the %s specifier.                                          |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %tS                                     | Start time [2]_ of SubRun corresponding to the %S specifier.                                          |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %tr                                     | Start time [2]_ of Run corresponding to the %r specifier.                                             |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+
| %tR                                     | Start time [2]_ of Run corresponding to the %R specifier.                                             |
+-----------------------------------------+-------------------------------------------------------------------------------------------------------+


.. [1] Also accepts printf-style fill modifiers (e.g. %05s for a substitution that is zero-filled to 5 digits).
.. [2] The format is "YYYYMMDDTHHMMSS", where "T" separates the date and time.




The ROOT file is written, initially as a temporary file with unique file name in the same directory as that specified by the fileName parameter. 
Upon close, it is renamed as specified by fileName.



Non-destructive post-close callback.
------------------------------------

In addition, interested services may register callback functions with signature:::

    void func(art::OutputFileInfo const &)

where an art::OutputFileInfo is described in :rsource:`art/Framework/Core/OutputFileInfo.h` 
and callbacks are registered with sPostCloseOutputFile.watch(...). 
These functions are called after the file has been moved to its final name and position and are intended (obviously) for non-destructive purposes.









