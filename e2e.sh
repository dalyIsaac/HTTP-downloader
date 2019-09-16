#!/bin/bash

# Used for testing ./downloader (and by extension, the source code of 
# downloader.c, http.c, and queue.c).

declare -a names=("small" "large")

for i in "${names[@]}"
do
    if [ -d "$i" ]
    then
        rm -Rf "$i"
    fi

    if [ -d "$i"-custom ]
    then
        rm -Rf "$i"-custom
    fi
done

for i in "${names[@]}"
do
    if [ -d download ]
    then 
        rm -Rf download
    fi

    echo "$i"
    mkdir "$i"
    cd "$i"
    wget -i ../"$i".txt
    cd ..
    ./downloader "$i".txt 12 download

    differences=$(diff -r download "$i")

    if ["$differences" == ""]
    then
        echo No differences for "$i"
        rm -r "$i"
    else
        echo Differences for "$i":
        echo "$differences"
        mv download "$i"-custom
    fi

    printf "\n\n\n\n"

done

rm -Rf download
