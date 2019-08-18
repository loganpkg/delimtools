#!/bin/sh

#
# Copyright (c) 2019 Logan Ryan McLintock
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

# possum -- stores pictures and movies based on their creation date.
# For my loving esposinha with her beautiful possum eyes.

set -e

if [ "$#" -ne 2 ]
then
	printf 'Usage: %s searchdir storedir\n' "$0" 1>&2
	exit 1
fi

searchdir="$1"
storedir="$2"
batch="$(date '+%s')"

# rename files
exiftool '-filename<CreateDate' -d '%Y_%m_%d_%H_%M_%S%%-c_'"$batch"'.%%ue' \
-r -ext jpg -ext mov -ext mp4 "$searchdir"

# move files
exiftool '-Directory<CreateDate' -d "$storedir"'/%Y/%m' \
-r -ext jpg -ext mov -ext mp4 "$searchdir"

# delete junk files
find "$searchdir" \
\( -name 'desktop.ini' -o -name '.nomedia' -o -iname '*.AAE' \) \
-type f -delete

# delete empty directories
find "$searchdir" -type d -empty -delete

# remove duplicates
fdupes -rdN "$storedir"
