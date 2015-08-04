NameToInstr = {
   ["John"] = "rhythm guitar",
   ["Paul"] = "base guitar",
   ["George"] = "lead guitar",
   ["Ringo"] = "drumkit"
}

-- print(NameToInstr["Paul"])

potluck = {
   John = "chips",
   Jane = "lemonade",
   Jolene = "egg salad"
}

potluck.Jolene = "fruit salad"
potluck.Jayesh = "lettuce wraps"
potluck.John = nil
-- print(potluck.John, potluck.Jane, potluck.Jolene, potluck.Jayesh)

squares = {}

for i = 1, 5 do
   squares[i] = i ^ 2
end

for i = 1, 5 do
--   print(i .. " squares is " .. squares[i])
end

for i, square in ipairs(squares) do
--   print(i .. " squared is " .. square)
end

Months = {
   [1] = "January", [2] = "February", [3] = "March",
   [4] = "April", [5] = "May", [6] = "June", [7] = "July",
   [8] = "August", [9] = "September", [10] = "October",
   [11] = "November", [12] = "December"
}

-- print(Months[11])
-- print(#Months)

for number, word in pairs({"one", "two", nil, "four"}) do
--   print(number, word)
end

for number, value in pairs(Months) do
--   print(number .. " is " .. value)
end

T = {Gleep = true, Glarg = false}
for Fuzzy, Wuzzy in pairs(T) do
   Fuzzy, Wuzzy = Fuzzy .. "ing", #tostring(Wuzzy)
   -- print(Fuzzy, Wuzzy)
end

-- A table that maps numbers to their English names:
numbers = {"one", "two", "three"}
-- A table that will contain functions:
prependNumber = {}
for num, numName in ipairs(numbers) do
   prependNumber[num] =
      function(str)
         return numName .. ": " .. str
      end
end

-- print(prependNumber[2]("is company"))
-- print(prependNumber[3]("is a crowd"))

table.sort(Months)
for i, val in ipairs(Months) do
   print(i, val)
end