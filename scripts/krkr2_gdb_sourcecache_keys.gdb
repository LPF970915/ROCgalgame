set pagination off
set confirm off
set breakpoint pending on
set $hits = 0

break motion::SourceCache::loadRenderSourceTextureByName(TJS::tTJSString const&, TJS::tTJSVariant const&, int, std::array<unsigned int, 4ul> const&)
commands
  silent
  set $variant_string = *(void **)$x1
  set $length = *(int *)((char *)$variant_string + 60)
  set $long_string = *(void **)((char *)$variant_string + 8)
  if $length > 21
    set $characters = (unsigned short *)$long_string
  else
    set $characters = (unsigned short *)((char *)$variant_string + 16)
  end
  printf "source[%d] len=%d blend=%ld colors=%08x,%08x,%08x,%08x key=", $hits, $length, $x3, *(unsigned int *)$x4, *(unsigned int *)($x4 + 4), *(unsigned int *)($x4 + 8), *(unsigned int *)($x4 + 12)
  set $index = 0
  while $index < $length
    printf "%c", $characters[$index]
    set $index = $index + 1
  end
  printf "\n"
  set $hits = $hits + 1
  if $hits >= 30
    detach
    quit
  end
  continue
end

continue
