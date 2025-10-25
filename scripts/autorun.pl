#!/usr/bin/env perl
use strict;
use warnings;
use Getopt::Long;

# Simple option parsing (keep existing script compatible)
my $help = 0;
GetOptions('help|h' => \$help) or usage();
usage() if $help;

# Declare a package-level array that will hold the characters of the first arg
our @FIRST_ARG_CHARS;

# Consume the first positional argument (if provided) and split it into characters
my $first_arg = shift @ARGV;
if (defined $first_arg) {
    @FIRST_ARG_CHARS = split //, $first_arg;
    # Debug line: prints the split characters. Remove or silence when not needed.
    my $c = $FIRST_ARG_CHARS[0] // '';
    my $d = $FIRST_ARG_CHARS[1] // '';
    my $e = $FIRST_ARG_CHARS[2] // '';

    if ($c eq 'a') {
        if ($d eq 'q') {
            if ($e eq 'f') {
                my $cmd_a = system('make');
                my $cmd_b = system('qemu-system-i386', '-fda', 'build/valkyrie_os.img');
                if ($cmd_a == 0) {
                    if ($cmd_b == 0) {
                        exit(0);
                    }
                }
            } elsif ($e eq 'h') {
                my $cmd_a = system('make');
                my $cmd_b = system('qemu-system-i386', '-hda', 'build/valkyrie_os.raw');
                if ($cmd_a == 0) {
                    if ($cmd_b == 0) {
                        exit(0);
                    }
                }
            } else {
                print "Invalid arguments!\n";
                usage();
            }
        } elsif ($d eq 'b') {
            if ($e eq 'f') {
                my $cmd_a = system('make');
                my $cmd_b = system('bochs', '-f', 'scripts/bochs_fda.txt');
                if ($cmd_a == 0) {
                    if ($cmd_b == 0) {
                        exit(0);
                    }
                }
            } elsif ($e eq 'h') {
                my $cmd_a = system('make');
                my $cmd_b = system('bochs', '-f', 'scripts/bochs_hda.txt');
                if ($cmd_a == 0) {
                    if ($cmd_b == 0) {
                        exit(0);
                    }
                }
            } else {
                print "Invalid arguments!\n";
                usage();
            }
        } elsif ($d eq 'g') {
            if ($e eq 'f') {
                my $cmd_a = system('make');
                my $cmd_b = system('qemu-system-i386', '-fda', 'build/valkyrie_os.img', '-S', '-s');
                if ($cmd_a == 0) {
                    if ($cmd_b == 0) {
                        exit(0);
                    }
                }
            } elsif ($e eq 'h') {
                my $cmd_a = system('make');
                my $cmd_b = system('qemu-system-i386', '-fda', 'build/valkyrie_os.raw', '-S', '-s');
                if ($cmd_a == 0) {
                    if ($cmd_b == 0) {
                        exit(0);
                    }
                }
            } else {
                print "Invalid arguments!\n";
                usage();
            }
        } else {
            print "Invalid arguments!\n";
            usage();
        }
    } elsif ($c eq 'b') {
        if ($d eq '-') {
            if ($e eq 'f') {
                my $cmd_a = system('make', 'floppy_image');
                if ($cmd_a == 0) {
                    exit(0);
                }
            } elsif ($e eq 'h') {
                my $cmd_a = system('make', 'disk_image');
                if ($cmd_a == 0) {
                    exit(0);
                }
            } else {
                print "Invalid arguments!\n";
                usage();
            }
        } else {
            print "Invalid arguments!\n";
            usage();
        }
    } elsif ($c eq 'r') {
        if ($d eq 'q') {
            if ($e eq 'f') {
                my $cmd_b = system('qemu-system-i386', '-fda', 'build/valkyrie_os.img');
                if ($cmd_b == 0) {
                    exit(0);
                }
            } elsif ($e eq 'h') {
                my $cmd_b = system('qemu-system-i386', '-hda', 'build/valkyrie_os.raw');
                if ($cmd_b == 0) {
                    exit(0);
                }
            } else {
                print "Invalid arguments!\n";
                usage();
            }
        } elsif ($d eq 'b') {
            if ($e eq 'f') {
                my $cmd_b = system('bochs', '-f', 'scripts/bochs_fda.txt');
                if ($cmd_b == 0) {
                    exit(0);
                }
            } elsif ($e eq 'h') {
                my $cmd_b = system('bochs', '-f', 'scripts/bochs_hda.txt');
                if ($cmd_b == 0) {
                    exit(0);
                }
            } else {
                print "Invalid arguments!\n";
                usage();
            }
        } elsif ($d eq 'g') {
            if ($e eq 'f') {
                my $cmd_b = system('qemu-system-i386', '-fda', 'build/valkyrie_os.img', '-S', '-s');
                if ($cmd_b == 0) {
                    exit(0);
                }
            } elsif ($e eq 'h') {
                my $cmd_b = system('qemu-system-i386', '-fda', 'build/valkyrie_os.raw', '-S', '-s');
                if ($cmd_b == 0) {
                    exit(0);
                }
            } else {
                print "Invalid arguments!\n";
                usage();
            }
        } else {
            print "Invalid arguments!\n";
            usage();
        }
    } else {
        print "Invalid arguments!\n";
        usage();
    }
} else {
    @FIRST_ARG_CHARS = ();
    warn "Not enough arguments!\n";
    usage();
}

sub usage {
    print "Usage: $0 [--help] <argument>\n";
    exit 1;
}

1;

