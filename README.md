# mruby-fast-remote-check   [![Build Status](https://travis-ci.org/matsumotory/mruby-fast-remote-check.svg?branch=master)](https://travis-ci.org/matsumotory/mruby-fast-remote-check)

FastRemoteCheck can perform port listening check at high speed using raw socket.

## install by mrbgems
- add conf.gem line to `build_config.rb`

```ruby
MRuby::Build.new do |conf|

    # ... (snip) ...

    conf.gem :github => 'matsumotory/mruby-fast-remote-check'
end
```
## example
```ruby
> f = FastRemoteCheck.new("127.0.0.1", 54321, "127.0.0.1", 6379, 3)
 => #<FastRemoteCheck:0x139d560>
> f.open_raw?
 => true
> f.connect?
 => true
> f = FastRemoteCheck.new("127.0.0.1", 54321, "127.0.0.1", 6378, 3)
 => #<FastRemoteCheck:0x139d2c0>
> f.open_raw?
 => false
> f.connect?
 => false
> f = FastRemoteCheck.new("127.0.0.1", 54321, "1.1.1.1", 6378, 3)
 => #<FastRemoteCheck:0x139cae0>
> f.open_raw?
(mirb):8: sys failed. errno: 11 message: Resource temporarily unavailable mrbgem message: recvfrom failed (RuntimeError)
> f.connect?
(mirb):9: sys failed. errno: 115 message: Operation now in progress mrbgem message: connect failed (RuntimeError)
>
```

## License
under the MIT License:
- see LICENSE file
