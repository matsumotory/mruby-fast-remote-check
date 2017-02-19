# mruby-fast-remote-check   [![Build Status](https://travis-ci.org/matsumotory/mruby-fast-remote-check.svg?branch=master)](https://travis-ci.org/matsumotory/mruby-fast-remote-check)

FastRemoteCheck can perform port listening check at high speed using raw socket.

## install by mrbgems
- add conf.gem line to `build_config.rb`

```ruby
MRuby::Build.new do |conf|

    # ... (snip) ...

    conf.gem :mgem => 'mruby-fast-remote-check'
end
```

## benchmark

`f.ready? :raw` used RAW socket via my user TCP stack using 3 packets. This is very fast.

`f.ready? :connect` used `connect(2)` and SO_LINGER `close(2)` via Kernel TCP stack using 4 packets.


#### 6379 port listeing

```
> f = FastRemoteCheck.new("127.0.0.1", 54321, "127.0.0.1", 6379, 3)
 => #<FastRemoteCheck:0x139f840>
> a = Time.now; 100000.times {f.ready? :connect}; (Time.now - a).to_f
 => 3.78796
> a = Time.now; 100000.times {f.ready? :raw}; (Time.now - a).to_f
 => 1.066795
```

#### 6380 port not listeing

```
> f = FastRemoteCheck.new("127.0.0.1", 54321, "127.0.0.1", 6380, 3)
 => #<FastRemoteCheck:0x139f4b0>
> a = Time.now; 100000.times {f.ready? :connect}; (Time.now - a).to_f
 => 1.037658
> a = Time.now; 100000.times {f.ready? :raw}; (Time.now - a).to_f
 => 0.620506
```

## example
```ruby
> f = FastRemoteCheck.new("127.0.0.1", 54321, "127.0.0.1", 6379, 3)
 => #<FastRemoteCheck:0x139d560>
> f.open_raw?
 => true
> f.ready?(:raw) # alias f.open_raw?
 => true
> f.connectable?
 => true
> f.ready?(:connect) # alias f.connectable?
 => true
> f = FastRemoteCheck.new("127.0.0.1", 54321, "127.0.0.1", 6378, 3)
 => #<FastRemoteCheck:0x139d2c0>
> f.open_raw?
 => false
> f.connectable?
 => false
> f = FastRemoteCheck.new("127.0.0.1", 54321, "1.1.1.1", 6378, 3)
 => #<FastRemoteCheck:0x139cae0>
> f.open_raw?
(mirb):8: sys failed. errno: 11 message: Resource temporarily unavailable mrbgem message: recvfrom failed (RuntimeError)
> f.connectable?
(mirb):9: sys failed. errno: 115 message: Operation now in progress mrbgem message: connect failed (RuntimeError)
>
```

## License
under the MIT License:
- see LICENSE file
