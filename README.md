# mruby-fast-remote-check   [![Build Status](https://travis-ci.org/matsumotory/mruby-fast-remote-check.svg?branch=master)](https://travis-ci.org/matsumotory/mruby-fast-remote-check)

FastRemoteCheck class

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
> FastRemoteCheck.new("127.0.0.1", 54321, "127.0.0.1", 6379, 3).open_raw?
 => true
> FastRemoteCheck.new("127.0.0.1", 54321, "127.0.0.1", 6378, 3).open_raw?
 => false
> FastRemoteCheck.new("127.0.0.1", 54321, "1.1.1.1", 6378, 3).open_raw?
(mirb):31: sys failed. errno: 11 message: Resource temporarily unavailable mrbgem message: recvfrom failed (RuntimeError)
>
```

## License
under the MIT License:
- see LICENSE file
