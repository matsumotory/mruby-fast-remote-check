##
## FastRemoteCheck Test
##

assert("FastRemoteCheck#hello") do
  t = FastRemoteCheck.new "hello"
  assert_equal("hello", t.hello)
end

assert("FastRemoteCheck#bye") do
  t = FastRemoteCheck.new "hello"
  assert_equal("hello bye", t.bye)
end

assert("FastRemoteCheck.hi") do
  assert_equal("hi!!", FastRemoteCheck.hi)
end
