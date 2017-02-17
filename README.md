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
p FastRemoteCheck.hi
#=> "hi!!"
t = FastRemoteCheck.new "hello"
p t.hello
#=> "hello"
p t.bye
#=> "hello bye"
```

## License
under the MIT License:
- see LICENSE file
