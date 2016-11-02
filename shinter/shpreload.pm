package shpreload;

use File::Basename;
use File::Path;
use FindBin qw($Bin);
use lib "$Bin/../lib";
use Cwd;
use Cwd 'abs_path';
use POSIX;
use Data::Dumper;
use Time::HiRes;

sub rep {
    my ($v) = @_;
    $v =~ s/\n/\\n/g;
    $v =~ s/"/\\"/g;
    return $v;
}

sub execve_ {
    my ($fn,$a,$e) = @_;
    my $real = "";
    my $dir = getcwd;
    my $cwd = abs_path($dir);
    my $prefix = strftime "%H-%M-%S", localtime;
    my $get = sprintf("%d",int(Time::HiRes::time()*1000));
    my $m = "#!/bin/bash -e\ncd $cwd\n"; 
    
    foreach my $k (keys(%$e)) {
	my $v = $$e{$k};
	$v = rep($v);
	$m .= "export $k=\"$v\"\n";
    }
    shift @$a;
    my @a = map { 
	$_ =~ s/"/\\"/g;
	$_ = "\"".$_."\"";
    } @$a;
    my $cmd = join(" ",($fn, @a));
    $m .= $cmd."\n";
    
    if (open(my $fh, '>>', '/tmp/report.txt')) {
	print $fh $m;
	close $fh;
    }
}

sub execv {
    my ($fn,$a) = @_;
    local $| = 1;
    #print(" Inside execv($fn): ".Dumper($a)."\n");
    execve_($fn,$a,\%ENV);
}

sub execve {
    my ($fn,$a,$e) = @_; my %he = ();
    local $| = 1;
    #print(" Inside execve($fn): ".Dumper($a)."\n".Dumper($e)."\n");
    foreach my $_e (@$e) {
	$_e =~ /^([^=]+)=(.*)$/;
	$he{$1} = $2;
    }
    execve_($fn,$a,\%he);
}


# ###################  edit #######################
# # name of the original command, absolute path preferred
# my $real = "ls";

# ################### maybe edit ##################
# # directory to save the restart scripts
# my $base = "./";
# # prefix tot the restart scripts
# my $fn = "dump_";

# ################### dont edit below #############
# my $dir = getcwd;
# my $cwd = abs_path($dir);
# my $prefix = strftime "%H-%M-%S", localtime;
# my $get = sprintf("%d",int(Time::HiRes::time()*1000));
# $fn = "$prefix.$get.".basename(${0}).".$fn";
# sub  trim { my $s = shift; $s =~ s/^\s+|\s+$//g; return $s };
# sub writefile {
#     my ($out,$re,$temp) = @_;
#     my $dir = dirname($out);
#     if ($dir) {
#         mkpath($dir);
#     }
#     open OUT, ">$out" or die ($out.$!);
#     print OUT ($re);
#     close OUT;
#     chmod 0777, $out;
# }

# my $base = abs_path($base)."/";
# my @_a = @ARGV;
# my @a = (); @f = ();
# if (defined($ENV{'SH_INTERPOSER_FILTER'})) {
#     @f = slit(",",$ENV{'SH_INTERPOSER_FILTER'});
#     foreach my $a (@_a) {
# 	my $found = 0;
# 	foreach my $f (@f) {
# 	    $found = 1 if ($a eq $f);
# 	}
# 	push(@a,$a) if (!$found);
#     }
# } else {
#     @a = @_a;
# }

# @a = map { 
#     $_ =~ s/"/\\"/g;
#     $_ = "\"".$_."\"";
# } @a;

# if ($0 =~ /gcc/ || $0 =~ /g\+\+/) {
#   for (my $i = 0; $i < scalar(@ARGV)-1; $i++) {
#     my $a = $ARGV[$i];
#     if (trim($a) eq '-o') {
#       $fn = $ARGV[$i+1]; last;
#     }
#   } 
# } elsif ($0 =~ /ld/) {
#   for (my $i = 0; $i < scalar(@ARGV)-1; $i++) {
#     my $a = $ARGV[$i+1];
#     if (trim($a) eq '-o') {
#       $fn = $ARGV[$i+1]; last;
#     }
#   }
# } elsif ($0 =~ /make/) {
#   if (defined($ENV{'SH_INTERPOSER_DIR'})) {
#     $fn = $ENV{'SH_INTERPOSER_DIR'}."/".$fn;
#   }
#   $ENV{'SH_INTERPOSER_DIR'} = $cwd;
# }


# #print Dumper([$0, @ARGV]);
# #print Dumper(\%ENV);
# my $cmd = join(" ",($real, @a));

# ######################################

# my $m = ""; 
# foreach my $k (keys(%ENV)) {
#     my $v = $ENV{$k};
#     $v = rep($v);
#     $m .= "export $k=\"$v\"\n";
# } 
# writefile("$base${fn}_env.sh",$m);

# ######################################
# my $m = "#!/bin/bash
# exec env -i bash $base${fn}_cmd.sh
# ";
# writefile("$base${fn}_start.sh",$m); 

# ######################################
# my $m = "#!/bin/bash
# source $base${fn}_env.sh
# cd $cwd
# ${cmd}
# ";
# writefile("$base${fn}_cmd.sh",$m); 

# if (defined($ENV{'SH_INTERPOSER_LOG'})) {
#   my $f = $ENV{'SH_INTERPOSER_LOG'};
#   open OUTF, ">>$f" or die ($f.$!);
#   print OUTF ("$base${fn}_start.sh : $prefix : $cmd\n");
#   close OUTF;
# }


1;
