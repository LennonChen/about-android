local count = select("#", ...)
if count > 0 then
   print("command-line arguments:")
   for i = 1, count do
      print(i, (select(i, ...)))
   end
else
   print("No command-line arguments given.")
end