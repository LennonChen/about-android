module(..., package.seeall)

function quote(str)
   return string.format("%q", str)
end