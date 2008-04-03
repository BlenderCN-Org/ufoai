#!/usr/bin/perl
#######################################
# Copyright (C) 2008 Werner Hoehrer
#
#This program is free software; you can redistribute it and/or
#modify it under the terms of the GNU General Public License
#as published by the Free Software Foundation; either version 2
#of the License, or (at your option) any later version.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
#See the GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program; if not, write to the Free Software
#Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#######################################

#######################################
# ufo2dot.pl
#	Scriot to parse (part) of the *.ufo files for UFO:Alien Invasion.
#	It could also partly be used for similar Quake-bases scripts
#	The main logic for parsing is in the parseUfoToken function.
#
#######################################
# Changelog
#	2008-04-03 Hoehrer
#		Added first working parseUfoToken function.
#		Multi-line support for text inside ""s
#		Hopefully fixed comment parsing - still needs some testing. Also added a repeating cleanup function.
#		First code to parse a technology and the reuirements inside it.
#		Requirements are parsed now.
#######################################
# TODO
#	* Parse research.ufo. See $techTestString for an example
#	* Write .dot file (graphviz). See $graphvizExample for an example.
#######################################

use strict;
use warnings;

my $filenames = [
	"research.ufo",
	"research_logic.ufo"
];

# Settings inside "tech" we'll just skip for now.
my $technologySkipSettings = [
	"description",		# { default "_monoknife_txt" }
	"pre_description",	# { default "_monoknife_pre_txt" }
	"mail",			# { from xxx to xxx ...}
	"mail_pre"		# { from xxx to xxx ...}
];

# Settings of "tech" with just one parameter that we want to parse.
my $technologySimpleSettings = [
	"type",		# weapon
	"up_chapter", 	# equipment
	"provides", 	# knifemono
	"time",		# 100
	"producetime"	# 100
];

# Settings of requirements that we want to parse
# The number tells us how many parameters they have.
my $technologyRequirementSettings = {
	"tech"		=> 1,
	"alienglobal"	=> 1,
	"item"		=> 2,
	"alien"		=> 2,
	"alien_dead"	=> 2
};

#######################################
# Writing (.dot syntax)
#######################################
sub printTechnologyStyle ($$) {
	print 'node [shape=box, color="#004e0099", style=filled, fontcolor=black];';
}

sub printTech ($) {
	my ($tech) = @_;
	# techID [label="Tech Name"];
	print $tech->{id}, '[label="',$tech->{name},'"];';
}

sub printTechGroup ($) {
	my ($tech) = @_;

	if ($tech->OR || $tech->AND) {
		if ($tech->OR && $tech->AND) {
			# subgraph techID_C { techID_OR -- techID; techID_AND -- techID_OR }
			print 'subgraph ', $tech->id, '_C { ', $tech->id, '_OR -- ', $tech->id, '; ', $tech->id, '_AND -- ', $tech->id, '_OR }';
		}

		if ($tech->OR && !$tech->AND) {
			# subgraph techID_C { techID_OR -- techID; }
			print 'subgraph ', $tech->id, '_C { ', $tech->id, '_OR -- ', $tech->id, ' }';
		}

		if (!$tech->OR && $tech->AND) {
			# subgraph techID_C { techID_AND -- techID; }
			print 'subgraph ', $tech->id, '_C { ', $tech->id, '_AND -- ', $tech->id, ' }';
		}
	}
}

