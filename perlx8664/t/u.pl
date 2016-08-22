#!/usr/bin/perl
use Getopt::Long;
use File::Basename;
use File::Path;
use FindBin qw($Bin);
use Cwd;
use Data::Dumper;
use Carp;
require "$Bin/../p.pm";

#$a = insn::addroffunc("$Bin/u.exe", "main");
#$i = { }; $f = {};
#insn::cont_at("$Bin/u.exe", $i, $f, $a);

$p = new insn({'insn'=>'lea    -0x8(%rsp),%rsp'});
$p = new insn({'insn'=>'xchg   %rbp,(%rsp)'});
$p = new insn({'insn'=>'mov    %rbp,(%rsp)'});
$p = new insn({'insn'=>'cmovne (%rsp),%rbp'});
$p = new insn({'insn'=>'jmpq   *-0x8(%rsp)'});
$p = new insn({'insn'=>'jmpq   f783c'});
$p = new insn({'insn'=>'retq'});
$p = new insn({'insn'=>'add    $0x288,%rsp'});
$p = new insn({'insn'=>'sub    $0x288,%rsp'});
$p = new insn({'insn'=>'lea    0x0(,%rcx,8),%r9'});
$p = new insn({'insn'=>'lea    0x0(%rdx,%rcx,8),%r9'});

    
