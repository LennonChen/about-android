do
   local count = 0
   function counter()
      count = count + 1
      return count
   end
end

print(counter())
print(counter())
print(counter())