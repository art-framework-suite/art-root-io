#!/usr/bin/perl -w

use strict;

use vars qw(%header_list);
use vars qw(%library_list);

BEGIN {
  %header_list = (
    "art/Framework/Services/Optional/TFileService.h" => "art_root_io/TFileService.h",
    "art/Framework/Services/Optional/TFileDirectory.h" => "art_root_io/TFileDirectory.h"
  );
  %library_list = (
    "art_Framework_Services_Optional_TFileService_service" => "art_root_io_TFileService_service",
    "ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE" => "ART_ROOT_IO_TFILESERVICE_SERVICE"
  );
}

my $top_level_dir = '.';
my $dry_run = 0;
if (scalar @ARGV > 0) {
  $top_level_dir = $ARGV[0];
}
if (scalar @ARGV > 1) {
  $dry_run = $ARGV[1];
}

# Replace C++ header dependencies
foreach my $old_inc (sort keys %header_list) {
  my $new_inc = $header_list{$old_inc};
  print "Checking for files using $old_inc\n";
  my $cpp_files_str = `find $top_level_dir \\( \\( -name .svn -o -name .git -o -name CVS \\) -prune \\) -o \\( -name '*.c' -o -name '*.cxx' -o -name '*.cc' -o -name '*.cpp' -o -name '*.C' -o -name '*.h' -o -name '*.hxx' -o -name '*.hh' -o -name '*.hpp' -o -name '*.[it]cc' -o -name '*.H*' \\) -print`;
  my @cpp_files = split /^/m, $cpp_files_str;
  foreach (@cpp_files) {
      my $output = `sed -n "s|^\\(#include\\s\\+\\"\\)$old_inc|\\1$new_inc|p" $_`;
      if (length $output ne 0) {
        if ($dry_run) {
          print "  Would change file $_";
        }
        else {
          print "  Updating file $_";
          `sed -i.backup "s|^\\(#include\\s\\+\\"\\)$old_inc|\\1$new_inc|g" $_`;
        }
      }
  }
}

# Replace Library link lines
foreach my $old_lib (sort keys %library_list) {
  my $new_lib = $library_list{$old_lib};
  print "Checking for uses of $old_lib\n";
  my $cmake_files_str = `find $top_level_dir \\( \\( -name .svn -o -name .git -o -name CVS \\) -prune \\) -o \\( -name 'CMakeLists.txt' -o -name '*.cmake' -o -name 'SConscript' \\) -print`;
  my @cmake_files = split /^/m, $cmake_files_str;
  foreach (@cmake_files) {
      my $output = `sed -n "s|$old_lib|$new_lib|p" $_`;
      if (length $output ne 0) {
        if ($dry_run) {
          print "  Would change file $_";
        }
        else {
          print "  Updating file $_";
          `sed -i.backup "s|$old_lib|$new_lib|g" $_`;
        }
      }
  }
}
