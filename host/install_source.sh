#!/bin/bash

# 1. untar source code tarball
# 2. create git repository
# 3. apply patches, each patches applied as a commit

PDIR=`dirname "$1"`
[ -d "$PDIR" ] || mkdir -p "$PDIR"

run ()
{
	echo "$@"
	"$@" >/dev/null || exit 1
}


run tar xf "$2" -C "$PDIR"

run git init "$1"
run git --git-dir "$1/.git" --work-tree "$1" add --force .
run git --git-dir "$1/.git" --work-tree "$1" commit -m "add original source"

[ -d "$3" ] || exit 0

for f in "$3"/*.patch ; do
	[ ! -f "$f" ] ||  run git --git-dir "$1/.git" --work-tree "$1" am "$f"
done

