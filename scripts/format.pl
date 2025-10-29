#!/usr/bin/env perl
use strict;
use warnings;
use File::Find;
use File::Temp qw/ tempfile /;
use File::Spec::Functions qw(catfile);

my $USAGE = <<"USAGE";
usage: format.pl [--list] [--check] [--diff] [--apply] [--help]

Options:
  --list    : list files that would be formatted
  --check   : print files that would change and exit non-zero if any
  --diff    : show unified diff for files that would change
  --apply   : overwrite files in-place with clang-format output
  --help    : this message

By default the script looks for files with extensions: c, cc, cpp, cxx, h, hh, hpp, hxx
It runs `clang-format -style=file` for each matched file.
USAGE

my $LIST  = 0;
my $CHECK = 0;
my $DIFF  = 0;
my $APPLY = 0;

for my $arg (@ARGV) {
   if ($arg eq '--list') { $LIST = 1 }
   elsif ($arg eq '--check') { $CHECK = 1 }
   elsif ($arg eq '--diff') { $DIFF = 1 }
   elsif ($arg eq '--apply') { $APPLY = 1 }
   elsif ($arg eq '--help' or $arg eq '-h') { print $USAGE; exit 0 }
   else { warn "Unknown option: $arg\n"; print $USAGE; exit 2 }
}

my @ext = qw(c cc cpp cxx h hh hpp hxx);
my $re = join('|', @ext);
my @files;

find( sub {
   return unless -f $_;
   return unless /\.($re)$/i;
   push @files, $File::Find::name;
}, '.');

if ($LIST) {
   print "Found ", scalar(@files), " source/header files:\n";
   print map { "$_\n" } @files;
   exit 0;
}

unless (@files) {
   print "No C/C++ source files found.\n";
   exit 0;
}

my $clang = `which clang-format 2>/dev/null`;
chomp $clang;
unless ($clang) {
   die "clang-format not found in PATH. Install clang-format and retry.\n";
}

my $would_change = 0;
my @changed_files;

for my $f (@files) {
   # run clang-format and capture output
   my $formatted = `"$clang" -style=file "$f"`;
   if (!defined $formatted) {
      warn "clang-format failed on $f\n";
      next;
   }

   open my $in, '<', $f or do { warn "Can't open $f: $!\n"; next };
   local $/;
   my $orig = <$in>;
   close $in;

   if ($orig eq $formatted) {
      next; # no change
   }

   $would_change++;
   push @changed_files, $f;

   if ($CHECK) {
      print "$f would be reformatted\n";
      next;
   }

   if ($DIFF) {
      my ($tfh, $tmpname) = tempfile();
      print $tfh $formatted;
      close $tfh;
      system('diff', '-u', $f, $tmpname) == 0 or print ""; # diff printed to stdout
      unlink $tmpname;
   }

   if ($APPLY) {
      open my $out, '>', $f or do { warn "Can't write $f: $!\n"; next };
      print $out $formatted;
      close $out;
      print "Updated: $f\n";
   }
}

if ($would_change) {
   unless ($APPLY or $DIFF or $LIST) {
      print "Files that would be changed (use --apply to overwrite):\n";
      print map { "$_\n" } @changed_files;
   }
}

if ($CHECK) {
   exit($would_change ? 1 : 0);
}

exit 0;
