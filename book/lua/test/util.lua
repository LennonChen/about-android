Util = Util or {}

function Util.quote(str)
   return string.format("%q", str)
end

return Util