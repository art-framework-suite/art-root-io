:orphan:

Facilitating Schema Evolution for Data Products
===============================================

[ NB: these notes are tentative. Experience is required with this before we can be sure that this is a sufficient and sustainable way of achieving reliable schema evolution. ]



Overview.
---------


The phrase, "schema evolution" describes the behavior of the ROOT system when encountering an object in an input file that is of a different (older) version than the one in memory. 
This may be a real version number (see below for how to define this); or it may be simply that the object checksum (again, see below) is different.

A certain amount of limited schema evolution of data products is automatic. 
If a data member has been removed from a class for example, objects with the data still present may still be read in and used from a file and the extraneous data member will be ignored. 
Similarly, new data members are fine under most circumstances with the caveat that the value of said data member will be whatever was in the object in memory prior to the data being streamed in -- 
either its default value (assuming it was initialized properly in the constuctor); 
or the pre-existing value of said data member if the object was previously in-use before being overwritten by the stream-in operation.



"Fast-cloning" and automatic schema evolution.
----------------------------------------------

If some objects in the input file are to be written to an output file without otherwise being read, they may be fast-cloned. 
This will produce a fatal error if the structure of the object being transferred from the input to the output is different from that of objects of the same class in memory.
This may be dealt with trivially either by:


1. Deactivating fast cloning:::

    fastCloning: false

in the output module configuration; or


2. Ignoring the particular products for output (again in the output module configuration). For instance:::

    outputCommands: [ "keep *", "drop sim::Electronss_driftel_*_GenieGen" ]



Identifying versions of data products.
--------------------------------------

Old products without an explicit version number may be identified by their object checksum. This may be obtained either by:


1. Opening the old file in a vanilla version of root without any data dictionaries loaded:::

    root -b -l <data-file>
    root [1] cout << std::hex << TClass::GetClass("arttest::IntProduct")->GetCheckSum() << std::dec << std::endl;
    e6a1e2a1

2. Within a software release of the right vintage, make the same calls to obtain the class and checksum.


This checksum (once obtained) may be referenced in the classes_def.xml file to specify particular behavior upon encountering this version of the class in a fie.



Adding a version to a previously un-labeled product class.
----------------------------------------------------------

A discussion of older objects not explicitly labeled with a version number was 
necessary prior to this section in order to understand the behavior of the ROOT system upon reading them in. 
Every distinct unlabeled version of a class is assigned a version number by ROOT (starting from 1) in the order it is seen during a particular execution of a job. 
Therefore, when labeling a class with a specific version number for the first time 
it is important to start at a number higher than 1 to allow for older, unlabeled versions of the class to be read into the job without collision.



The syntax for adding a version number to a class is different when using CINT dictionaries vs genreflex: we shall discuss only the latter here. 
Here is an example of adding a version number label to a class as specified in classes_def.xml:::

    <class name="art::TriggerResults" ClassVersion="10"/>


The following <class> declarations are eligible for addition of a ClassVersion notation:
* Classes and structs;
* Contained classes and structs.

The following <class> declarations are not eligible for addition of a ClassVersion notation:
* Typedefs;
* Instantiations of templates (anything with a < character in the name) -- but see below.

Note that you only need add version numbers to the description of the contained class -- not that of any containers thereof or the wrapper.

If your product or a contained member datum happens to be a template (eg location<T> with T being (for instance) a floating point type) 
then instead of using classes_def.xml one must instead add a member function to your class, viz:::

    static short Class_Version() { return 10; }

This is not advisable for non-template products because it is not possible to check currently for consistency (see below).



Specifying backward-compatibility behavior for older class versions.
--------------------------------------------------------------------

If one needs to specify some particular steps to be taken upon encountering a version or versions of a class, then a clause in classes_def.xml is required. 
The full instructions may be found in the ROOT document(**LINK**).::

    <ioread sourceClass="art::RefCore" version="[-9]" targetClass="art::RefCore" source="" target="prodPtr_">
      <![CDATA[prodPtr_=0;
       ]]>
    </ioread>


