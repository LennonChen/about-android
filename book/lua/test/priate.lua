-- Returns two functions: a function that gets N's value,
-- and a function that increments N by its argument.

function makeGetAndInc(n)

   local function get()
      return n
   end

   local function inc(m)
      n = n + m
   end

   return get, inc

end

getA, incA = makeGetAndInc(0)
getB, incB = makeGetAndInc(100)

print(getA())
print(getB())
incA(5)
print(getA())
incB(4)
print(getB())