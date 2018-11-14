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

sub replace_in_files {
  my ($files_ref, $old_pattern, $new_pattern, $dry) = @_;
  foreach (@{$files_ref}) {
    my $output = `sed -n "s|$old_pattern|$new_pattern|p" $_`;
    if (length $output ne 0) {
      if ($dry) {
        print "  Would change file $_";
      }
      else {
        print "  Updating file $_";
        `sed -i.backup "s|$old_pattern|$new_pattern|g" $_`;
      }
    }
  }
}

# Replace C++ header dependencies
foreach my $old_inc (sort keys %header_list) {
  my $new_inc = $header_list{$old_inc};
  print "Checking for files using $old_inc\n";
  my $cpp_files_str = `find $top_level_dir \\( \\( -name .svn -o -name .git -o -name CVS \\) -prune \\) -o \\( -name '*.c' -o -name '*.cxx' -o -name '*.cc' -o -name '*.cpp' -o -name '*.C' -o -name '*.h' -o -name '*.hxx' -o -name '*.hh' -o -name '*.hpp' -o -name '*.[it]cc' -o -name '*.H*' \\) -print`;
  my @cpp_files = split /^/m, $cpp_files_str;
  # This pattern also removes trailing whitespace
  my $old_pattern = '^\\(#include\\s\\+\\"\\)' . $old_inc . '[^\\s]*$';
  my $new_pattern = '\\1' . $new_inc;
  replace_in_files(\@cpp_files, $old_pattern, $new_pattern, $dry_run);
}

# Replace Library link lines
foreach my $old_lib (sort keys %library_list) {
  my $new_lib = $library_list{$old_lib};
  print "Checking for uses of $old_lib\n";
  my $cmake_files_str = `find $top_level_dir \\( \\( -name .svn -o -name .git -o -name CVS \\) -prune \\) -o \\( -name 'CMakeLists.txt' -o -name '*.cmake' -o -name 'SConscript' \\) -print`;
  my @cmake_files = split /^/m, $cmake_files_str;
  replace_in_files(\@cmake_files, $old_lib, $new_lib, $dry_run);
}