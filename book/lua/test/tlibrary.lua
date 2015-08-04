-- A demonstration of sorting an associative table.

nameToStr = {
   John = "rhythm guitar",
   Paul = "base guitar",
   George = "lead guitar",
   Ringo = "drumkit"
}

-- Tranfer the associative table nameToStr to the array sorted.

sorted = {}
for name, str in pairs(nameToStr) do
   table.insert(sorted, {name = name, str = str})
end

table.sort(sorted, function(A, B) return A.name < B.name end)

for _, val in ipairs(sorted) do
   print(val.name .. " played " .. val.str)
end