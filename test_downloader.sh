#!/bin/bash

declare -a names=("small" "large")

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
        mv download "$i"-curl
    fi

    printf "\n\n\n\n"

done

rm -Rf download