#######################################
# Parsing (.ufo techtree syntax)
#######################################
sub parseUfoToken ($) {
	my ($d) = @_;
	my $text = $$d;

	my $token = '';

	# Clean the text up a bit
	my $modified = 1;
	while ($modified) {
		$modified = 0;

		$text =~ s/^\s*//;	# Skip whitespace

		# Skip single-line comment.
		if ($text =~ m/^\/\//) {
			$text =~ s/([^\n]*)([.\n?]*)/$2/;	# Skip everything until newline.
			$modified = 1;
		}

		$text =~ s/^\s*//;	# Skip whitespace

		# Skip multi-line comment.
		if ($text =~ m/^\/\*/) {
			$text =~ s/\/\*([^"]*?)\*\/([.\n?]*)/$2/; # Skip everything until (and including) closing "*/" characters.
			$modified = 1;
		}

	}

	$text =~ s/^\s*//;	# Skip whitespace

	# Garbage is gone at this point. Only the next token.

	# Parse (multi-line) text encloded by ""
	if ($text =~ m/^"/) {
		#print "\"\"\"\"\"\"\"\"\"\n";
		$text =~ s/"([^"]*?)"([.\n?]*)/$2/;
		$token = $1;
		$$d = $text;
		return $token;
	}

	$text =~ s/([^\s]*)([.\n?]*)/$2/;
	$token = $1;
	$$d = $text;
	return $token;
}



#~ require_AND
#~ {
	#~ tech rs_weapon_kerrblade
	#~ tech rs_alien_bloodspider_autopsy
	#~ tech rs_skill_close
	#~ tech rs_damage_normal
#~ }
sub parseTechRequirements ($$) {
	my ($d, $techID) = @_;
	my $text = $$d;

	my $reqs = [];

	my $token = parseUfoToken(\$text);

	if ($token ne '{') {
		die "Empty tech requirement in '", $techID, "' found.\n";
	}

	$token = parseUfoToken(\$text);
	while ($token ne '}') {
		my $req = {};
		if (exists($technologyRequirementSettings->{$token})) {
			$req->{'type'} = $token;
			$token = parseUfoToken(\$text);
			$req->{'value1'} = $token;

			if ($technologyRequirementSettings->{$token} > 1) {
				$token = parseUfoToken(\$text);
				$req->{'value2'} = $token;
			}

			push(@{$reqs}, $req);
			$token = parseUfoToken(\$text);
		} else {
			die "Unknown requirement type '", $token ,"' in tech '", $techID ,"'\n";
		}
	}

	$$d = $text;
	return $reqs;
}

sub parseSkip ($) {
	my ($d) = @_;
	my $text = $$d;

	#$text =~ s/\{([^\}]*?)\}([.\n?]*)/$2/;	# TODO how would this work?

	my $token = parseUfoToken(\$text);
	while ($token ne '}') {
		$token = parseUfoToken(\$text);
	}

	$$d = $text;
}

sub parseTech ($) {
	my ($d) = @_;
	my $text = $$d;

	my $tech = {};

	my $token = parseUfoToken(\$text);

	if ($token ne 'tech') {
		die "Expecting 'tech' keyword, got this instead: '", $token, "'\n";
	}

	$token = parseUfoToken(\$text);
	my $techID = $token;

	$token = parseUfoToken(\$text);

	if ($token ne '{') {
		die "Empty tech '", $techID, "' found.\n";
	}

	$token = parseUfoToken(\$text);
	while ($token ne '}') {
		foreach my $setting (@{$technologySkipSettings}) {
			if ($token =~ m/^$setting/) {
				parseSkip(\$text);
				$token = parseUfoToken(\$text);
				print "Skipped $setting - next: $token\n";
			}
		}

		foreach my $setting (@{$technologySimpleSettings}) {
			if ($token =~ m/^$setting/) {
				$tech->{$setting} = parseUfoToken(\$text);
				$token = parseUfoToken(\$text);
				print "Parsed settings $setting - next: $token\n";
			}
		}

		if ($token eq "require_AND") {
			$tech->{'AND'} = parseTechRequirements(\$text, $techID);
			$token = parseUfoToken(\$text);
			print "Parsed AND - next: $token\n";
		} elsif ($token eq "require_OR") {
			$tech->{'OR'} = parseTechRequirements(\$text, $techID);
			$token = parseUfoToken(\$text);
			print "Parsed OR - next: $token\n";
		}

		#print "End of cycle\n";
	}

	$$d = $text;
	return $tech;
}

