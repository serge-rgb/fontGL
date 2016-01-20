if [ ! -d libserg ]; then
  git clone https://github.com/serge-rgb/libserg
else
  cd libserg
  git pull
  cd ..
fi
