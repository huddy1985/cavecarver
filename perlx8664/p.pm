#!/usr/bin/perl
package modrm;
sub new {
    my ($c,$g,$n,$s_) = @_;
    bless $s,$c;
}

package modrm::imm;
@ISA = ('modrm');
sub new {  my ($c,$a,$val) = @_; my $s = {'all'=>$a,'val'=>$val};  bless $s,$c; return $s; }

package modrm::mem;
@ISA = ('modrm');
sub new {  my ($c,$a,$reg,$val) = @_; $val=0 if (!defined($val)); my $s = {'all'=>$a,'reg'=>$reg,'val'=>$val};  bless $s,$c; return $s; }

package modrm::rip;
@ISA = ('modrm');
sub new {  my ($c,$a,$reg,$val) = @_; my $s = {'all'=>$a,'reg'=>$reg,'val'=>$val};  bless $s,$c; return $s; }

package modrm::memstripe;
@ISA = ('modrm');
sub new {  my ($c,$a,$base,$idx,$stripe,$val) = @_; $val=0 if (!defined($val)); my $s = {'all'=>$a,'base'=>$base,'idx'=>$idx,'stripe'=>$stripe,'val'=>$val};  bless $s,$c; return $s; }

package modrm::reg;
@ISA = ('modrm');
sub new {  my ($c,$a,$reg) = @_; my $s = {'all'=>$a,'reg'=>$reg};  bless $s,$c; return $s; }

##define RM_AX 1    /* AL, AH, AX, EAX, RAX */
#define RM_CX 2     /* CL, CH, CX, ECX, RCX */
#define RM_DX 4     /* DL, DH, DX, EDX, RDX */
#define RM_BX 8     /* BL, BH, BX, EBX, RBX */
#define RM_SP 0x10  /* SPL, SP, ESP, RSP */
#define RM_BP 0x20  /* BPL, BP, EBP, RBP */
#define RM_SI 0x40  /* SIL, SI, ESI, RSI */
#define RM_DI 0x80  /* DIL, DI, EDI, RDI */
#define RM_FPU 0x100 /* ST(0) - ST(7) */
#define RM_MMX 0x200 /* MM0 - MM7 */
#define RM_SSE 0x400 /* XMM0 - XMM15 */
#define RM_AVX 0x800 /* YMM0 - YMM15 */
#define RM_CR 0x1000 /* CR0, CR2, CR3, CR4, CR8 */
#define RM_DR 0x2000 /* DR0, DR1, DR2, DR3, DR6, DR7 */
#define RM_R8 0x4000 /* R8B, R8W, R8D, R8 */
#define RM_R9 0x8000 /* R9B, R9W, R9D, R9 */
#define RM_R10 0x10000 /* R10B, R10W, R10D, R10 */
#define RM_R11 0x20000 /* R11B, R11W, R11D, R11 */
#define RM_R12 0x40000 /* R12B, R12W, R12D, R12 */
#define RM_R13 0x80000 /* R13B, R13W, R13D, R13 */
#define RM_R14 0x100000 /* R14B, R14W, R14D, R14 */
#define RM_R15 0x200000 /* R15B, R15W, R15D, R15 */

package modrm;


package insn::sub;
@ISA = ('insn');
package insn::add;
@ISA = ('insn');
package insn::mov;
@ISA = ('insn');
package insn::cmov;
@ISA = ('insn');
package insn::lea;
@ISA = ('insn');
package insn::jc;
@ISA = ('insn');
package insn::push;
@ISA = ('insn');
package insn::xchg;
@ISA = ('insn');
package insn::jmpq;
@ISA = ('insn');
sub exitblock { return 1; }
package insn::retq;
@ISA = ('insn');
sub exitblock { return 1; }

package insn;
sub exitblock { return 0; }

use Carp;
use Data::Dumper;

sub new {
    my ($c,$insn) = @_;
    my $s = insn::decode($insn);
    return $s;
}

