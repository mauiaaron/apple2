    #!/system/bin/sh
    # WARNING : these $ variables need to be defined above and outside this bundled script
    set -x
    PACKAGE=org.deadc0de.apple2ix.basic
    export TMPDIR=/data/data/org.deadc0de.apple2ix.basic
    exec /data/local/Inst/bin/valgrind --gen-suppressions=all $GPU_VARIANT --num-callers=16 --error-limit=no -v --error-limit=no --default-suppressions=yes --suppressions=/data/local/Inst/lib/valgrind/default.supp --trace-children=yes --log-file=/sdcard/valgrind.log.%p --tool=memcheck --leak-check=full --show-reachable=yes $*
