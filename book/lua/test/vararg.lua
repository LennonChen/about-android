function average(...)
   local ret, count = 0, 0

   for _, num in ipairs({...}) do
      ret = ret + num
      count = count + 1
   end

   assert(count > 0, "Attempted to average zero numbers")

   return ret / count
end

--print(average(1))

--print(average(41, 43))

--print(average(31, -41, 59, -26, 53))

function makePrinter(...)
   local args = {...}
   return function()
--      print(unpack(args))
   end
end

printer = makePrinter("a", "b", "c")
printer()

--print(select(1, "a", "b", "c"))
--print(select(2, "a", "b", "c"))
--print(select(3, "a", "b", "c"))
--print(select("#", "a", "b", "c"))

function makePrinter(...)
   local args = {...}
   local count = select("#", ...)
   return function()
      print(unpack(args, 1, count))
   end
end

printer = makePrinter(nil, "b", nil, nil)
printer()