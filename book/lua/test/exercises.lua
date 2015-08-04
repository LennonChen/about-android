function typedToString(val)
   return (type(val) .. ": " ..  val)
end

print(typedToString("abc"))
print(typedToString(42))
-- print(typedToString(true))
-- print(typedToString(function() end))