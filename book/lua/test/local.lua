-- Returns a function that tests whether a number is
-- less than N:
function makeLessThan(N)
   return function(X)
      return X < N
   end
end

lessThanFive = makeLessThan(5)
lessThanTen = makeLessThan(10)

print(lessThanFive(4))
print(lessThanTen(11))