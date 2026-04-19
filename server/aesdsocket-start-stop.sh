#!/bin/bash

case "$1" in
  start)
    echo "Starting aesdsocket"
    # start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
    start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket
    ;;
  stop)
    echo "Stopping aesdsocket"
    start-stop-daemon -K -n aesdsocket -s SIGTERM
    ;;
  *)
    echo "Usage: $0 {start|stop}"
    exit 1
esac
exit 0