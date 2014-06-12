uzip
====

unpack zip files (extract without decompress)

it lets you to get the files content the way they are stored. This is useful when the
zip manager can't let you get the files without the password.

this quick code can teach you how the zip format works internally

Examples
========

$ uzip zipfile.zip
Unpacking file1.txt
Unpacking file2.txt
....


here, the txt files are COMPRESSED so you can't read their content, unless if the
files were just archived (not compressed).
