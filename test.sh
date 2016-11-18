curl http://bee.org/dist/public.key | sudo apt-key add -
echo "deb http://bee.org/dist/master/ubuntu/ `lsb_release -c -s` main" | sudo tee -a /etc/apt/sources.list.d/bee.list
sudo apt-get update > /dev/null
sudo apt-get -q -y install bee bee-dev

phpize && ./configure
make
make install
sudo pip install PyYAML
/usr/bin/python test-run.py --flags
