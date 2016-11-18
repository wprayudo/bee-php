dnl config.m4 for extension bee
PHP_ARG_WITH(bee, for bee support,
[  --with-bee	Enable bee support])

if test "$PHP_BEE" != "no"; then
    PHP_NEW_EXTENSION(bee,    \
        src/bee.c             \
        src/bee_network.c     \
        src/bee_msgpack.c     \
        src/bee_schema.c      \
        src/bee_proto.c       \
        src/bee_tp.c          \
        src/third_party/msgpuck.c   \
        src/third_party/sha1.c      \
        src/third_party/base64_tp.c \
        src/third_party/PMurHash.c  \
        , $ext_shared)
    PHP_ADD_BUILD_DIR([$ext_builddir/src/])
    PHP_ADD_BUILD_DIR([$ext_builddir/src/third_party])
fi
