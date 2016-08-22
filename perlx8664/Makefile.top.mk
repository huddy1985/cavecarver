all:
	perl p.pl --verbose --start='isl::iostreams::filter::detail::parse_hybrid_header(isl::iostreams::filter::detail::xp_block&)' /opt/Xilinx-2015.4-newinstall/Vivado/2015.4/lib/lnx64.o/libisl_iostreams.so 

c:
	perl p.pl --verbose --start='HACGConstraintGenerator::execute' /opt/Xilinx-2015.4-newinstall/Vivado/2015.4/lib/lnx64.o/librdi_coregen.so 
