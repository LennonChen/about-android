function myprint(...)
   local count = select("#", ...)
   for i = 1, count do
      if i > 1 then
         io.write("\t")
      end
      io.write(tostring(select(i, ...)))
   end
   io.write("\n")
end

line = io.read()

myprint(line)