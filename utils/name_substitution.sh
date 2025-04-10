#!/bin/bash
set -e

ACTION="$1"
SOURCE_DIR="$2"
BACKUP_TAR_PATH="$3"

REGEXES=(
    # stuff we don't want to replace
    's/(\w+) Root Program/\1_unreplace Root Program/g'
    's/(\w+) Web( S|s)tore/\1_unreplace Web Store/g'
    's/(\w+) Remote Desktop/\1_unreplace Remote Desktop/g'
    's/("BEGIN_LINK_CHROMIUM")(.*?Chromium)(.*?<ph name="END_LINK_CHROMIUM")/\1\2_unreplace\3/g'

    # main replacement(s)
    's/(?:Google )?Chrom(e|ium)(?!\w)/Helium/g'

    # post-replacement cleanup
    's/((?:Google )?Chrom(e|ium))_unreplace/\1/g'
    's/_unreplace//g'
)

REGEX_CHAIN=$(IFS=';'; echo "${REGEXES[*]}")

sanity_check() {
    if ! [ -f OWNERS ]; then
        echo "wrong src directory" >&2
        exit 1
    fi
}

do_sub() {
    if [ "$BACKUP_TAR_PATH" != "" ] && [ -f "$BACKUP_TAR_PATH" ]; then
        echo "$(basename "$BACKUP_TAR_PATH") exists -- name substitution has already been done." >&2
        echo "if you are really sure you wanna do this, delete it" >&2
        exit 1
    elif [ "$BACKUP_TAR_PATH" != "" ]; then
        BACKUP_TAR_PATH=$(greadlink -f "$BACKUP_TAR_PATH")
    fi

    case $OSTYPE in
        darwin*) SED_CMD=gsed;;
        *) SED_CMD=sed;;
    esac

    cd "$SOURCE_DIR" && sanity_check

    FILE_LIST="$(find . -type f -not -path '*/.*' -not -path '*/out/*' -name '*.grd*')"

    if [ "$BACKUP_TAR_PATH" != "" ]; then
        echo "$FILE_LIST" \
        | xargs tar cvf "$BACKUP_TAR_PATH" >/dev/null 2>/dev/null
    fi

    while IFS= read -r file_path; do
        if [[ $file_path == ./out/* ]] || [[ $file_path == *.new ]]; then
            continue;
        fi

        cp "$file_path" "$file_path.new"
        perl -pi -e "$REGEX_CHAIN" "$file_path.new"

        diff -Naur "$file_path" "$file_path.new" || true
        mv "$file_path.new" "$file_path"
    done <<< "$FILE_LIST"
}

do_unsub() {
    if [ "$BACKUP_TAR_PATH" = "" ] && ! [ -f "$BACKUP_TAR_PATH" ]; then
        echo "BACKUP_TAR_PATH missing or file does not exist" >&2
        exit 1
    fi

    BACKUP_TAR_PATH=$(greadlink -f "$BACKUP_TAR_PATH")
    cd "$SOURCE_DIR" && sanity_check
    tar xvf "$BACKUP_TAR_PATH"
    rm "$BACKUP_TAR_PATH"
}

do_str() {
    perl -pe "$REGEX_CHAIN" <<< "$1"
}

case $ACTION in
    sub) do_sub;;
    unsub) do_unsub;;
    str) do_str "$2";;
    *) echo "usage: $0 <sub | unsub> source_dir [backup_tarball_path]" >&2
       exit 1
esac