sub parseUfoTree (%$) {
	my ($self, $filename) = @_;
	die "\nUsage: parseUfoTree(techtree-HASHREF filename)"
	unless ((@_                == 2      ) &&
		(ref($self)        eq 'HASH' ) );

	print "Read File '", $filename, "'....\n";
	open(SOURCE, "< $filename") or die "\nCOuldn not open file '$filename' for reading: $!\n";

	# TODO parse files
}

####################
# MAIN
####################

my $techTestString ='tech rs_weapon_monoknife
{
	type	weapon
	up_chapter	equipment // comment test
	description {	/* comment test2 */
		default "_monoknife_txt"	/* comment
test3 */
		dummydesc "dumytext spanning
over several lines"
	}
	pre_description {
		default "_monoknife_pre_txt"
	}
	mail_pre
	{
		from	"_mail_from_paul_navarre"
		to		"_mail_to_base_commander"
		subject	"_Monomolecular Blades"
		icon	icons/tech
	}

	mail
	{
		from	"_mail_from_paul_navarre"
		to		"_mail_to_base_commander"
		subject	"_Monomolecular Blades"
		icon	icons/tech
	}

	require_AND
	{
		tech rs_weapon_kerrblade
		tech rs_alien_bloodspider_autopsy
		tech rs_skill_close
		tech rs_damage_normal
	}
	provides	knifemono
	time	100
	producetime	100
}';

#my $token = 'fixme2';
#while ($token ne '' && $techTestString ne '') {
#	$token = parseUfoToken(\$techTestString);
#	print "token: '", $token, "'\n";
#	#print "text : '", $techTestString, "'\n";
#	#$token = '';
#}

my $tech = {};
$tech = parseTech(\$techTestString);
use Data::Dumper;
print Dumper($tech);

###################################################################
my $graphvizExample = '
graph G {
	edge [color="#0b75cf"];
	label = "Research Tree\nUFO: Alien Invasion";

	node [shape=box, color="#004e0099", style=filled, fontcolor=black];
		tech1 [label="Test1"];
		tech2 [label="Test2"];
		tech3 [label="Test3"];
		tech4 [label="Test4"];
		tech5 [label="Test5"];
		tech6 [label="Test6"];
		tech7 [label="Test7"];

	node [shape=ellipse, label="OR", color="#71a4cf99", style=filled, fontcolor=black];
		/* OR1; */
		/* OR2; */
		OR3;
		/* OR4; */
		/* OR5; */
		OR6;
		OR7;

	node [shape=ellipse, label="AND", color="#71cf9a99", style=filled, fontcolor=black];
		/* AND1; */
		/* AND2; */
		AND3;
		/* AND4; */
		/* AND5; */
		AND6;
		/* AND7; */

/*
	node [shape=record,label="<AND> AND|<OR> OR"];
		testing;
*/

	subgraph items {
	rank=source;
	node [shape=box, color="#f7e30099", style=filled, fontcolor=black];
		item1 [label="Collected\nArtifact 1"];
		item2 [label="Collected\nArtifact 2"];
		item3 [label="Collected\nArtifact 3"];
	}

	/* subgraph tech1_C { OR1 -- tech1; AND1 -- OR1 } None are used. */
	/* subgraph tech2_C { OR2 -- tech2; AND2 -- OR2 }	None are used. */
	subgraph tech3_C { OR3 -- tech3; AND3 -- OR3 }	/* Both are used */
	/* subgraph tech4_C { OR4 -- tech4; AND4 -- OR4 }	None are used. */
	/* subgraph tech5_C { OR5 -- tech5; AND5 -- OR5 }	None are used. */
	subgraph tech6_C { OR6 -- tech6; AND6 -- OR6 }
	subgraph tech7_C { OR7 -- tech7; }	/* Only OR is used */

	tech1 -- AND3;
	tech2 -- AND3;
	tech4 -- OR3;
	tech5 -- OR3;
	item1 -- OR3;

	tech4 -- OR7;
	tech1 -- OR7;

	item2 -- OR6
	item3 -- OR6
	tech5 -- OR6

	tech2 -- AND6

	fontsize=20;
}
';

#############
# EOF