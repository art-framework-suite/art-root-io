Product sizes dumper
====================

art provides a utility that determines the on-disk size of each branch present in a Root output file. The utility is used by invoking::

    product_sizes_dumper <options> <source-file(s)>

where the options are as follows:::

    -h [ --help ]                 this help message.
    -f [ --fraction ] arg (=0.05) floating point number on the range [0,1].  If a
                                  TTree occupies a fraction on disk of the total 
                                  space in the file that is less than <f>, then a
                                  detailed analysis of its branches will not be 
                                  done
    -s [ --source ] arg           source data file (multiple OK)

The fraction has a default value of 0.05. It is not required to specify the "-s [--source]" option when listing the source files.

The output consists of two parts--one that summarizes the size of each TTree/TKey present in the file, 
and a part that lists the on-disk size of each branch for TTree objects whose on-disk size corresponds to a fraction greater than 0.05. 
As an example, the following output is obtained using a Mu2e Root output file:

Part 1.
-------

::

    Size on disk for the file: data_03.root
    Total size on disk: 547915622
    
         Size in bytes   Fraction TTree/TKey Name
             546094464      0.997 Events
                144784      0.000 EventMetaData
                 99419      0.000 SubRuns
                 46796      0.000 Runs
                 13906      0.000 MetaData
                  8535      0.000 EventHistory
                   836      0.000 Parentage
                   552      0.000 SubRunMetaData
                   539      0.000 RunMetaData
                     0      0.000 RootFileDB  (skipped because not a TTree; it is aTKey)
    ------------------------------
             546409831      0.997 Total



Part 2
------

::

    Details for each TTree that occupies more than the fraction 0.05 of the size on disk.

    Details for branch: Events
    
         Size in bytes   Fraction Data Product Name
             324543436      0.594 mu2e::SimParticlemv_g4run__G4Test03.
              80444376      0.147 mu2e::SimParticleart::Ptrmu2e::MCTrajectorystd::map_g4run__G4Test03.
              59352080      0.109 mu2e::PointTrajectorymv_g4run__G4Test03.
              45465242      0.083 mu2e::StepPointMCs_g4run_calorimeter_G4Test03.
              11486131      0.021 mu2e::StepPointMCs_g4run_protonabsorber_G4Test03.
               8286123      0.015 mu2e::StepPointMCs_g4run_tracker_G4Test03.
               3669371      0.007 mu2e::StepPointMCs_g4run_stoppingtarget_G4Test03.
               3116418      0.006 art::RNGsnapshots_randomsaver__G4Test03.
               2937955      0.005 mu2e::StepPointMCs_g4run_virtualdetector_G4Test03.
               2677101      0.005 mu2e::StepPointMCart::Ptrss_CaloReadoutHitsMaker_CaloHitMCCrystalPtr_G4Test03.
                836355      0.002 mu2e::StrawHits_makeSH__G4Test03.
                766270      0.001 mu2e::GenParticles_generate__G4Test03.
                686818      0.001 mu2e::StrawHitMCTruths_makeSH__G4Test03.
                343304      0.001 mu2e::StepPointMCart::Ptrss_makeSH_StrawHitMCPtr_G4Test03.
                331846      0.001 mu2e::CaloCrystalHits_CaloCrystalHitsMaker__G4Test03.
                232855      0.000 mu2e::CaloHitMCTruths_CaloReadoutHitsMaker__G4Test03.
                226707      0.000 mu2e::CaloHits_CaloReadoutHitsMaker__G4Test03.
                187969      0.000 mu2e::CaloCrystalOnlyHits_CaloReadoutHitsMaker__G4Test03.
                159374      0.000 mu2e::StepPointMCs_g4run_CRV_G4Test03.
                 48134      0.000 mu2e::CaloHitSimPartMCs_CaloReadoutHitsMaker__G4Test03.
                 43422      0.000 mu2e::StepPointMCs_g4run_calorimeterRO_G4Test03.
                 22240      0.000 EventAuxiliary
                 20156      0.000 mu2e::StepPointMCart::Ptrss_CaloReadoutHitsMaker_CaloHitMCReadoutPtr_G4Test03.
                 15216      0.000 mu2e::StatusG4_g4run__G4Test03.
                 13015      0.000 mu2e::ExtMonFNALSimHitCollection_g4run__G4Test03.
                 12841      0.000 mu2e::StepPointMCs_g4run_itrackerFWires_G4Test03.
                 12829      0.000 mu2e::StepPointMCs_g4run_trackerSWires_G4Test03.
                 12824      0.000 mu2e::StepPointMCs_g4run_ExtMonUCITof_G4Test03.
                 12815      0.000 mu2e::StepPointMCs_g4run_trackerWalls_G4Test03.
                 12803      0.000 mu2e::StepPointMCs_g4run_ttrackerDS_G4Test03.
                 12723      0.000 mu2e::StepPointMCs_g4run_PSVacuum_G4Test03.
                 12695      0.000 mu2e::StepPointMCs_g4run_timeVD_G4Test03.
                 12168      0.000 art::TriggerResults_TriggerResults__G4Test03.
                  9842      0.000 mu2e::G4BeamlineInfos_generate__G4Test03.
    ------------------------------
             546023454      1.000 Total



Part 1 of the output starts with two lines that give the name of the file and its size on disk, in bytes. 
This size should agree with the size given by ls -l. The main body of Part 1 describes all of the objects in the top level TDirectory. 
The first column gives the size of the object on disk, in bytes. 
The second column gives the same information as a fraction of total size on disk. 
The third column gives the name of the object. Part 1 concludes with a "Total" line; 
on this line the first column is the sum of the sizes on disk of all of the objects in the top TDirectory; 
the second column gives this same information as a fraction of the total size on disk. 
You will notice that the sum of the object sizes does not add up to the full on-disk size of the file. 
The reason is that the sum of the sizes does not include the file level metadata.


Part 2 of the output gives additional details for those TTree objects whose sizes exceed a minimum fraction of the size on disk. 
For this example the requirement was set at 0.05, which is given in the first line of Part 2. The format is the same as in Part 1.



















