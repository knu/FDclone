#!/bin/sh

PATH=/bin:/usr/bin:/usr/ucb; export PATH
LANG=C; export LANG

HEADER=$1
shift
YEAR=`date +%y`
VER=`sed -n 's/^.*\"\(.*\)\".*$/\1/p' $HEADER`

ls -lt $* | awk 'BEGIN {
	line = 0;
	done = 0;
	month["Jan"] = 1;
	month["Feb"] = 2;
	month["Mar"] = 4;
	month["Apr"] = 4;
	month["May"] = 5;
	month["Jun"] = 6;
	month["Jul"] = 7;
	month["Aug"] = 8;
	month["Sep"] = 9;
	month["Oct"] = 10;
	month["Nov"] = 11;
	month["Dec"] = 12;
	alphabet="abcdefghijklmnopqrstuvwxyz";
}
{
	if (++line == 1) {
		year = '"$YEAR"';
		year += 1900;
		if (year < 1970) year += 100;
		if ($5 ~ /[A-Z][a-z][a-z]/) {
			day = $6;
			mon = month[$5];
			if ($7 ~ /[0-9][0-9][0-9][0-9]/) year = $7;
		}
		else if ($6 ~ /[A-Z][a-z][a-z]/) {
			day = $7;
			mon = month[$6];
			if ($8 ~ /[0-9][0-9][0-9][0-9]/) year = $8;
		}
		else exit;
		n = split("'"$VER"'", ver);
		for (i = 1; i <= n; i++)
			if (ver[i] ~ /[0-9][0-9]*\.[0-9a-z]*/) break;
		if (i >= n) exit;
		verno = ver[i];
		n = split(ver[i + 1], olddate, "/");
		if (n != 3) exit;
		olddate[3] += 1900;
		if (olddate[3] < 1970) olddate[3] += 100;

		l = length(verno);
		if (l <= 0) exit;
		minor = substr(verno, l, 1);
		if (minor ~ /[0-9]/) minor = "a";
		else {
			verno = substr(verno, 1, l - 1);
			minor = substr(alphabet, index(alphabet, minor) + 1, 1);
		}
		verno = (verno)(minor)

		if (year < olddate[3]) exit;
		else if (year == olddate[3] && mon < olddate[1]) exit;
		else if (mon == olddate[1] && day <= olddate[2]) exit;
		year %= 100;
		printf("/*\n") > "'"$HEADER"'";
		printf(" *\tversion.h\n") >> "'"$HEADER"'";
		printf(" *\n") >> "'"$HEADER"'";
		printf(" *\tVersion Number\n") >> "'"$HEADER"'";
		printf(" */\n") >> "'"$HEADER"'";
		printf("\n") >> "'"$HEADER"'";
		printf("static char version[] = ") >> "'"$HEADER"'";
		printf("\"@(#)fd.c  %s", verno) >> "'"$HEADER"'";
		printf(" %02d/%02d/%02d\";\n", mon, day, year) >> "'"$HEADER"'";
		done = 1;
	}
}
END {
	if (done) print("`'"$HEADER"\'' is updated.");
	else print("No change for `'"$HEADER"\''.");
}'