sub decode {
    my ($in) = @_;
    my $insn = $$in{'insn'};
    my $i = {}; my $c = 'insn';
    my @parts = grep { ! /addr32/ } split(/\s+/,$insn);
    my $pre = $parts[0];
    if ($pre =~ /^lea.*/)
    {
	my ($src,$dst) = getops($insn,$parts[1]);
	($i,$c) = ({'src'=>$src,'dst'=>$dst},'insn::lea');
    }
    elsif ($pre =~ /^xchg$/)
    {
	my ($src,$dst) = getops($insn,$parts[1]);
	($i,$c) = ({'src'=>$src,'dst'=>$dst},'insn::xchg');
    }
    elsif ($pre =~ /^mov/)
    {
	my ($src,$dst) = getops($insn,$parts[1]);
	($i,$c) = ({'src'=>$src,'dst'=>$dst},'insn::mov');
    }
    elsif ($pre =~ /^cmov.*/)
    {
	my ($src,$dst) = getops($insn,$parts[1]);
	($i,$c) = ({'src'=>$src,'dst'=>$dst},'insn::cmov');
    }
    elsif ($pre =~ /^push.*/)
    {
	my ($src) = getops($insn,$parts[1]);
	($i,$c) = ({'src'=>$src},'insn::push');
    }
    elsif ($pre =~ /^(?:addr32\s*)?jmpq/ || $pre =~ /^jmp/) # *
    {
	if ($insn =~ /^(?:addr32\s*)?jmpq?\s+([0-9a-f]+)/) { # jmpq f456
	    my ($dst) = getops($insn,"\$0x".$1);
	    ($i,$c) = ({'dst'=>$dst},'insn::jmpq');
	} elsif ($insn =~ /^(?:addr32\s*)?jmpq\s*\*(.*)/) { # jmpq *-8(%rsp)
	    my ($dst) = getops($insn,$1);
	    ($i,$c) = ({'dst'=>$dst},'insn::jmpq');
	} else {
	}
    }
    elsif ($pre =~ /^(?:addr32\s*)?j.*\s+([0-9a-f]+)/) # *
    {
	my ($dst) = getops($insn,"\$0x".$1);
	($i,$c) = ({'dst'=>$dst},'insn::jc');
    }
    elsif ($pre =~ /^retq/)
    {
	($i,$c) = ({},'insn::retq');
    }
    elsif ($pre =~ /^sub/)
    {
	my ($src,$dst) = getops($insn,$parts[1]);
	($i,$c) = ({'src'=>$src,'dst'=>$dst},'insn::sub');
    }
    elsif ($pre =~ /^add/)
    {
	my ($src,$dst) = getops($insn,$parts[1]);
	($i,$c) = ({'src'=>$src,'dst'=>$dst},'insn::add');
    }
    foreach my $k (keys(%$i)) {
	$$in{$k} = $$i{$k};
    }
    bless $in,$c;
    return $in;
}

sub convhex {
    my ($off) = @_; 
    return -hex(substr($off,1)) if ($off =~ /^-/); 
    return hex($off);
}

sub getops {
    my ($insn,$op) = @_;
    my @a = (); my $all = $op; my $n;
    while (length($op)) {
	if ($op =~ /^\%(?:[def]s):/) {
	    $op = $';
	}
	if ($op =~ /^\$0x([a-f0-9]+)/) {
	    # imm hex
	    my ($val,$all) = (hex($1),$&); $op = $';
	    push(@a, new modrm::imm($all,$val));
	}
	elsif ($op =~ /^\$([a-f0-9]+)/) {
	    # imm
	    my ($val,$all) = (int($1),$&);  $op = $';
	    push(@a, new modrm::imm($all,$val));
	}
	elsif ($op =~ /^((?:-?0x[a-f0-9]+)?)\(\%(.{1,4})\)/) {
	    # register indiret, optional offset
	    my ($val,$reg,$all) = (convhex($1),$2,$&);  $op = $';
	    if ($reg eq 'rip') {
		$insn =~ /# ([a-f0-9]+)/ or die ("cannot parse pc rel dest addr in '$insn'");
		push(@a, new modrm::rip($all,$reg, hex($1)));
	    } else {
		push(@a, new modrm::mem($all,$reg, $val));
	    }
	}
	elsif ($op =~ /^((?:-?0x[a-f0-9]+)?)\((?:\%([a-z0-9]{1,4}))?,\%([a-z0-9]{1,4}),([0-9a-f])\)/) {
	    # register indiret, optional sspice
	    my ($val,$base,$idx,$stripe,$all) = (convhex($1),$2,$3,$4,$&);  $op = $';
	    push(@a, $n = new modrm::memstripe($all,$base,$idx,$stripe,$val));
	}
	elsif ($op =~ /^\%([a-z0-9]{1,4})/) {
	    # reg
	    my ($reg,$all) = ($1,$&);  $op = $';
	    push(@a, new modrm::reg($all,$reg));
	}
	elsif ($op =~ /^0x([a-f0-9]+)/) {
	    # imm hex
	    my ($val,$all) = (hex($1),$&); $op = $';
	    push(@a, new modrm::imm($all,$val));
	}
	else  {
	    croak("Cannot decode: '$op' of '$insn'\n");
	}
	if ($op =~ /^,/) {
	    $op = $';
	} elsif ($op =~ /^\s*#(.*)$/) {
	    $op = $';
	}
    }
    return @a;
}

