##
## FastRemoteCheck Test
##

assert("FastRemoteCheck#port") do
  # check redis port
  t = FastRemoteCheck.new "127.0.0.1", 54321, "127.0.0.1", 6379, 3
  assert_true t.open_raw?
end

assert("FastRemoteCheck#port") do
  # check redis port
  t = FastRemoteCheck.new "127.0.0.1", 54321, "127.0.0.1", 6380, 3
  assert_false t.open_raw?
end

assert("FastRemoteCheck#port") do
  # check redis port
  timeout = 2
  t = FastRemoteCheck.new "127.0.0.1", 54321, "1.1.1.1", 6380, timeout
  before = Time.now
  assert_raise(RuntimeError) { t.open_raw? }
  after = Time.now
  assert_true (after - before) < (timeout + 1)
end
