##
## FastRemoteCheck Test
##

assert("FastRemoteCheck#open_raw? for listeing") do
  # check redis port
  t = FastRemoteCheck.new "127.0.0.1", 54321, "127.0.0.1", 6379, 3
  assert_true t.open_raw?
  assert_true t.ready?(:raw)
end

assert("FastRemoteCheck#open_raw? for not linsting") do
  # check redis port
  t = FastRemoteCheck.new "127.0.0.1", 54321, "127.0.0.1", 6380, 3
  assert_false t.open_raw?
  assert_false t.ready?(:raw)
end

assert("FastRemoteCheck#open_raw? for ip unreachable") do
  # check redis port
  timeout = 2
  t = FastRemoteCheck.new "127.0.0.1", 54321, "203.0.113.1", 6380, timeout
  before = Time.now
  assert_raise(RuntimeError) { t.open_raw? }
  after = Time.now
  assert_true (after - before) < (timeout + 1)
end

assert("FastRemoteCheck#connectable? for listeing") do
  # check redis port
  t = FastRemoteCheck.new "127.0.0.1", 54321, "127.0.0.1", 6379, 3
  assert_true t.connectable?
  assert_true t.ready?(:connect)
end

assert("FastRemoteCheck#connectable? for not linsting") do
  # check redis port
  t = FastRemoteCheck.new "127.0.0.1", 54321, "127.0.0.1", 6380, 3
  assert_false t.connectable?
  assert_false t.ready?(:connect)
end

assert("FastRemoteCheck#open_raw? for ip unreachable") do
  # check redis port
  timeout = 2
  t = FastRemoteCheck.new "127.0.0.1", 54321, "203.0.113.1", 6380, timeout
  before = Time.now
  assert_raise(RuntimeError) { t.connectable? }
  after = Time.now
  assert_true (after - before) < (timeout + 1)
end

assert("FastRemoteCheck#open_raw? for ip unreachable with timeout as msec") do
  # check redis port
  timeout = 2.358
  t = FastRemoteCheck.new "127.0.0.1", 54321, "203.0.113.1", 6380, timeout
  before = Time.now
  assert_raise(RuntimeError) { t.connectable? }
  after = Time.now
  assert_true (after - before) < (timeout + 1)
end

assert("FastRemoteCheck::ICMP#ping? for ip reachable") do
  t = FastRemoteCheck::ICMP.new "8.8.8.8", 3
  assert_true t.ping?
end

assert("FastRemoteCheck::ICMP#ping? for ip unreachable") do
  timeout = 2
  t = FastRemoteCheck::ICMP.new "203.0.113.1", timeout
  before = Time.now
  assert_raise(RuntimeError) { t.ping? }
  after = Time.now
  assert_true (after - before) < (timeout + 1)
end

assert("FastRemoteCheck::ICMP#ping? for ip unreachable with timeout as msec") do
  timeout = 2.358
  t = FastRemoteCheck::ICMP.new "203.0.113.1", timeout
  before = Time.now
  assert_raise(RuntimeError) { t.ping? }
  after = Time.now
  assert_true (after - before) < (timeout + 1)
end

assert("FastRemoteCheck::ICMP#ping? for wait packet reply") do
  t1 = Thread.new { FastRemoteCheck::ICMP.new("1.1.1.1", 1).ping? }
  t2 = Thread.new { FastRemoteCheck::ICMP.new("203.0.113.1", 5).ping? rescue "raised" }
  t3 = Thread.new { FastRemoteCheck::ICMP.new("8.8.8.8", 1).ping? }
  assert_true(t1.join)
  assert_equal("raised", t2.join)
  assert_true(t3.join)
end