sub scan_ar {
    my ($a,$f,$m) = (@_);
    my $last = undef; my @i = (); my $j = 0;
	
    foreach $m (@$m) {
	my $found = 0;
	#print("Process $m\n");
	if ($m =~ /^\s*$/ms) {
	} elsif ($m =~ /file format elf64/) {
	} elsif ($m =~ /\.\.\.$/) {
	} elsif ($m =~ /\(bad\)/) {
	} elsif ($m =~ /^Disassembly of section (.*)/) {
	} elsif ($m =~ /^0*([0-9a-f]+) \<(.*)\>:$/s) {
	    my ($addr,$fn) = ($1,$2);
	    $f{$fn} = $addr;
	} elsif ($m =~ /^\s*([0-9a-f]+)\:\s*((?:[0-9a-f]{2} )+)\s*$/) {
	    my ($addr,$bytes) = ($1,$2);
	    croak("Last address mismatch\n") if (!defined($$a{$last}));
	    $$a{$last}{'data'} = $$a{$last}{'data'}.$bytes;
	} elsif ($m =~ /^\s*([0-9a-f]+)\:\s*((?:[0-9a-f]{2} )+)\s*/) {
	    my ($addr,$bytes,$insn) = (hex($1),$2,$');
	    $$a{$addr} = decode({'addr'=>$addr, 'data'=>$bytes, 'insn'=>$insn}) ;;
	    $found = 1; $last = $addr; push(@i, $last);
	} else {
	    print ("Cannot decode: '$m' $j\n");
	    exit(1);
	}
	$last = undef if (!$found);
	$j++;
    }
    foreach my $k (keys(%$a)) {
	my $len = (length($$a{$k}{'data'})/3);
	$$a{$k}{'len'} = $len;
    }
    return \@i;
}

sub cont_at {
    my ($fname,$a,$f,$addr) = @_;
    my $cmd;
    if (!defined($$a{$addr})) {
	my $e = $addr + 0x100;
	$cmd = sprintf("objdump -C -d %s --start-address=0x%x --stop-address=0x%x 2> /dev/null",$fname,$addr,$e);
	my @ar = `$cmd`;
	@ar = map { $_ =~ s/\n$//; $_ } @ar;
	$i = scan_ar($a,$f,\@ar);
	delete($$a{pop(@$i)}); # remove last
    }
    if (!defined($$a{$addr})) {
	#print (Dumper($a));
	croak(sprintf("Error: cannot resolve '0x%x' using '$cmd'\n", $addr, $cmd));
	exit(1);
    }
    return $$a{$addr};
}

sub addroffunc {
    my ($fname, $func) = @_;
    my $cmd = sprintf("nm -C -D %s | grep 'T %s' | awk '{print \$1;}'", $fname, $func);
    my $val = hex(`$cmd`);
    croak("'$cmd' failed to retrive addr of $func") if (!$val);
    return $val;
}

1;
