import csv

def confusables():
	with open('confusables.txt', encoding='utf-8-sig') as f:
		# Filter comments
		f = map(lambda line: line.split('#')[0], f)
		return list(csv.DictReader(f, fieldnames=['Value', 'Target', 'Category'], delimiter=';'))

UNICODEDATA_FIELDS = (
	"Value",
	"Name",
	"General_Category",
	"Canonical_Combining_Class",
	"Bidi_Class",
	"Decomposition_Type",
	"Decomposition_Mapping",
	"Numeric_Type",
	"Numeric_Mapping",
	"Bidi_Mirrored",
	"Unicode_1_Name",
	"ISO_Comment",
	"Simple_Uppercase_Mapping",
	"Simple_Lowercase_Mapping",
	"Simple_Titlecase_Mapping",
)

def data():
	with open('UnicodeData.txt', encoding='utf-8') as f:
		return list(csv.DictReader(f, fieldnames=UNICODEDATA_FIELDS, delimiter=';'))

def unhex(s):
	return int(s, 16)

def unhex_sequence(s):
	return [unhex(x) for x in s.split()] if '<' not in s else None
