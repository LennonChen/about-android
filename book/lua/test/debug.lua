function B()
   print(debug.traceback("B"))
end

function A()
   print(debug.traceback("A1"))
   B()
   print(debug.traceback("A2"))
end

A()

----------------------------------------

function C()
   print(1 + nil)
end

function D()
   C()
end

function E()
   D()
end

E()