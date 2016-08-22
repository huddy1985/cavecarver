#!/usr/bin/perl
use Getopt::Long;
use File::Basename;
use File::Path;
use FindBin qw($Bin);
use Cwd;
use Data::Dumper;
use Carp;
require "$Bin/p.pm";

sub usage { print("usage: $0 --start=<func> <infile>"); exit(1); };
Getopt::Long::Configure(qw(bundling));
GetOptions(\%OPT,qw{
    quiet|q+
    verbose|v+
    start|s=s
} ,@g_more) or usage(\*STDERR);

sub verbose {
    my ($p) = @_;
    if ($OPT{'verbose'}) {
	print(STDERR $p);
    }
}

my $verbose_len = 92;
sub verbose_insn {
    my ($i, $interpret) = @_;
    my $insn = $$i{'insn'};
    $insn = substr($insn, 0, $verbose_len-3)."..." if (length(substr($insn,0,$verbose_len)) < length($insn));
    while (length($insn) < $verbose_len) { $insn = $insn." "; };
    verbose(sprintf("%08x:",$$i{'addr'}).$insn.$interpret."\n");
}    

$fname = $ARGV[0];
$start = insn::addroffunc($fname,$OPT{'start'});
verbose(sprintf("Start function at 0x%x\n",$start));

@a = ($start); %done={}; $funcs={};
%stack = (); %regs = ('rsp' => 0x7fffffff10000000 );

