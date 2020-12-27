#!/bin/sh

export LANG=C

# tries to find all XML rieman configs and convert them to new dotconf format

prefix=$(rieman -vv |grep prefix | awk '{ print $3 }' | sed 's/"//g')

locations="$XDG_CONFIG_HOME/rieman \
          $HOME/.config/rieman \
          $prefix/share/rieman
          $XDG_DATA_HOME/rieman/skins \
          $HOME/.local/share/rieman/skins \
          $prefix/usr/local/share/rieman/skins"

srcs=

for d in $(printf "%s\n" $locations | sort -u)
do
    if [ -e $d ]; then
        srcs="$srcs $(find $d -type f -name '*xml')"
    fi
done

echo
echo "Found rieman XML files:"
echo

count=0
for s in $srcs
do
    echo $s
    count=$(($count + 1))
done

if [ $count = 0 ]; then
    echo "No rieman XML files found (prefix='$prefix' HOME='$HOME' XDG_CONFIG_HOME='$XDG_CONFIG_HOME'), exiting"
    exit 0
fi

echo
read -p "Type YES to proceed: " agree

if [ "x$agree" != "xYES" ]; then
    echo "Bye, nothing to do"
    exit
fi

for s in $srcs
do
    tgt=$(echo $s | sed 's/xml/conf/g')
    printf "%s" "Converting '$s' to '$tgt'..."
    ./scripts/xml2conf.sh $s > $tgt
    if [ $? -ne 0 ]; then
        echo "error"
        echo "please run ./scripts/xml2conf.sh '$s' manually and check what's going on"
        echo
    else
        echo "ok"
    fi
done

echo
echo "If everything works fine, you can now remove no longer needed XML files:"
echo

for s in $srcs
do
    echo $s
done

