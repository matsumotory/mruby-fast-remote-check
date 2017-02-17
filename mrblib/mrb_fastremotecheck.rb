class FastRemoteCheck
  def ready? type
    case type
    when :connect then
      connectable?
    when :raw then
      open_raw?
    else
      raise ArgumentError, "invalid type: #{type} supports :connect or :raw"
    end
  end
end
