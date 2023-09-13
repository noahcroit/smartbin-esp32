### face recognition for smartbin login
Use dlib and face_recognition library in python. follow this instruction https://gist.github.com/MikeTrizna/4964278bb6378de72ba4b195553a3954
Need to build dlib in no CUDA mode when you want to use only CPU. By set -DDLIB_USE_CUDA=0 in CMake process.
```
$ git clone https://github.com/davisking/dlib.git
$ cd dlib
$ mkdir build
$ cd build
$ cmake .. -DDLIB_USE_CUDA=0 -DUSE_AVX_INSTRUCTIONS=1
$ cmake --build .
$ cd ..
$ python setup.py install --set DLIB_USE_CUDA=0
```

To use face-login demo, run
```
python3 face-login-demo.py
```
