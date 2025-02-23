## hwo to build
```sh
meson setup builddir --native-file clang.ini
ninja -C builddir
```


```sh
meson setup build --buildtype=debug --native-file clang.ini
meson configure build --buildtype=debug
ninja -C build
```
