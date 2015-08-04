local function quote(str)
   assert(type(str) == "string", "str is not a string")
   return string.format("%q", str)
end

-- print(quote("test"))

----------------------------------------------------------

function answer(question)
   local res
   if question == "no bananas" then
      res = "yes"
   elseif question == "everything" then
      res = 42
   elseif question == "tuesday" then
      res = "belgium"
   else
      error("No answer for " .. tostring(question), 1)
   end
end

-- print(answer("this statement is false"))

local function func(a, b, c)
   a, b, c = tonumber(a), tonumber(b), tonumber(c)
   print(a and b and c and a + b + c or "three numbers expected")
end

--func(1, "2", 3)
--func(1, "test", 3)

local function func1(a, b, c)
   local sum, errStr
   a, b, c = tonumber(a), tonumber(b), tonumber(c)
   if a and b and c then
      sum = a + b + c
   else
      errStr = "three numbers exptected"
   end
   return sum, errStr
end

local sum, errStr = func1(1, "2", 3)
--print(sum or errStr)
local sum, errStr = func1(1, nil, 3)
--print(sum or errStr)

-----------------------------------------------------------------

local function fileCopyLineNum(srcFileStr, dstFileStr)
   local srcHnd, dstHnd, errStr, line
   srcHnd, errStr = io.open(srcFileStr, "r")
   if srcHnd then
      dstHnd, errStr = io.open(dstFileStr, "w")
      if dstHnd then
         line = 0
         for str in srcHnd:lines() do
            line = line + 1
            dstHnd:write(line, " ", str, "\n")
         end
         if line == 0 then
            errStr = srcFileStr .. ": File is empty"
            line = nil
         end
         dstHnd:close()
      end
      srcHnd:close()
   end
   return line, errStr
end

local count, errStr = fileCopyLineNum("index.html", "index.lst")
io.write(count and ("OK: count " .. count) or errStr, "\n")