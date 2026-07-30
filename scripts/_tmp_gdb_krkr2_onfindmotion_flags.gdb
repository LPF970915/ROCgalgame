set pagination off
set confirm off
set print thread-events off
handle SIGSTOP nostop noprint pass
set logging file /tmp/criss-gdb-flags2-onfindmotion.txt
set logging overwrite on
set logging enabled on
set $hits = 0
break 'motion::Player::onFindMotion(TJS::tTJSString, int)'
commands
  silent
  set $hits = $hits + 1
  printf "hit=%d this=%p flags=%d\n", $hits, $x0, (int)$x2
  bt 4
  if $hits >= 20
    detach
    quit
  end
  continue
end
signal 0
