function sorted(tbl)
   local sorted = {}
   for key in pairs(tbl) do
      sorted[#sorted + 1] = key
   end
   table.sort(sorted)
   local i = 0
   return function()
      i = i + 1
      local key = sorted[i]
      return key, tbl[key]
   end
end

function main(infilename, outfilename)
   local lines = {}
   local iter = infilename and io.lines(infilename) or io.lines()

   for line in iter do
      lines[line] = true
   end

   local outhnd = outfilename and io.open(outfilename, "w") or io.stdout
   for line in sorted(lines) do
      outhnd:write(line, "\n")
   end
   outhnd:close()
end

main(...)