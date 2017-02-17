##
## FastRemoteCheck Test
##

assert("FastRemoteCheck#open_raw? for listeing") do
  # check redis port
  t = FastRemoteCheck.new "127.0.0.1", 54321, "127.0.0.1", 6379, 3
  assert_true t.open_raw?
end

assert("FastRemoteCheck#open_raw? for not linsting") do
  # check redis port
  t = FastRemoteCheck.new "127.0.0.1", 54321, "127.0.0.1", 6380, 3
  assert_false t.open_raw?
end

assert("FastRemoteCheck#oepn_raw? for ip unreachable") do
  # check redis port
  timeout = 2
  t = FastRemoteCheck.new "127.0.0.1", 54321, "1.1.1.1", 6380, timeout
  before = Time.now
  assert_raise(RuntimeError) { t.open_raw? }
  after = Time.now
  assert_true (after - before) < (timeout + 1)
end

assert("FastRemoteCheck#connect? for listeing") do
  # check redis port
  t = FastRemoteCheck.new "127.0.0.1", 54321, "127.0.0.1", 6379, 3
  assert_true t.connect?
end

assert("FastRemoteCheck#connect? for not linsting") do
  # check redis port
  t = FastRemoteCheck.new "127.0.0.1", 54321, "127.0.0.1", 6380, 3
  assert_false t.connect?
end

assert("FastRemoteCheck#oepn_raw? for ip unreachable") do
  # check redis port
  timeout = 2
  t = FastRemoteCheck.new "127.0.0.1", 54321, "1.1.1.1", 6380, timeout
  before = Time.now
  assert_raise(RuntimeError) { t.connect? }
  after = Time.now
  assert_true (after - before) < (timeout + 1)
end
