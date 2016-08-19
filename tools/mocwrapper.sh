ARGS=
for i
do
    if [ "$1" == "-isystem" ]; then
        ARGS="$ARGS -I"
    else
        ARGS="$ARGS $i"
    fi
    shift
done
moc $ARGS