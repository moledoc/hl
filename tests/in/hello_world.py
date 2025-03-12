print("hello world")

# NOTE: it's an english sentance

"""
NOTE: multi-line
comment
"""

class Date(object):
    
    def __init__(self, day=0, month=0, year=0):
        self.day = day
        self.month = month
        self.year = year

    @classmethod
    def from_string(cls, date_as_string):
        day, month, year = map(int, date_as_string.split('-'))
        date1 = cls(day, month, year)
        return date1

    def to_string(self):
        return "{}-{}-{}".format(self.year, self.month, self.day)

    @staticmethod
    def is_date_valid(date_as_string):
        day, month, year = map(int, date_as_string.split('-'))
        return day <= 31 and month <= 12 and year <= 3999

date2 = Date.from_string('11-09-2012')
is_date = Date.is_date_valid('11-09-2012')
print(date2.to_string(), is_date)

print("unclosed"

print(f"{date2.to_string()}")
print("{[(a)]} {(]") # NOTE: try double-click highlighting on brackets
# # NOTE: try double-click highlighting on brackets {[(a)]} {(]
# print("{[(a)]} {(]") # NOTE: try double-click highlighting on brackets

# make sure brackets are not messed up after unclosed ones in commenst
a = [1,2,3,4]
a[0] = 2
b = {"test": 1}
b["test"] = 2
print(f"{date2.to_string()} [something] (something)")

# this shouldn't be colored to string color
