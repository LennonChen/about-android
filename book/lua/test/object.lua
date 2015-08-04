-- Returns a table of two functions: a function that gets
-- N's value, and a function that increments N by its argument.
function MakeGetAndInc(N)

   local function Get()
      return N
   end

   local function Inc(M)
      N = N + M
   end

   return {Get = Get, Inc = Inc}
end

A = MakeGetAndInc(50)
--print(A.Get())
A.Inc(2)
--print(A.Get())

--------------------------------------------------


do
   local function get(obj)
      return obj.n
   end

   local function inc(obj, m)
      obj.n = obj.n + m
   end

   function makeGetAndInc(n)
      return {n = n, get = get, inc = inc}
   end
end

A = makeGetAndInc(50)
--print(A:get())
A:inc(2)
--print(A:get())

---------------------------------------------------

do
   local T = {}

   function T:get()
      return self.n
   end

   function T:inc(m)
      self.n = self.n + m
   end

   function makeGetAndInc(n)
      return {n = n, get = T.get, inc = T.inc}
   end
end

A = makeGetAndInc(50)
--print(A:get())
A:inc(2)
--print(A:get())