while(scalar(@a)) {
    #next block
    my ($a,$cont,$block) = (shift(@a),1,{});
    my $ip = $a;
    while($cont) {
	last if (exists($done{$ip}));
	$done{$ip} = 1;
	my $i = insn::cont_at($fname,$block,$funcs,$ip);
	$nip = $ip + $$i{'len'};
	my $interpret = "";
	my $suppress=0;

	#print (Dumper($i));
	
	if (ref($i) eq 'insn::jmpq' &&
	    (ref($$i{dst}) eq 'modrm::imm' || 
	     ref($$i{dst}) eq 'modrm::mem')) {
	    if (ref($$i{dst}) eq 'modrm::imm') {
		$interpret = sprintf("jmpq => -----------------------    (0x%x)", $$i{dst}{'val'}); 
		unshift(@a,$$i{dst}{'val'});
	    } else { # modrm::mem
		if ($$i{dst}{'reg'} eq 'rsp') {
		    my $sa = $regs{'rsp'}+$$i{dst}{'val'};
		    my $sv = $stack{$sa};
		    if (ref($sv) eq 'ARRAY') { # when from cmov 
			$interpret = sprintf("jmpq (0x%x)=> -----------------------   (0x%x,0x%x)", $sa, $$sv[0], $$sv[1]); 
			unshift(@a,$$sv[0]);
			push(@a,$$sv[1]);
		    } else {
			unshift(@a,$sv);
			$interpret = sprintf("jmpq (0x%x)=> -----------------------  (0x%x)", $sa, $sv); 
		    }
		}
	    }
	    $suppress = 1;
	    $cont = 0;
	} elsif (ref($i) eq 'insn::jc') {
	    $interpret = sprintf("jcond +> (0x%x)", $$i{dst}{'val'}); 
	    unshift(@a,$$i{dst}{'val'});
	} elsif (ref($i) eq 'insn::retq') {
	    my $sa = $regs{'rsp'};
	    my $sv = $stack{$sa};
	    if (ref($sv) eq 'ARRAY') { # when from cmov 
		$interpret = sprintf("retq -----------------------  sp:0x%x=> (0x%x,0x%x)", $sa, $$sv[0], $$sv[1]); 
		push(@a,$$sv[0]);
		push(@a,$$sv[1]);
	    } else {
		push(@a,$sv);
		$interpret = sprintf("retq => -----------------------   (0x%x)", $sv); 
	    }
	    
	} elsif (ref($i) eq 'insn::sub' &&
		 ref($$i{dst}) eq 'modrm::reg' &&
		 ref($$i{src}) eq 'modrm::imm') {
	    $interpret = sprintf("%s <= (0x%x-%d)", $$i{dst}{'reg'}, $regs{$$i{dst}{'reg'}}, $$i{src}{'val'}); 
	    $regs{$$i{dst}{'reg'}} -= $$i{src}{'val'};
	} elsif (ref($i) eq 'insn::add' &&
		 ref($$i{dst}) eq 'modrm::reg' &&
		 ref($$i{src}) eq 'modrm::imm') {
	    $interpret = sprintf("%s <= (0x%x+%d)", $$i{dst}{'reg'}, $regs{$$i{dst}{'reg'}}, $$i{src}{'val'}); 
	    $regs{$$i{dst}{'reg'}} += $$i{src}{'val'};
	} elsif (ref($i) eq 'insn::push' &&
		 ref($$i{src}) eq 'modrm::reg') {
	    $interpret = sprintf("stack[0x%x-8] <= (0x%x)", $regs{'rsp'}, $regs{$$i{src}{'reg'}});
	    $regs{'rsp'} -= 8;
	    $stack{$regs{'rsp'}} = $regs{$$i{src}{'reg'}};
	} elsif (ref($i) eq 'insn::lea' &&
		 ref($$i{dst}) eq 'modrm::reg' &&
		 (ref($$i{src}) eq 'modrm::rip' || ref($$i{src}) eq 'modrm::mem')) {
	    if (ref($$i{src}) eq 'modrm::rip') {
		$interpret = sprintf("%s <= (0x%x)", $$i{dst}{'reg'}, $$i{src}{'val'}); 
		$regs{$$i{dst}{'reg'}} = $$i{src}{'val'};
	    } else { #modrm::mem
		$interpret = sprintf("%s <= %s(0x%x+%d)", $$i{dst}{'reg'}, $$i{src}{'reg'}, $regs{$$i{src}{'reg'}}, $$i{src}{'val'}); 
		$regs{$$i{dst}{'reg'}} = $regs{$$i{src}{'reg'}} + $$i{src}{'val'};
	    }
	} elsif (ref($i) eq 'insn::xchg' &&
		 ref($$i{src}) eq 'modrm::reg' &&
		 ref($$i{dst}) eq 'modrm::mem') {
	    if ($$i{dst}{'reg'} eq 'rsp') {
		my $val = $stack{$regs{'rsp'}};
		$interpret = sprintf("stack[0x%x] <= %s(0x%x)", $regs{'rsp'}, $$i{src}{'reg'}, Dumper($regs{$$i{src}{'reg'}})); 
		$stack{$regs{'rsp'}} = $regs{$$i{src}{'reg'}};
		$regs{$$i{src}{'reg'}} = $val;
	    }
	} elsif (ref($i) eq 'insn::mov' &&
		 ref($$i{src}) eq 'modrm::reg' &&
		 ref($$i{dst}) eq 'modrm::mem') {
	    if ($$i{dst}{'reg'} eq 'rsp') {
		$interpret = sprintf("stack[0x%x+%d] <= %s(0x%x)", $regs{'rsp'}, $$i{dst}{'val'}, $$i{src}{'reg'}, $regs{$$i{src}{'reg'}}); 
		$stack{$regs{'rsp'}+$$i{dst}{'val'}} = $regs{$$i{src}{'reg'}};
	    }
	} elsif (ref($i) eq 'insn::cmov') {
	    if ($$i{src}{'reg'} eq 'rsp') {
		$interpret = sprintf("%s => %s(%s)", $$i{dst}{'reg'}, Dumper($regs{$$i{dst}{'reg'}})); 
		$regs{$$i{dst}{'reg'}} = [$regs{$$i{dst}{'reg'}},$stack{$regs{'rsp'}}]; # save conditionally overwritten jump address
	    }
	} 
	
	verbose_insn($i, $interpret) if (!$suppress);
	last if ($i->exitblock);
	$ip = $nip;
    }
}