The keywords are explained as follows:

* sourceClass
    The class on-file to be used as input.

* version
    A comma-separated vector (denoted by []) listing versions to which this rule is applicable.

* targetClass
    The class in memory to be updated.

* source
    A ;-separated list of members of the sourceClass to be used in the rule.

* target
    A ;-separated list of members of the targetClass to be updated by the rule.

* <![CDATA[...]]>
    The update code to be executed.




Dealing with consistency problems.
----------------------------------

If one is less than completely vigilant about updating the class version in classes_def.xml every time the class code is changed, 
there is the potential for confusion and collision, 
with different versions of a class being assigned the same numeric label. 
The version number should be updated for a class any time that class' interface, inheritance or member data are changed, 
even if that class is only ever saved by containment rather than standalone.



checkClassVersion and automatic build-checking of class version consistency.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As of ART v0.07.00, the package will contain an adaption of edmCheckClassVersion (source:tools/checkClassVersion), 
a python script written by Chris Jones of CMS in order to verify the consistency of the version assignments in a given classes_def.xml file. 
This utility will check the version consistency of all classes with ClassVersion entries in the classes_def.xml file to be verified 
by reading the library and asking ROOT for the checksum for each such object. 
Each unique version number/checksum pair will be recorded for the relevant <class> declaration 
by inserting (or instructing you to insert, which is the default) 
a nested <version> declaration (ignored by genreflex). This information will be used by future invocations of the script 
to check the consistency of the current version / checksum of the class.



Caveats.
........

This script is unable to verify the consistency of class templates whose class versions must be specified by means of a static member function. 
Developers must be sure to update the version of these templates themselves in order to maintain consistency.


Usage.
......

::

    checkClassVersion [<options>] -x <classes_def.xml-file>
    
    Options
    
      -g
         Produce a new file classes_def.xml.generated in the current
         working directory containing updated information (if appropriate).
         If a consistent classes_def.xml.generated file could be produced
         (or if the file was already consistent with the library) then the
         script will exit with code 0.
    
      -G
         Update the specified classes_def.xml file in-place. Note that the
         script will still have a non-zero exit code (2) if any changes were
         made to the classes_def.xml file even if the resulting file is
         consistent and correct. If this script is invoked by a build system,
         this will signal to the build system that the build must be re-done.
    
      -l
         Specify the library in which to find the dictionary information for
         the classes to be verified. Without this option, ROOT's own plugin
         manager will be used to find the library via any .rootmap files in
         LD_LIBRARY_PATH. Note that the library must be linked with everything
         upon which it depends (except those libraries pulled in by
         libart_Framework_Core.so, which is loaded automatically) or a failure
         will occur.
    
      -x
         Specify the location of the classes_def.xml file.


If -g or -G are selected, then the script will make the changes (if it can) to make the classes_def.xml file (or a generated copy thereof) correct and consistent. 
Otherwise instructions will be printed as to steps the user should take to do same and the program will exit with code 1.


Example: use in the ART build system.
.....................................

The CMake-based ART build system now has a macro art_dictionary which invokes build_dictionary followed by check_class_version. 
The latter macro will ensure that the checkClassVersion script is invoked with the -G option at the appropriate point in the build 
(after the dictionary, the library containing the class implementation and all dependent libraries have been built). 
Should any problems be found with the classes_def.xml file the script will fail with 
exit code 1 or 2 depending respectively on whether the problems were fatal or fixed by the script. 
This will cause the build to fail, and either a printed ERROR will give details of the un-fixable problem or a WARNING 
will prompt the user to re-try the build, which should then succeed.


As of 2011/04/29, all appropriate ART classes declared in classes_def.xml 
have had an attribute ClassVersion="10" added to their class declaration 
and checkClassVersion is now invoked at the appropriate place in every build to verify consistency. 
ART developers should ensure they check in updated classes_def.xml files along with any changes they make to persistent classes.























