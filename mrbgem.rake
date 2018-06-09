MRuby::Gem::Specification.new('mruby-fast-remote-check') do |spec|
  spec.license = 'MIT'
  spec.authors = 'MATSUMOTO Ryosuke'
  spec.add_test_dependency 'mruby-time', :core => 'mruby-time'
  spec.add_test_dependency 'mruby-thread'

  task :test do
    sh 'which setcap' # check setcap command
    sh "sudo setcap cap_net_raw+ep '#{build.build_dir}/bin/mrbtest'"
  end
end
