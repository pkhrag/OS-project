# NachOS
If you find this error during `make`:
```
/usr/bin/ld.bfd.real: cannot find -lstdc++
collect2: error: ld returned 1 exit status
make: *** [nachos] Error 1
```
use the following command:

`sudo ln -s /usr/lib32/libstdc++.so.6 /usr/lib32/libstdc++.so`
