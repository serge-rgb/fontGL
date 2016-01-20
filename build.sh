# Comment to cleanup
  # -Wno-unused-variable \

clang++ font.cc -o font \
  -D_REENTRANT -g -O0 \
  -I/usr/include/SDL2 \
  -fno-omit-frame-pointer \
  -Wall \
  -Werror \
  -Wno-c++11-compat-deprecated-writable-strings \
  -Wno-unused-function \
  -Wno-unused-variable \
  -lGL -lSDL2 -lGLEW

