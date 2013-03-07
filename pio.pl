#!/usr/bin/perl
#vincent v.li@f5.com 2013-02-25
#sort per process I/O by write bytes or with -r to sort by read bytes
#from pio /proc/* output

use strict;
use warnings;
use Getopt::Std;

my %options = ();
getopts("r", \%options);

my $srw = defined $options{r} ? 'read_bytes' : 'write_bytes';

my (%tasks, $current_pid, $current_cmd);

while ( <> ) {
	chomp;
	($current_pid, $current_cmd)  = ($1, $2), next if /^(\d+):\s*(.*)$/;
	my $s = $tasks{$current_pid} ||= { pid => $current_pid, cmd =>  $current_cmd } ;
	if (/^read_bytes:\s*(\d+)$/) {
		$s->{'read_bytes'} = $1;
	}
	if (/^write_bytes:\s*(\d+)$/) {
		$s->{'write_bytes'} = $1;
	}
}

printf("%10s %10s %10s %10s\n", qw(PID READ_BYTES WRITE_BYTES TASK));

foreach my $task ( sort { $b->{$srw} <=> $a->{$srw} } values %tasks ) {
	printf("%10d %10d %10d %10s\n", 
	$task->{'pid'}, $task->{'read_bytes'}, $task->{'write_bytes'}, $task->{'cmd'} );
}



