:orphan:

Data Product Dictionary How-To
==============================

Prelude
-------


Here, we describe the steps that must be taken to apprise the ROOT persistence system used by art of a new (or altered) data product.

First, please read the Data Product Design Guide (**LINK**): 
the advice it offers will lead to easier maintenance and better robustness of your data product and the uses thereof.



Overview
--------

ROOT needs to understand the structure of the data product you wish to save in a ROOT file. 
Currently this is done by invoking the genreflex utility to produce a syntax tree with gccxml which is then used to generate code which forms the dictionary. 
A dictionary must be generated for each class that must be understood: 
the top-level data product, the art::Wrapper of same, and then every non-basic type on down through the class hierarchy.


Some pre-defined dictionaries such as standard containers of basic types (integers types, floating point types, strings) are already provided by art: 
you should not duplicate these yourself.


Build system requirements
-------------------------

Your build system must be able to create a shared library XXX_dict.so (or .dylib for OS X) 
by compiling code generated using genreflex from the two files integral to the specification of and building of a dictionary: 
classes_def.xml and classes.h. MRB(**LINK**) and the CET(**LINK**) build tools do this, as do SoftRelTools and the Mu2e experiment's SCons build system. 
Please contact your experiment's software experts for details on your specific system. 
If on the other hand, you are your experiment's software expert, please enter a support issue. for details on how to teach your build system about ROOT dictionaries.


The classes_def.xml and classes.h files
---------------------------------------

There should be exactly one of each of these files in every directory where a dictionary should be created. 
Most people tend to put them in the same directory as the library code for which dictionaries should be generated, 
but there is no reason other than bookkeeping and transparency why they could not be in a directory of their own.


classes_def.xml
~~~~~~~~~~~~~~~

The general form of a classes_def.xml file is as follows:::

    <lcgdict><class .../>...</lcgdict>

Every class (including nested classes) should have a <class/> entry, 
and there should be an entry for the class art::Wrapper<XXX>, 
where XXX is your top level data product. For further details, see genreflex -h or the ROOT classes_def.xml documenation(**LINK**).


Every entry for a non-template class should ideally have a classVersion="##" attribute, where ## is any number, 
but a good rule of thumb is that it should start at 10 and be incremented whenever the inheritance or the data members of the class change 
(all the way down the inheritance or containment hierarchy). 
This enables the use of ROOT's schema evolution capabilities. Templates of your own design should have a public member function:::

    static short Class_Version() { return 10; }

which must be updated manually whenever the template definition is changed.

If you are using the CET build system or MRB, the checkClassVersion script will add a checksum entry to the class 
and automatically bump the classVersion attribute whenever necessary (this will require the build to be restarted).



For example, the following classes:::

    struct ContainedClass { int x; };
    
    struct BaseClass { virtual ~BaseClass() {} };
    
    struct DerivedClass : public BaseClass { float f; };
    
    struct OtherContainedClass { std::string y; }
    
    struct DProd {
       ContainedClass a;
       DerivedClass b;
       std::vector<OtherContainedClass> c;
       int d;
    };


requires the following classes_def.xml file:::

    <lcgdict>
      <class name="ContainedClass" classVersion="10"/>
      <class name="BaseClass" classVersion="10"/>
      <class name="DerivedClass" classVersion="10"/>
      <class name="OtherContainedClass" classVersion="10"/>
      <class name="std::vector<OtherContainedClass>"/>
      <class name="DProd" classVersion="10"/>
      <class name="art::Wrapper<DProd>"/>
    </lcgdict>


Note that you may assume that std::string is already known to the ROOT system and neither needs to be mentioned in classses_def.h
nor as an instantiation of class std::basic_string<char>.


classes.h
~~~~~~~~~

This is a C++ header file which must include all the necessary headers for the class, struct and enum entries you reference in classes_def.xml.









