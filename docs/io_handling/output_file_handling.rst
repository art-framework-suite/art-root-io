Output-file handling
====================

RootOutput configuration
------------------------

All RootOutput modules can be configured to switch to a new output file when one or more criteria are met. The file-switching criteria are specified in the fileProperties configuration table, as shown here:::

    out: {
    
      module_type: RootOutput
    
      fileName: "myFile.root" 
    
      # ...
    
      fileProperties: {
    
        maxEvents: <unsigned>  # default is unbounded
        maxSubRuns: <unsigned>  # default is unbounded
        maxRuns: <unsigned>  # default is unbounded
        maxInputFiles: <unsigned>  # default is unbounded
    
        # Maximum size of file (in KiB)
        maxSize: <unsigned>  # default is unbounded
    
        # Maximum age of output file (in seconds)
        maxAge: <unsigned>  # default is unbounded
    
        granularity: [Event | SubRun | Run | InputFile | Job]  # default is "Event" 
      }
    }


For a full list of RootOutput configuration parameters supported for your particular version, type art --print-description RootOutput.


The ``granularity`` parameter specifies the level at which an output file may close. 
By default, the granularity is set to Event, which is the finest granularity available. 
If users would like to ensure that only full SubRuns or full Runs (as determined by the input file) should be written to an output file, 
the appropriate granularity is SubRun or Run, respectively. The max* parameters can be specified simultaneously. See below for some examples.::

    # FHiCL file snippet
    ...
    physics.e1: [o1, o2, o3, o4]
    
    outputs: {
    
      o1: { # Only one output file for entire process
        module_type: RootOutput
        fileName: "out.root" 
      }
    
      o2: { # Switch to new output file at each Run boundary
        module_type: RootOutput
        fileName: "out_r%R.root" 
        fileProperties: {
          maxRuns: 1
          granularity: Run
        }
      }
    
      o3: { # Switch to new output file at the next SubRun after (at least) 
            # 1000 events have been written to the output file
        module_type: RootOutput
        fileName: "out_%#.root" 
        fileProperties: {
          maxEvents: 1000
          granularity: SubRun
        }
      }
    
      o4: { # Switch to new output file if a new input file has been reached OR
            # 1000 events have been written to the output file.
        module_type: RootOutput
        fileName: "out_%#.root" 
        fileProperties: {
          maxEvents: 1000
          maxInputFiles: 1
        }
      }
    }



Note that specifying a granularity other than Event is primarily useful for **delaying** output file-rollover until a new object of the specified granularity has been reached.



Output-file handling and (Sub)Run products
------------------------------------------

Such flexibility, as described above, introduces output-file switching at potentially arbitrary times, splitting up runs and sub-runs into fragments. 
To be able to accommodate such fragmentation, and for the user to be able to interpret the Run and SubRun products correctly, 
infrastructure has been put in place that ties each product to the appropriate set of events or sub-runs. This is discussed here(DEAD LINK).






















