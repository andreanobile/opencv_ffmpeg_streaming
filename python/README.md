# python wrapper
building the python wrapper requires boost-numpy. On Ubuntu 18 the package can be installed with:
```
$ sudo apt-get install libboost-numpy-dev
```

from this directory, to build the python library in Release mode:
```
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make
```

test_stream.py is an example on how to use the wrapper.
