set pagination off
set confirm off
set print thread-events off
handle SIGSTOP nostop noprint pass
set logging file /tmp/criss-gdb-flags2-onfindmotion-one.txt
set logging overwrite on
set logging enabled on
break 'motion::Player::onFindMotion(TJS::tTJSString, int)'
commands
  silent
  printf "this=%p flags=%d\n", $x0, (int)$x2
  p ((motion::Player*)$x0)->_motionKey
  p ((motion::Player*)$x0)->_chara
  p ((motion::Player*)$x0)->_parentPlayer
  p ((motion::Player*)$x0)->_runtime
  p ((motion::Player*)$x0)->_runtime->activeMotion
  bt 12
  detach
  quit
end
signal 0
